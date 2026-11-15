cmake_minimum_required(VERSION 3.20)
project(aes_lab2 LANGUAGES CXX)

# =============================================================================
# Lab 2 - AES-128 in pure C++ (CTR mode).  Build:  mkdir build && cd build && cmake .. && cmake --build .
#
# Constraint of the lab: NO external cryptographic library in the AES core or in the submitted tool.
#   aes_pure      the whole implementation (AES core, CTR, XTS, AES-NI bonus, KAT runner, benchmark).
#                 Links NOTHING but the C++ runtime and the OS (BCryptGenRandom / getrandom).
#   aestool       the command-line tool built on aes_pure   (no OpenSSL, no Crypto++, no libsodium)
#   aestool_ref   the SAME tool + OpenSSL / Crypto++ reference code, used ONLY for cross-validation
#                 (`xval`) and the performance comparison (`bench --ref`). Built only when one of those
#                 libraries is installed; skipping it never affects the lab's own code.
# =============================================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Single-config generators (Ninja, Makefiles): default to Release. Stored in the cache so that imported
# libraries (vcpkg debug/release pairs) resolve to the SAME configuration as this project.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# =============================================================================
# Platform & compiler detection (informational)
# =============================================================================
if(WIN32)
    set(PLATFORM_NAME "windows")
else()
    set(PLATFORM_NAME "ubuntu")
endif()
if(MSVC)
    set(COMPILER_NAME "msvc")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(COMPILER_NAME "clang")
else()
    set(COMPILER_NAME "gcc")
endif()
message(STATUS "Platform: ${PLATFORM_NAME}, Compiler: ${COMPILER_NAME}")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../cmake")

option(LAB2_STATIC_RUNTIME "Link the C++ runtime (and reference libraries) statically: release binaries need no extra DLL/.so" OFF)
option(LAB2_BUILD_REFERENCE "Also build aestool_ref (OpenSSL / Crypto++ cross-validation + benchmark comparison) when available" ON)
option(LAB2_WERROR "Treat warnings as errors (developer option; OFF so other compilers still build)" OFF)
option(BUILD_TESTS "Build unit tests, KAT runner, and CLI tests" ON)

find_package(Threads REQUIRED)

# =============================================================================
# Compiler warnings
# =============================================================================
if(MSVC)
    add_compile_options(/W4 /permissive- /utf-8)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
    if(LAB2_WERROR)
        add_compile_options(/WX)
    endif()
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
    if(LAB2_WERROR)
        add_compile_options(-Werror)
    endif()
endif()

# =============================================================================
# Reference libraries (optional, cross-validation + comparison only)
# =============================================================================
set(LAB2_HAVE_OPENSSL FALSE)
set(LAB2_HAVE_CRYPTOPP FALSE)
if(LAB2_BUILD_REFERENCE)
    # MSVC and Linux ignore PATH-derived prefixes, so an MSYS2/MinGW library on the Windows PATH
    # (also appended automatically under WSL) is never picked up by mistake.
    if(MSVC OR (UNIX AND NOT WIN32))
        set(CMAKE_FIND_USE_SYSTEM_ENVIRONMENT_PATH OFF)
    endif()
    # Static reference libraries only on Windows. On Linux, distro static libcrypto.a drags in extra static
    # dependencies (jitterentropy, zstd, ...), so aestool_ref stays dynamic there (only `aestool` is fully static).
    if(LAB2_STATIC_RUNTIME AND WIN32)
        set(OPENSSL_USE_STATIC_LIBS ON)
        set(CRYPTOPP_PREFER_STATIC ON)
    endif()
    find_package(OpenSSL QUIET)
    find_package(CryptoPP QUIET)
    unset(CMAKE_FIND_USE_SYSTEM_ENVIRONMENT_PATH)
    if(OPENSSL_FOUND)
        set(LAB2_HAVE_OPENSSL TRUE)
    endif()
    if(CryptoPP_FOUND)
        set(LAB2_HAVE_CRYPTOPP TRUE)
    endif()
    message(STATUS "Reference libraries: OpenSSL=${LAB2_HAVE_OPENSSL} Crypto++=${LAB2_HAVE_CRYPTOPP}")
endif()

# =============================================================================
# Sources
# =============================================================================
set(LAB2_SOURCES
    src/aes128.cpp
    src/aesni.cpp
    src/xts.cpp
    src/ctr.cpp
    src/util.cpp
    src/json.cpp
    src/kat.cpp
    src/stats.cpp
    src/bench.cpp
    src/xval.cpp
)

# Build info printed by `aestool bench` (hardware/compiler table in the report)
string(TOUPPER "${CMAKE_BUILD_TYPE}" _bt)
set(AESP_FLAGS "${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_${_bt}}")
string(STRIP "${AESP_FLAGS}" AESP_FLAGS)

function(lab2_configure_library target)
    target_include_directories(${target} PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
        $<INSTALL_INTERFACE:include>
    )
    target_link_libraries(${target} PUBLIC Threads::Threads)
    if(WIN32)
        target_link_libraries(${target} PUBLIC bcrypt)   # OS CSPRNG (BCryptGenRandom)
    endif()
    target_compile_definitions(${target} PRIVATE
        AESP_BUILD_TYPE="${CMAKE_BUILD_TYPE}"
        AESP_BUILD_FLAGS="${AESP_FLAGS}"
    )
endfunction()

function(lab2_configure_executable target)
    if(WIN32)
        target_link_libraries(${target} PRIVATE shell32)   # CommandLineToArgvW (UTF-8 argv)
    endif()
    if(LAB2_STATIC_RUNTIME AND NOT MSVC AND (WIN32 OR NOT target STREQUAL "aestool_ref"))
        target_link_options(${target} PRIVATE -static)     # MSVC: configure with /MT (see README)
    endif()
endfunction()

# --- aes_pure + aestool : NO external cryptographic library ---------------------------------------
add_library(aes_pure STATIC ${LAB2_SOURCES})
lab2_configure_library(aes_pure)

add_executable(aestool src/main.cpp)
target_link_libraries(aestool PRIVATE aes_pure)
lab2_configure_executable(aestool)

# --- aestool_ref : same code + reference implementations (cross-validation / comparison only) ------
set(LAB2_REF_TARGET "")
if(LAB2_HAVE_OPENSSL OR LAB2_HAVE_CRYPTOPP)
    add_library(aes_pure_ref STATIC ${LAB2_SOURCES})
    lab2_configure_library(aes_pure_ref)
    if(LAB2_HAVE_OPENSSL)
        target_compile_definitions(aes_pure_ref PRIVATE AESP_HAVE_OPENSSL=1)
        target_link_libraries(aes_pure_ref PRIVATE OpenSSL::Crypto)
    endif()
    if(LAB2_HAVE_CRYPTOPP)
        target_compile_definitions(aes_pure_ref PRIVATE AESP_HAVE_CRYPTOPP=1)
        target_link_libraries(aes_pure_ref PRIVATE CryptoPP::CryptoPP)
    endif()
    add_executable(aestool_ref src/main.cpp)
    target_link_libraries(aestool_ref PRIVATE aes_pure_ref)
    lab2_configure_executable(aestool_ref)
    set(LAB2_REF_TARGET aestool_ref)
else()
    message(STATUS "No OpenSSL / Crypto++ found: aestool_ref (cross-validation, comparison) is not built. "
                   "Install libssl-dev and/or libcrypto++-dev, or pass -DCMAKE_PREFIX_PATH / -DOPENSSL_ROOT_DIR / -DCRYPTOPP_ROOT_DIR.")
endif()

# =============================================================================
# Tests
# =============================================================================
if(BUILD_TESTS)
    enable_testing()

    # Catch2 v3: use an installed one (apt install catch2 / vcpkg install catch2 / pacman -S mingw-w64-x86_64-catch);
    # otherwise download v3.5.3 at configure time (needs internet once).
    set(_catch_noenv "")
    if(MSVC OR (UNIX AND NOT WIN32))
        set(_catch_noenv NO_SYSTEM_ENVIRONMENT_PATH)
    endif()
    find_package(Catch2 3 QUIET ${_catch_noenv})
    if(NOT Catch2_FOUND)
        message(STATUS "Catch2 v3 not installed: fetching v3.5.3 (use -DBUILD_TESTS=OFF to skip the unit tests)")
        include(FetchContent)
        FetchContent_Declare(Catch2
            GIT_REPOSITORY https://github.com/catchorg/Catch2.git
            GIT_TAG v3.5.3
        )
        FetchContent_MakeAvailable(Catch2)
    endif()

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_aes.cpp")
        add_executable(aes_tests tests/test_aes.cpp)
        target_link_libraries(aes_tests PRIVATE aes_pure Catch2::Catch2WithMain)
        target_compile_definitions(aes_tests PRIVATE
            AESP_KAT_FILE="${CMAKE_CURRENT_SOURCE_DIR}/kat/vectors.json"
        )
        add_test(NAME lab2_unit_tests COMMAND aes_tests)
    endif()

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/kat/vectors.json")
        add_test(NAME lab2_kat_runner COMMAND aestool --kat ${CMAKE_CURRENT_SOURCE_DIR}/kat/vectors.json)
    endif()

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_e2e.cmake")
        add_test(NAME lab2_cli_end_to_end
            COMMAND ${CMAKE_COMMAND}
                    -DTOOL=$<TARGET_FILE:aestool>
                    -DWORK=${CMAKE_CURRENT_BINARY_DIR}/e2e_work
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_e2e.cmake
        )
    endif()

    if(LAB2_REF_TARGET)
        add_test(NAME lab2_cross_validation COMMAND aestool_ref xval --ref all --cases 30)
    endif()
endif()

# =============================================================================
# Installation (optional)
# =============================================================================
install(TARGETS aes_pure aestool ${LAB2_REF_TARGET}
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)
install(DIRECTORY src/ DESTINATION include FILES_MATCHING PATTERN "*.hpp")

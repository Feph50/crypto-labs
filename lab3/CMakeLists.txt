cmake_minimum_required(VERSION 3.20)
project(lab3_rsatool LANGUAGES CXX)

# =============================================================================
# Lab 3 - RSA-OAEP and hybrid (envelope) encryption with Crypto++.
#   Build:  mkdir build && cd build && cmake .. && cmake --build .
#
#   rsa_core   static library: RSA-OAEP (SHA-256, MGF1-SHA-256), key I/O (PEM/DER/PKCS#8/SPKI/metadata),
#              hybrid envelope (RSA-OAEP wrapped AES-256-GCM key), KAT runner
#   rsatool    command-line tool (keygen, encrypt, decrypt, --kat)
#   rsabench   benchmark harness (keygen, RSA-OAEP, AES-GCM, hybrid) -> CSV
#   rsa_tests  Catch2 unit tests (run through ctest)
# Crypto++ is the only cryptographic library. nlohmann_json parses the envelope header and the KAT file.
# =============================================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "In-source builds are not supported. Use: mkdir build && cd build && cmake ..")
endif()

# Single-config generators: default to Release (stored in the cache so imported vcpkg libraries use the same config).
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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

option(LAB3_STATIC_RUNTIME "Link the C++ runtime and Crypto++ statically (release binaries need no extra DLL/.so)" OFF)
option(LAB3_WERROR "Treat warnings as errors (developer option; OFF so other compilers still build)" OFF)
option(BUILD_TESTS "Build unit tests and CLI tests" ON)

# =============================================================================
# Dependencies
# =============================================================================
if(LAB3_STATIC_RUNTIME)
    set(CRYPTOPP_PREFER_STATIC ON)
endif()
find_package(CryptoPP REQUIRED)
find_package(Threads REQUIRED)

# nlohmann_json: installed package if present (apt install nlohmann-json3-dev / pacman -S mingw-w64-x86_64-nlohmann-json /
# vcpkg install nlohmann-json), otherwise downloaded once at configure time (needs internet).
# (MSVC and Linux ignore PATH-derived prefixes, so an MSYS2/MinGW copy on the Windows PATH - also appended under WSL -
#  is never picked up by mistake: its include directory would break the MSVC / Linux headers.)
set(_noenv "")
if(MSVC OR (UNIX AND NOT WIN32))
    set(_noenv NO_SYSTEM_ENVIRONMENT_PATH)
endif()
find_package(nlohmann_json 3 CONFIG QUIET ${_noenv})
if(NOT nlohmann_json_FOUND)
    message(STATUS "nlohmann_json not installed: fetching v3.11.3 (see README to install it instead)")
    include(FetchContent)
    FetchContent_Declare(nlohmann_json
        URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz)
    FetchContent_MakeAvailable(nlohmann_json)
endif()

# =============================================================================
# Compiler warnings
# =============================================================================
if(MSVC)
    add_compile_options(/W4 /permissive- /utf-8 /EHsc)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
    if(LAB3_WERROR)
        add_compile_options(/WX)
    endif()
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
    if(LAB3_WERROR)
        add_compile_options(-Werror)
    endif()
endif()

# =============================================================================
# Targets
# =============================================================================
string(TOUPPER "${CMAKE_BUILD_TYPE}" _bt)
set(RSA_FLAGS "${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_${_bt}}")
string(STRIP "${RSA_FLAGS}" RSA_FLAGS)

add_library(rsa_core STATIC
    src/util.cpp
    src/encoding.cpp
    src/keyio.cpp
    src/rsa_oaep.cpp
    src/envelope.cpp
    src/hybrid.cpp
    src/kat.cpp
)
target_include_directories(rsa_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(rsa_core PUBLIC CryptoPP::CryptoPP nlohmann_json::nlohmann_json Threads::Threads)
if(WIN32)
    target_link_libraries(rsa_core PUBLIC ws2_32 shell32)
endif()
target_compile_definitions(rsa_core PRIVATE
    RSALAB_BUILD_TYPE="${CMAKE_BUILD_TYPE}"
    RSALAB_BUILD_FLAGS="${RSA_FLAGS}")

function(lab3_configure_executable target)
    if(LAB3_STATIC_RUNTIME AND NOT MSVC)
        target_link_options(${target} PRIVATE -static)     # MSVC: configure with /MT (see README)
    endif()
endfunction()

add_executable(rsatool src/main.cpp src/cli.cpp)
target_link_libraries(rsatool PRIVATE rsa_core)
lab3_configure_executable(rsatool)

add_executable(rsabench bench/bench_main.cpp)
target_link_libraries(rsabench PRIVATE rsa_core)
target_compile_definitions(rsabench PRIVATE
    RSALAB_BUILD_TYPE="${CMAKE_BUILD_TYPE}"
    RSALAB_BUILD_FLAGS="${RSA_FLAGS}")
lab3_configure_executable(rsabench)

# =============================================================================
# Tests
# =============================================================================
if(BUILD_TESTS)
    enable_testing()

    # Catch2 v3: installed package, else v3.5.3 downloaded at configure time. MSVC and Linux ignore PATH-derived
    # prefixes, so an MSYS2/MinGW copy on the Windows PATH (also appended under WSL) is never picked up by mistake.
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
            GIT_TAG v3.5.3)
        FetchContent_MakeAvailable(Catch2)
    endif()

    add_executable(rsa_tests
        tests/test_support.cpp
        tests/test_encoding.cpp
        tests/test_rsa_oaep.cpp
        tests/test_keyio.cpp
        tests/test_hybrid.cpp
        tests/test_negative.cpp
        tests/test_kat.cpp)
    target_link_libraries(rsa_tests PRIVATE rsa_core Catch2::Catch2WithMain)
    target_compile_definitions(rsa_tests PRIVATE RSALAB_KAT_FILE="${CMAKE_CURRENT_SOURCE_DIR}/kat/vectors.json")
    add_test(NAME lab3_unit_tests COMMAND rsa_tests)

    add_test(NAME lab3_kat_runner COMMAND rsatool --kat ${CMAKE_CURRENT_SOURCE_DIR}/kat/vectors.json)

    add_test(NAME lab3_cli_end_to_end
        COMMAND ${CMAKE_COMMAND}
                -DTOOL=$<TARGET_FILE:rsatool>
                -DWORK=${CMAKE_CURRENT_BINARY_DIR}/e2e_work
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_e2e.cmake)
endif()

# =============================================================================
# Installation (optional)
# =============================================================================
install(TARGETS rsa_core rsatool rsabench
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin)

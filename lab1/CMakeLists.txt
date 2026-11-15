cmake_minimum_required(VERSION 3.20)
project(aes_lab1 LANGUAGES CXX)

# =============================================================================
# Project Configuration
# =============================================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Default build type
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Export compile commands for clangd/VS Code IntelliSense
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# =============================================================================
# Platform & Compiler Detection
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

# Build directory naming hint (for user reference)
message(STATUS "Platform: ${PLATFORM_NAME}, Compiler: ${COMPILER_NAME}")
message(STATUS "Suggested build dir: ${CMAKE_SOURCE_DIR}/build_${PLATFORM_NAME}_${COMPILER_NAME}/")

# =============================================================================
# Module Path (for FindCryptoPP.cmake)
# =============================================================================
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../cmake")

# =============================================================================
# Find Dependencies
# =============================================================================

# --- Crypto++ ---
option(AESTOOL_STATIC_RUNTIME "Link libstdc++/libgcc and Crypto++ statically (used for the release binaries: no extra DLL/.so needed)" OFF)
if(AESTOOL_STATIC_RUNTIME)
    set(CRYPTOPP_PREFER_STATIC ON)
endif()
find_package(CryptoPP REQUIRED)

# Threads (std::thread for `bench --threads`, and Crypto++ on Linux)
find_package(Threads REQUIRED)

# =============================================================================
# Compiler Warning Flags
# =============================================================================
option(AESTOOL_WARNINGS_AS_ERRORS "Treat compiler warnings as errors (developer option; OFF so other compilers still build)" OFF)
if(MSVC)
    add_compile_options(/W4 /permissive- /utf-8)
    if(AESTOOL_WARNINGS_AS_ERRORS)
        add_compile_options(/WX)
    endif()
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
    if(AESTOOL_WARNINGS_AS_ERRORS)
        add_compile_options(-Werror)
    endif()
endif()

# =============================================================================
# Targets
# =============================================================================

# --- Static library: aescore ---
add_library(aescore STATIC src/aescore.cpp)
target_include_directories(aescore PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include>
)
# Crypto++ is the ONLY cryptographic library used by Lab 1 (no OpenSSL, no libsodium).
target_link_libraries(aescore PUBLIC
    CryptoPP::CryptoPP
    Threads::Threads
)

# --- Executable: aestool ---
add_executable(aestool src/main.cpp)
target_link_libraries(aestool PRIVATE aescore)
if(AESTOOL_STATIC_RUNTIME AND NOT MSVC)
    target_link_options(aestool PRIVATE -static)   # MSVC builds are already self-contained when built with /MT
endif()
if(WIN32)
    target_link_libraries(aestool PRIVATE shell32)   # CommandLineToArgvW (UTF-8 argv)
endif()

# =============================================================================
# Tests (Optional)
# =============================================================================
option(BUILD_TESTS "Build unit tests and KAT runners" ON)

if(BUILD_TESTS)
    enable_testing()
    
    # Catch2 v3: use an installed one (apt install catch2 / vcpkg install catch2 / pacman -S mingw-w64-x86_64-catch);
    # otherwise download v3.5.3 at configure time (needs internet once).
    # (MSVC and Linux ignore PATH-derived prefixes, so a MinGW/MSYS2 Catch2 on PATH - e.g. the Windows PATH that WSL
    #  appends - is never picked up by mistake)
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
    
    # On Windows Clang, skip unit tests due to ABI incompatibility with GCC-built libs
    set(SKIP_TESTS_FOR_CLANG_WINDOWS FALSE)
    if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        set(SKIP_TESTS_FOR_CLANG_WINDOWS TRUE)
        message(WARNING "Windows Clang: Skipping unit tests due to ABI incompatibility with a GCC-built Crypto++")
    endif()
    
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_core.cpp" AND NOT SKIP_TESTS_FOR_CLANG_WINDOWS)
        add_executable(test_core tests/test_core.cpp)
        target_link_libraries(test_core PRIVATE aescore Catch2::Catch2WithMain)
        # Clang on Windows: __COUNTER__ is a Microsoft extension, disable C2y warning for tests
        if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
            target_compile_options(test_core PRIVATE -Wno-c2y-extensions)
        endif()
        add_test(NAME lab1_unit_tests COMMAND test_core)
    endif()
    
    # KAT test (if vectors exist) - skip on Windows Clang
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/kat/vectors.json" AND NOT SKIP_TESTS_FOR_CLANG_WINDOWS)
        add_test(NAME lab1_nist_kat COMMAND aestool --kat ${CMAKE_CURRENT_SOURCE_DIR}/kat/vectors.json)
    endif()
    
    # CLI negative tests (if script exists) - skip on Windows Clang
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_negative.cmake" AND NOT SKIP_TESTS_FOR_CLANG_WINDOWS)
        add_test(NAME lab1_cli_negative
            COMMAND ${CMAKE_COMMAND}
                -DTOOL=$<TARGET_FILE:aestool>
                -DWORK=${CMAKE_CURRENT_BINARY_DIR}/cli_neg
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_negative.cmake
        )
    endif()
endif()

# =============================================================================
# Installation (Optional)
# =============================================================================
install(TARGETS aescore aestool
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)
install(DIRECTORY src/ DESTINATION include FILES_MATCHING PATTERN "*.hpp")
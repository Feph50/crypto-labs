cmake_minimum_required(VERSION 3.20)
project(NT219_Crypto_Labs LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands for language servers
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Default build type
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Global module path
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Build options
option(BUILD_LAB1 "Build Lab 1: Symmetric Encryption with Crypto++" ON)
option(BUILD_LAB2 "Build Lab 2: Pure C++ AES-128 (CTR mode)" ON)
option(BUILD_LAB3 "Build Lab 3: RSA-OAEP & Hybrid Encryption" OFF)
option(BUILD_LAB4 "Build Lab 4: Hashing & PKI" OFF)
option(BUILD_LAB5 "Build Lab 5: Digital Signatures" OFF)
option(BUILD_LAB6 "Build Lab 6: Post-Quantum Cryptography" OFF)
option(BUILD_TESTS "Build Unit Tests" ON)

if(BUILD_LAB1)
    add_subdirectory(lab1)
endif()

if(BUILD_LAB2)
    add_subdirectory(lab2)
endif()

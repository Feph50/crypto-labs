# FindCryptoPP.cmake - locate Crypto++ on Windows (MSVC, MinGW64) and Linux WITHOUT editing any path.
#
# Result (when found):
#   CryptoPP_FOUND, CryptoPP_VERSION, CryptoPP_INCLUDE_DIR, CryptoPP_LIBRARY (when found by path)
#   imported target  CryptoPP::CryptoPP
#
# Search order (first hit wins):
#   1. Explicit cache variables:  -DCryptoPP_LIBRARY=<file> -DCryptoPP_INCLUDE_DIR=<dir>
#   2. CMake package config (vcpkg, Conan, a CMake install of Crypto++):  find_package(cryptopp CONFIG)
#   3. pkg-config (Debian/Ubuntu: libcrypto++-dev provides "libcryptopp"; MSYS2 provides "libcryptopp")
#   4. Standard prefixes and the hints below: CMAKE_PREFIX_PATH, -DCRYPTOPP_ROOT_DIR=<dir>,
#      environment variables CRYPTOPP_ROOT_DIR / CRYPTOPP_ROOT, then the system directories
#      (/usr, /usr/local, MSYS2 /mingw64, Homebrew).
#   5. Legacy layout of the original author (last resort only).
#
# Typical set-ups (see the lab README):
#   Ubuntu:  sudo apt install libcrypto++-dev                      -> nothing else needed
#   MSYS2:   pacman -S mingw-w64-x86_64-crypto++                   -> nothing else needed
#   MSVC:    vcpkg install cryptopp:x64-windows-static             -> -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake
#                                                                     -DVCPKG_TARGET_TRIPLET=x64-windows-static
#   anywhere: -DCRYPTOPP_ROOT_DIR=/path/to/cryptopp-install

include(FindPackageHandleStandardArgs)

if(TARGET CryptoPP::CryptoPP)
    set(CryptoPP_FOUND TRUE)
    return()
endif()

set(_cpp_how "")

# Optional: -DCRYPTOPP_PREFER_STATIC=ON makes the path-based searches prefer libcryptopp.a over libcryptopp.so
set(_cpp_old_suffixes "${CMAKE_FIND_LIBRARY_SUFFIXES}")
if(CRYPTOPP_PREFER_STATIC AND NOT MSVC)
    set(CMAKE_FIND_LIBRARY_SUFFIXES .a ${CMAKE_FIND_LIBRARY_SUFFIXES})
endif()

# A Crypto++ built for another toolchain must never be picked up just because its bin directory is on PATH:
#   * MSVC with the MSYS2 mingw64 bin directory on PATH (different ABI and headers),
#   * Linux under WSL, where the Windows PATH (/mnt/c/...) is appended automatically.
# So for MSVC and for Linux the directories derived from PATH are ignored (system prefixes /usr and /usr/local, vcpkg,
# CMAKE_PREFIX_PATH and CRYPTOPP_ROOT_DIR still work). MinGW keeps using PATH (its prefix is derived from g++'s bin dir).
set(_cpp_noenv "")
if(MSVC OR (UNIX AND NOT WIN32))
    set(_cpp_noenv NO_SYSTEM_ENVIRONMENT_PATH)
endif()

# ---- 1. explicit cache variables -------------------------------------------------------------------------------------
if(CryptoPP_LIBRARY AND CryptoPP_INCLUDE_DIR)
    set(_cpp_how "explicit -DCryptoPP_LIBRARY/-DCryptoPP_INCLUDE_DIR")
endif()

# ---- 2. CMake package config -----------------------------------------------------------------------------------------
if(NOT _cpp_how)
    find_package(cryptopp CONFIG QUIET ${_cpp_noenv})
    if(NOT cryptopp_FOUND)
        find_package(CryptoPP CONFIG QUIET ${_cpp_noenv})   # some builds install CryptoPPConfig.cmake
    endif()
    foreach(_t cryptopp::cryptopp cryptopp::cryptopp-static cryptopp::cryptopp-shared
               cryptopp-static cryptopp-shared cryptopp CryptoPP::cryptopp)
        if(TARGET ${_t})
            add_library(CryptoPP::CryptoPP INTERFACE IMPORTED)
            set_property(TARGET CryptoPP::CryptoPP PROPERTY INTERFACE_LINK_LIBRARIES ${_t})
            set(_cpp_how "CMake package config (${_t})")
            set(CryptoPP_FOUND TRUE)
            break()
        endif()
    endforeach()
    if(_cpp_how)
        if(cryptopp_VERSION)
            set(CryptoPP_VERSION "${cryptopp_VERSION}")
        endif()
        message(STATUS "Found Crypto++ ${CryptoPP_VERSION} via ${_cpp_how}")
        return()
    endif()
endif()

# ---- 3. pkg-config ---------------------------------------------------------------------------------------------------
if(NOT _cpp_how)
    if(NOT MSVC)
        find_package(PkgConfig QUIET)
    endif()
    if(PKG_CONFIG_FOUND)
        foreach(_pc libcryptopp libcrypto++ cryptopp)
            pkg_check_modules(_CPP_PC QUIET ${_pc})
            if(_CPP_PC_FOUND)
                break()
            endif()
        endforeach()
        if(_CPP_PC_FOUND)
            find_library(CryptoPP_LIBRARY NAMES ${_CPP_PC_LIBRARIES} cryptopp crypto++ HINTS ${_CPP_PC_LIBRARY_DIRS})
            find_path(CryptoPP_INCLUDE_DIR NAMES cryptopp/cryptlib.h crypto++/cryptlib.h cryptlib.h
                      HINTS ${_CPP_PC_INCLUDE_DIRS})
            if(CryptoPP_LIBRARY AND CryptoPP_INCLUDE_DIR)
                set(_cpp_how "pkg-config")
                if(_CPP_PC_VERSION)
                    set(CryptoPP_VERSION "${_CPP_PC_VERSION}")
                endif()
            endif()
        endif()
    endif()
endif()

# ---- 4. prefixes / hints / system directories ------------------------------------------------------------------------
if(NOT _cpp_how)
    set(_cpp_roots "")
    foreach(_r "${CRYPTOPP_ROOT_DIR}" "$ENV{CRYPTOPP_ROOT_DIR}" "$ENV{CRYPTOPP_ROOT}")
        if(_r)
            list(APPEND _cpp_roots "${_r}")
        endif()
    endforeach()
    # <root>/include, <root>/lib and the layouts produced by common Windows builds of Crypto++
    set(_cpp_inc_hints "")
    set(_cpp_lib_hints "")
    foreach(_r IN LISTS _cpp_roots)
        list(APPEND _cpp_inc_hints "${_r}" "${_r}/include")
        list(APPEND _cpp_lib_hints "${_r}" "${_r}/lib" "${_r}/lib64" "${_r}/library"
             "${_r}/x64/Output/Release" "${_r}/x64/cryptlib/Release" "${_r}/x64/Output/Debug")
    endforeach()

    find_path(CryptoPP_INCLUDE_DIR NAMES cryptopp/cryptlib.h crypto++/cryptlib.h cryptlib.h
              HINTS ${_cpp_inc_hints}
              PATH_SUFFIXES include
              ${_cpp_noenv})
    find_library(CryptoPP_LIBRARY NAMES cryptopp crypto++ cryptlib libcryptopp cryptopp-static
                 HINTS ${_cpp_lib_hints}
                 PATH_SUFFIXES lib lib64
                 ${_cpp_noenv})
    if(CryptoPP_LIBRARY AND CryptoPP_INCLUDE_DIR)
        set(_cpp_how "search paths")
    endif()
endif()

# ---- 5. legacy layout of the original author (kept only so old checkouts still configure) ----------------------------
if(NOT _cpp_how)
    set(_cpp_legacy "D:/UIT/NT219/libs/cryptopp" "$ENV{HOME}/Desktop/nt219lib/cryptopp")
    find_path(CryptoPP_INCLUDE_DIR NAMES cryptlib.h cryptopp/cryptlib.h
              PATHS ${_cpp_legacy}
              PATH_SUFFIXES include gcc/include clang/include NO_DEFAULT_PATH)
    find_library(CryptoPP_LIBRARY NAMES cryptopp cryptlib libcryptopp
                 PATHS ${_cpp_legacy}
                 PATH_SUFFIXES library/msvc library/gcc library/clang gcc/lib clang/lib lib NO_DEFAULT_PATH)
    if(CryptoPP_LIBRARY AND CryptoPP_INCLUDE_DIR)
        set(_cpp_how "legacy author layout")
    endif()
endif()

set(CMAKE_FIND_LIBRARY_SUFFIXES "${_cpp_old_suffixes}")

# ---- version (from config_ver.h / config.h when available) -----------------------------------------------------------
if(CryptoPP_INCLUDE_DIR AND NOT CryptoPP_VERSION)
    foreach(_h "${CryptoPP_INCLUDE_DIR}/cryptopp/config_ver.h" "${CryptoPP_INCLUDE_DIR}/crypto++/config_ver.h"
               "${CryptoPP_INCLUDE_DIR}/config_ver.h" "${CryptoPP_INCLUDE_DIR}/cryptopp/config.h"
               "${CryptoPP_INCLUDE_DIR}/crypto++/config.h" "${CryptoPP_INCLUDE_DIR}/config.h")
        if(EXISTS "${_h}")
            file(STRINGS "${_h}" _ver_line REGEX "^#define[ \t]+CRYPTOPP_VERSION[ \t]+[0-9]+")
            if(_ver_line)
                string(REGEX REPLACE ".*CRYPTOPP_VERSION[ \t]+([0-9]+).*" "\\1" _ver "${_ver_line}")
                math(EXPR _maj "${_ver} / 100")
                math(EXPR _min "(${_ver} / 10) % 10")
                math(EXPR _pat "${_ver} % 10")
                set(CryptoPP_VERSION "${_maj}.${_min}.${_pat}")
                break()
            endif()
        endif()
    endforeach()
endif()

find_package_handle_standard_args(CryptoPP
    REQUIRED_VARS CryptoPP_LIBRARY CryptoPP_INCLUDE_DIR
    VERSION_VAR CryptoPP_VERSION
    FAIL_MESSAGE "Crypto++ was not found. Install it (Ubuntu: sudo apt install libcrypto++-dev | MSYS2: pacman -S mingw-w64-x86_64-crypto++ | MSVC: vcpkg install cryptopp:x64-windows-static and pass -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake) or pass -DCRYPTOPP_ROOT_DIR=<install prefix> or -DCryptoPP_LIBRARY=<file> -DCryptoPP_INCLUDE_DIR=<dir>.")

if(CryptoPP_FOUND AND NOT TARGET CryptoPP::CryptoPP)
    add_library(CryptoPP::CryptoPP UNKNOWN IMPORTED)
    set_target_properties(CryptoPP::CryptoPP PROPERTIES
        IMPORTED_LOCATION "${CryptoPP_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${CryptoPP_INCLUDE_DIR}")
    if(WIN32 AND NOT MSVC)
        set_property(TARGET CryptoPP::CryptoPP APPEND PROPERTY INTERFACE_LINK_LIBRARIES ws2_32)
    endif()
    message(STATUS "Crypto++ found via ${_cpp_how}: ${CryptoPP_LIBRARY}")
endif()

mark_as_advanced(CryptoPP_INCLUDE_DIR CryptoPP_LIBRARY)

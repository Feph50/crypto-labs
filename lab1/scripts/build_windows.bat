@echo off
setlocal EnableExtensions EnableDelayedExpansion

REM ===========================================================================
REM build_windows.bat - Build + test Lab 1 (aestool) on Windows with MSVC or MinGW64 (GCC)
REM
REM Usage (from the lab1 folder or anywhere):
REM     scripts\build_windows.bat [msvc|gcc|clang] [clean]
REM
REM   msvc   Microsoft Visual C++ (default). Run inside "x64 Native Tools Command Prompt for VS".
REM   gcc    MinGW-w64 GCC (MSYS2 mingw64) - put C:\msys64\mingw64\bin first in PATH.
REM   clang  Clang from MSYS2 mingw64.
REM
REM No path has to be edited. Crypto++ is found automatically:
REM   MSVC : set VCPKG_ROOT (or pass nothing if you already pass your own -D flags through CMAKE_ARGS) after
REM          vcpkg install cryptopp:x64-windows-static     -> the script uses the vcpkg toolchain + static /MT.
REM   MinGW: pacman -S mingw-w64-x86_64-crypto++ mingw-w64-x86_64-catch mingw-w64-x86_64-ninja
REM Optional environment variables:
REM   VCPKG_ROOT         vcpkg checkout (MSVC route)
REM   CRYPTOPP_ROOT_DIR  install prefix of a self-built Crypto++ (headers in include\, library in lib\)
REM   CMAKE_ARGS         extra arguments passed verbatim to the CMake configure step
REM ===========================================================================

cd /d "%~dp0\.."

set "COMPILER=msvc"
set "ACTION="
:PARSE_ARGS
if "%~1"=="" goto :DONE_PARSE
if /I "%~1"=="msvc"  set "COMPILER=msvc"  & shift & goto :PARSE_ARGS
if /I "%~1"=="gcc"   set "COMPILER=gcc"   & shift & goto :PARSE_ARGS
if /I "%~1"=="clang" set "COMPILER=clang" & shift & goto :PARSE_ARGS
if /I "%~1"=="clean" set "ACTION=clean"   & shift & goto :PARSE_ARGS
echo [WARN] Unknown argument: %~1
shift
goto :PARSE_ARGS
:DONE_PARSE

if /I "%ACTION%"=="clean" (
    for /d %%D in (build_windows_*) do rmdir /s /q "%%D" 2>nul
    echo Build directories removed.
    exit /b 0
)

where cmake >nul 2>nul
if errorlevel 1 (
    echo [ERROR] cmake not found in PATH. Install: winget install Kitware.CMake
    exit /b 1
)

REM Prefer Ninja when available (faster, works for every compiler); otherwise fall back to CMake's default.
set "GEN="
where ninja >nul 2>nul
if not errorlevel 1 set "GEN=-G Ninja"

set "BUILD_DIR=build_windows_%COMPILER%"
set "EXTRA="
set "CXX="

if /I "%COMPILER%"=="msvc" (
    where cl >nul 2>nul
    if errorlevel 1 (
        echo [ERROR] cl.exe not found. Open "x64 Native Tools Command Prompt for VS" and run again.
        exit /b 1
    )
    set "EXTRA=-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded"
    if not "%VCPKG_ROOT%"=="" set "EXTRA=!EXTRA! -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT:\=/%/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows-static"
)
if /I "%COMPILER%"=="gcc" (
    where g++ >nul 2>nul
    if errorlevel 1 (
        echo [ERROR] g++ not found in PATH. Use MSYS2 mingw64: C:\msys64\mingw64\bin
        exit /b 1
    )
    if "%GEN%"=="" set "GEN=-G "MinGW Makefiles""
    set "CXX=-DCMAKE_CXX_COMPILER=g++"
)
if /I "%COMPILER%"=="clang" (
    where clang++ >nul 2>nul
    if errorlevel 1 (
        echo [ERROR] clang++ not found in PATH. Use MSYS2 mingw64.
        exit /b 1
    )
    if "%GEN%"=="" set "GEN=-G "MinGW Makefiles""
    set "CXX=-DCMAKE_CXX_COMPILER=clang++"
)
if not "%CRYPTOPP_ROOT_DIR%"=="" set "EXTRA=!EXTRA! -DCRYPTOPP_ROOT_DIR=%CRYPTOPP_ROOT_DIR:\=/%"

echo == 1/4 Configure (%COMPILER%, build dir: %BUILD_DIR%) ==
cmake -S . -B "%BUILD_DIR%" %GEN% %CXX% -DCMAKE_BUILD_TYPE=Release !EXTRA! %CMAKE_ARGS%
if errorlevel 1 ( echo [ERROR] Configure failed. & exit /b 1 )

echo == 2/4 Build ==
cmake --build "%BUILD_DIR%" --config Release
if errorlevel 1 ( echo [ERROR] Build failed. & exit /b 1 )

echo == 3/4 Test (ctest) ==
pushd "%BUILD_DIR%"
ctest -C Release --output-on-failure
set "CTEST_RC=!ERRORLEVEL!"
popd
if not "!CTEST_RC!"=="0" ( echo [ERROR] Tests failed. & exit /b 1 )

echo == 4/4 KAT (Known Answer Tests) ==
set "EXE=%BUILD_DIR%\aestool.exe"
if not exist "!EXE!" set "EXE=%BUILD_DIR%\Release\aestool.exe"
if not exist "!EXE!" ( echo [ERROR] Executable not found. & exit /b 1 )
"!EXE!" --kat kat\vectors.json
if errorlevel 1 ( echo [ERROR] KAT failed. & exit /b 1 )

echo.
echo SUCCESS - binary: %CD%\!EXE!
exit /b 0

@echo off
rem Silent Storm (A5) - VS2022 build driver.
rem Usage: build.cmd [build-dir] [config] [target...]
rem   build-dir default: %~dp0..\..\..\..\..\_build\vs2022   (keep it SHORT: MSVC has a 260-char path limit)
rem   config    default: RelWithDebInfo
setlocal enabledelayedexpansion
set "SRC=%~dp0"
set "BLD=%~1"
if "%BLD%"=="" set "BLD=%SRC%..\..\..\..\..\_build\vs2022"
set "CFG=%~2"
if "%CFG%"=="" set "CFG=RelWithDebInfo"
set "TGT="
:more
if not "%~3"=="" ( set "TGT=!TGT! %~3" & shift /3 & goto more )

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" ( echo vswhere.exe not found - install Visual Studio 2022 or Build Tools & exit /b 1 )
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSDIR=%%i"
if "%VSDIR%"=="" ( echo No Visual Studio with the C++ x86/x64 toolset found & exit /b 1 )
call "%VSDIR%\VC\Auxiliary\Build\vcvarsall.bat" x86 >nul 2>nul || exit /b 1

where cmake >nul 2>nul || set "PATH=%VSDIR%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;%PATH%"
where ninja >nul 2>nul || set "PATH=%VSDIR%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;%PATH%"

cmake -G Ninja -S "%SRC%." -B "%BLD%" -DCMAKE_BUILD_TYPE=%CFG% -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl || exit /b 1
if "%TGT%"=="" ( cmake --build "%BLD%" -- -k 0 ) else ( cmake --build "%BLD%" --target %TGT% -- -k 0 )
exit /b %ERRORLEVEL%

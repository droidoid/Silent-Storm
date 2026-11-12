@echo off
rem Silent Storm (A5) - VS2022 build driver.
rem Usage: build.cmd [build-dir] [config] [target...]
rem   build-dir default: %~dp0..\..\..\..\..\_build\vs2022   (keep it SHORT: MSVC has a 260-char path limit)
rem   config    default: RelWithDebInfo
rem
rem Exactly ONE Visual Studio installation is used for everything: vcvarsall, cl.exe, link.exe, dumpbin.exe,
rem cmake.exe and ninja.exe all come from it (several installs side by side used to mix compiler and CMake).
rem   A5_VSDIR  - optional: installation to use (e.g. C:\Program Files\Microsoft Visual Studio\2022\Community).
rem               Default: the newest VS 2022 (17.x) with the C++ x86/x64 toolset, else the newest VS with it.
rem   A5_CMAKE  - optional: cmake.exe to use instead of the one bundled with that installation (3.24 or newer).
rem If the build dir was configured with another compiler or CMake, it is reconfigured from scratch (--fresh).
setlocal enabledelayedexpansion
set "SRC=%~dp0"
set "BLD=%~1"
if "%BLD%"=="" set "BLD=%SRC%..\..\..\..\..\_build\vs2022"
set "CFG=%~2"
if "%CFG%"=="" set "CFG=RelWithDebInfo"
set "TGT="
:more
if not "%~3"=="" ( set "TGT=!TGT! %~3" & shift /3 & goto more )

rem --- 1. pick one Visual Studio installation ----------------------------------------------------------
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "VCREQ=Microsoft.VisualStudio.Component.VC.Tools.x86.x64"
set "VSDIR=%A5_VSDIR%"
if not "%VSDIR%"=="" goto vs_chosen
if not exist "%VSWHERE%" echo [build] vswhere.exe not found - install Visual Studio 2022 or Build Tools 2022, or set A5_VSDIR& exit /b 1
for /f "usebackq delims=" %%i in (`call "%VSWHERE%" -latest -products * -version "[17.0,18.0)" -requires %VCREQ% -property installationPath`) do if "!VSDIR!"=="" set "VSDIR=%%i"
if not "%VSDIR%"=="" goto vs_chosen
for /f "usebackq delims=" %%i in (`call "%VSWHERE%" -latest -products * -requires %VCREQ% -property installationPath`) do if "!VSDIR!"=="" set "VSDIR=%%i"
:vs_chosen
if "%VSDIR%"=="" set "VSDIR=?"
if "%VSDIR:~-1%"=="\" set "VSDIR=%VSDIR:~0,-1%"
if not exist "!VSDIR!\VC\Auxiliary\Build\vcvarsall.bat" ( echo [build] No Visual Studio with the C++ x86/x64 toolset found ^(VSDIR="!VSDIR!"^) & exit /b 1 )
echo [build] Visual Studio: %VSDIR%

rem A developer prompt of another installation would leave its tools in PATH/INCLUDE/LIB; vcvarsall on top of
rem it prepends the chosen one, and step 2 checks that cl.exe really comes from the chosen installation.
set "_OLD=%VSINSTALLDIR%"
if defined _OLD if "!_OLD:~-1!"=="\" set "_OLD=!_OLD:~0,-1!"
if defined _OLD if /i not "!_OLD!"=="!VSDIR!" echo [build] note: started from a developer prompt of !_OLD! - it is overridden
call "%VSDIR%\VC\Auxiliary\Build\vcvarsall.bat" x86 >nul 2>nul
if not defined VCToolsInstallDir ( echo [build] vcvarsall.bat x86 failed for !VSDIR! & exit /b 1 )

rem --- 2. compiler / linker from that installation (passed to CMake by full path) -------------------------
set "CL_EXE="
for /f "delims=" %%i in ('where cl.exe 2^>nul') do if "!CL_EXE!"=="" set "CL_EXE=%%i"
if "%CL_EXE%"=="" ( echo [build] cl.exe not found after vcvarsall & exit /b 1 )
set "_REST=!CL_EXE:%VCToolsInstallDir%=!"
if "!_REST!"=="!CL_EXE!" ( echo [build] PATH resolves cl.exe to "!CL_EXE!", which is not under "!VCToolsInstallDir!" & exit /b 1 )
echo [build] compiler:      %CL_EXE%

rem --- 3. CMake and Ninja from the same installation (A5_CMAKE overrides CMake) -------------------------
set "VSCMAKE=%VSDIR%\Common7\IDE\CommonExtensions\Microsoft\CMake"
set "CMAKE_EXE=%A5_CMAKE%"
if "%CMAKE_EXE%"=="" if exist "%VSCMAKE%\CMake\bin\cmake.exe" set "CMAKE_EXE=%VSCMAKE%\CMake\bin\cmake.exe"
if "!CMAKE_EXE!"=="" ( echo [build] !VSDIR! has no bundled CMake ^(component "C++ CMake tools for Windows"^); install it or set A5_CMAKE & exit /b 1 )
if not exist "!CMAKE_EXE!" ( echo [build] cmake not found: !CMAKE_EXE! & exit /b 1 )
set "NINJA_EXE="
if exist "%VSCMAKE%\Ninja\ninja.exe" set "NINJA_EXE=%VSCMAKE%\Ninja\ninja.exe"
if "%NINJA_EXE%"=="" for /f "delims=" %%i in ('where ninja.exe 2^>nul') do if "!NINJA_EXE!"=="" set "NINJA_EXE=%%i"
if "%NINJA_EXE%"=="" ( echo [build] ninja.exe not found & exit /b 1 )
set "CMVER="
for /f "tokens=3" %%v in ('call "%CMAKE_EXE%" --version') do if "!CMVER!"=="" set "CMVER=%%v"
for /f "tokens=1,2 delims=.-" %%a in ("!CMVER!") do set /a "CMNUM=%%a*100+%%b"
if !CMNUM! LSS 324 ( echo [build] CMake !CMVER! is too old - 3.24 or newer is needed: !CMAKE_EXE! & exit /b 1 )
echo [build] cmake:         %CMAKE_EXE% (!CMVER!)
echo [build] ninja:         %NINJA_EXE%

rem --- 4. configure (from scratch if the build dir was made with another toolchain) and build ----------
set "STAMP=%VSDIR%^|%CL_EXE%^|%CMAKE_EXE%"
set "FRESH="
if exist "%BLD%\CMakeCache.txt" (
	set "OLDSTAMP="
	if exist "%BLD%\a5_toolchain.txt" set /p OLDSTAMP=<"%BLD%\a5_toolchain.txt"
	if not "!OLDSTAMP!"=="!VSDIR!|!CL_EXE!|!CMAKE_EXE!" (
		echo [build] !BLD! was configured with another toolchain - reconfiguring from scratch
		set "FRESH=--fresh"
	)
)
set "CL_FWD=%CL_EXE:\=/%"
set "NINJA_FWD=%NINJA_EXE:\=/%"
"%CMAKE_EXE%" %FRESH% -G Ninja -S "%SRC%." -B "%BLD%" -DCMAKE_BUILD_TYPE=%CFG% -DCMAKE_MAKE_PROGRAM="%NINJA_FWD%" -DCMAKE_C_COMPILER="%CL_FWD%" -DCMAKE_CXX_COMPILER="%CL_FWD%" || exit /b 1
> "%BLD%\a5_toolchain.txt" echo %STAMP%
if "%TGT%"=="" ( "%CMAKE_EXE%" --build "%BLD%" -- -k 0 ) else ( "%CMAKE_EXE%" --build "%BLD%" --target %TGT% -- -k 0 )
exit /b %ERRORLEVEL%

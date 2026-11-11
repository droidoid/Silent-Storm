@echo off
rem Builds tools\a5dbg.exe (crash-reporting launcher) into the given directory (default: next to this script).
setlocal
set "OUT=%~1"
if "%OUT%"=="" set "OUT=%~dp0"
for /f "usebackq delims=" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -property installationPath`) do set "VSDIR=%%i"
call "%VSDIR%\VC\Auxiliary\Build\vcvarsall.bat" x86 >nul 2>nul || exit /b 1
cl /nologo /EHsc /O1 /Fo"%OUT%\\" /Fe"%OUT%\a5dbg.exe" "%~dp0a5dbg.cpp" dbghelp.lib

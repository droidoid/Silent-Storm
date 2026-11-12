@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x86 >nul
set CM="C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
%CM% -S C:\a5b\ext\recon -B C:\a5b\ext\rb -G "Visual Studio 17 2022" -A Win32 || exit /b 1
%CM% --build C:\a5b\ext\rb --config Release -- /m /v:minimal /clp:ErrorsOnly

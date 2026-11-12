@echo off
rem Builds stand.exe: stand.cpp + all Main sources (compiled here with the port's flags; the port's own Main
rem objects are not present in C:\a5b\s) + the port's import libs from C:\a5b\s\lib. Everything is written to out\.
setlocal
set SRC=C:\Users\admin\SS\Silent-Storm\Soft\Andy\Jan03\a5dll
set B=C:\a5b\s
set O=C:\a5b\recon\c\stand\out
if not exist %O%\main mkdir %O%\main
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x86 >nul || exit /b 1
set FLAGS=/nologo /c /DWIN32 /D_WINDOWS /EHsc /Zi /O2 /Ob1 /DNDEBUG /MD /source-charset:windows-1251 /execution-charset:windows-1251 /W1 /bigobj /Zm300 /GR /Zc:strictStrings- -DA5_NULL_LIFESTUDIO -DA5_VS2022_PORT -D_CRT_NONSTDC_NO_WARNINGS -D_CRT_SECURE_NO_WARNINGS -D_HAS_AUTO_PTR_ETC=1 -D_HAS_STD_BYTE=0 -D_SCL_SECURE_NO_WARNINGS -D_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS -D_SILENCE_STDEXT_HASH_DEPRECATION_WARNINGS -D_USRDLL -I%SRC%\vs2022\compat -I%SRC%\Main -I%SRC%\vs2022\stubs\lifestudio
cd /d %O%
if "%1"=="nomain" goto stand
cl %FLAGS% /MP16 /Fo%O%\main\ /Fd%O%\main\vc.pdb @C:\a5b\recon\c\stand\main_src.rsp > %O%\main_cl.log 2>&1 || (echo MAIN COMPILE FAILED & exit /b 1)
:stand
cl %FLAGS% /Fo%O%\stand.obj /Fd%O%\stand_vc.pdb C:\a5b\recon\c\stand\stand.cpp || exit /b 1
link /nologo /machine:X86 /debug /subsystem:console /out:%O%\stand.exe %O%\stand.obj %O%\main\*.obj ^
 %B%\lib\Misc.lib %B%\lib\Script.lib %B%\lib\DBFormat.lib %B%\lib\FModSound.lib dxguid.lib d3d9.lib %B%\lib\Input.lib %B%\lib\MiscDll.lib %B%\lib\ADOImport.lib %B%\lib\FileIO.lib ^
 kernel32.lib user32.lib gdi32.lib winspool.lib shell32.lib ole32.lib oleaut32.lib uuid.lib comdlg32.lib advapi32.lib || exit /b 1
echo BUILD OK

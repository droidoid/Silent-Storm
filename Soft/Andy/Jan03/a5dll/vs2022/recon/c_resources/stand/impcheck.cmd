@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x86 >nul
cd /d C:\a5b\recon\c\stand\out
dumpbin /nologo /imports stand.exe > imports.txt
for %%d in (FileIO MiscDll Script DBFormat FModSound Input ADOImport) do dumpbin /nologo /exports C:\a5b\s\bin\%%d.dll > exp_%%d.txt
echo done

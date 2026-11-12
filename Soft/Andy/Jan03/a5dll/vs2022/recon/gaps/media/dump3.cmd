@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x86 >nul 2>nul
for %%f in (Game.exe Main.dll MapEdit.exe TexConv.exe) do if exist C:\a5b\s\bin\%%f dumpbin /dependents C:\a5b\s\bin\%%f
dumpbin /imports:Image.dll C:\a5b\s\bin\Main.dll
dumpbin /imports:FModSound.dll C:\a5b\s\bin\Main.dll
dumpbin /exports C:\a5b\s\bin\Image.dll
dumpbin /imports C:\a5b\s\bin\FModSound.dll

@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x86 >nul 2>nul
set R=C:\Users\admin\SS\Silent-Storm
dumpbin /imports %R%\Versions\Current\Game.exe > C:\a5b\recon\gaps_m\B\game_vercur_imports.txt
dumpbin /imports %R%\Soft\Andy\RussianGold\Game.exe > C:\a5b\recon\gaps_m\B\game_rugold_imports.txt
dumpbin /headers %R%\Versions\Current\Game.exe | findstr /i "date stamp" > C:\a5b\recon\gaps_m\B\game_vercur_hdr.txt

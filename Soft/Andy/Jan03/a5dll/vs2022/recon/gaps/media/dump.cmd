@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x86 >nul
set R=C:\Users\admin\SS\Silent-Storm\bin
dumpbin /exports %R%\fmod.dll > C:\a5b\recon\gaps_m\B\fmod_exports.txt
dumpbin /exports %R%\LifeStudioHeadAPI.dll > C:\a5b\recon\gaps_m\B\lifestudio_exports.txt
dumpbin /exports %R%\binkw32.dll > C:\a5b\recon\gaps_m\B\bink_exports.txt
dumpbin /exports %R%\FMODWrapper.dll > C:\a5b\recon\gaps_m\B\fmodwrapper_exports.txt
dumpbin /imports %R%\Game.exe > C:\a5b\recon\gaps_m\B\game_imports.txt
dumpbin /imports %R%\FMODWrapper.dll > C:\a5b\recon\gaps_m\B\fmodwrapper_imports.txt
dumpbin /imports %R%\LifeStudioHeadAPI.dll > C:\a5b\recon\gaps_m\B\lifestudio_imports.txt

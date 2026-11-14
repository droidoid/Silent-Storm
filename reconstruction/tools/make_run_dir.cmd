@echo off
rem Builds a run directory for Game.exe from the data in this repository (Complete\, bin\ and
rem Versions\Current\scripts\).
rem Everything is COPIED (no links), so the game can never write into Complete\.
rem
rem usage: make_run_dir.cmd <run-dir> <build-output-dir> [repo-root]
rem   build-output-dir: the folder with Game.exe and PkgBuilder.exe, e.g. build\Release
rem   repo-root:        default = two levels above this script (reconstruction\tools -> repo root)
rem
rem What it does (the release layout, see Complete\makeCurrent.bat):
rem   - Game.exe, zlib.dll from the build; fmod, binkw32, LifeStudioHeadAPI, GDPFile, IMorphFile,
rem     TriangLib DLLs from bin\
rem   - game.db, tree.mma, cfg\*.cfg, res\ (FaceGenHead, Video) from Complete\
rem   - scripts\*.l from Versions\Current\scripts\ (not Complete\Scripts, see below)
rem   - packs every resource folder of Complete\ into res\<Name>.res with the built PkgBuilder
rem     (an existing pack is kept; delete it to rebuild)
rem   - start.cfg with "mainmenu" (console commands run at start-up; without it the game stays on
rem     the developers' intermission screen)
rem Then run:  Game.exe -windowed -800   (full screen may fail on remote desktops: "Failed to set display mode")
setlocal enabledelayedexpansion
if "%~2"=="" ( echo usage: %~nx0 run-dir build-output-dir [repo-root] & exit /b 1 )
set "RUN=%~f1"
set "BLD=%~f2"
set "ROOT=%~f3"
if "%ROOT%"=="" for %%i in ("%~dp0..\..") do set "ROOT=%%~fi"
set "DATA=%ROOT%\Complete"
if not exist "%BLD%\Game.exe" ( echo Game.exe not found in %BLD% & exit /b 1 )
if not exist "%BLD%\PkgBuilder.exe" ( echo PkgBuilder.exe not found in %BLD% & exit /b 1 )
if not exist "%DATA%\game.db" ( echo %DATA%\game.db not found & exit /b 1 )
if not exist "%RUN%\res" mkdir "%RUN%\res" || exit /b 1
if not exist "%RUN%\cfg" mkdir "%RUN%\cfg" || exit /b 1

copy /y "%BLD%\Game.exe" "%RUN%\" >nul || exit /b 1
if exist "%BLD%\zlib.dll" copy /y "%BLD%\zlib.dll" "%RUN%\" >nul
for %%d in (fmod binkw32 LifeStudioHeadAPI GDPFile IMorphFile TriangLib) do (
	copy /y "%ROOT%\bin\%%d.dll" "%RUN%\" >nul || ( echo missing %ROOT%\bin\%%d.dll & exit /b 1 )
)
copy /y "%DATA%\game.db" "%RUN%\" >nul || exit /b 1
rem tree.mma: the LifeStudio muscle tree for unit heads; without it a mission crashes on load
rem (null read in LifeStudioHeadAPI.dll), the main menu does not need it
copy /y "%DATA%\tree.mma" "%RUN%\" >nul || ( echo missing %DATA%\tree.mma & exit /b 1 )
copy /y "%DATA%\cfg\*.cfg" "%RUN%\cfg\" >nul
rem autoexec.cfg: the one in Complete\cfg runs "exec c:\a5\cfg\input.cfg" - a path of the developers'
rem machines. With that file the binds are never loaded: no keyboard, no mouse, and the game pins
rem the mouse pointer of the whole desktop. Write one with relative paths, as the retail game has.
> "%RUN%\cfg\autoexec.cfg" (
	echo // written by make_run_dir.cmd
	echo exec .\cfg\input.cfg
	echo exec .\cfg\config.cfg
)
findstr /i /c:"cursor_x" "%RUN%\cfg\input.cfg" >nul || ( echo %RUN%\cfg\input.cfg has no cursor binds & exit /b 1 )
rem scripts\*.l: the Lua libraries every world auto-loads by file name (game.db table AutoLoadScripts:
rem Constants.l, TriggersManager.l, Common.l, Hint.l); without them mission scripts fail with "call a
rem nil value". They come from Versions\Current\scripts, the release set that game.db goes with.
rem Complete\Scripts holds an older Common.l without DelayGameStartEx, StartGameEx,
rem StartGameWithSequence, DialogPlayWithSequence, DividedDeployDemo, GroupGiveXP, OnPlayerLose and
rem OnRealExit: 33 game.db scripts call them, and the engine itself calls the last two (defeat,
rem leaving a zone). The other three files are the same in both folders.
set "SCRIPTS=%ROOT%\Versions\Current\scripts"
xcopy /i /y /q "%SCRIPTS%\*.l" "%RUN%\scripts" >nul || ( echo missing %SCRIPTS% & exit /b 1 )
findstr /b /c:"function OnRealExit" "%RUN%\scripts\Common.l" >nul || ( echo %RUN%\scripts\Common.l is not the release one & exit /b 1 )
xcopy /e /i /y /q "%DATA%\res" "%RUN%\res" >nul

for %%t in (AIBinds AIBSPTrees AIGeometries Animations Binds Buildings Chapters Effects Fonts Geometries Globals Groups Heads LRTextures Lights Locators Sequences Skeletons Sounds Terrain Textures Units Waypoints) do (
	if exist "%RUN%\res\%%t.res" (
		echo   %%t.res exists - kept
	) else (
		echo   packing %%t
		"%BLD%\PkgBuilder.exe" "%RUN%\res\%%t.res" "%DATA%\%%t" >nul || ( echo PkgBuilder failed on %%t & exit /b 1 )
	)
)
if not exist "%RUN%\start.cfg" > "%RUN%\start.cfg" echo mainmenu
echo run directory ready: %RUN%
echo start:  cd /d "%RUN%" ^&^& Game.exe -windowed -800

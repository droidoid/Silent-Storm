@echo off
rem Creates a run directory for the ported Game.exe without touching the original game data.
rem usage: make_run_dir.cmd <run-dir> <path-to-Complete> <path-to-built-bin>
rem   - copies game.db and cfg\ (the game writes cfg\config.cfg and save\ - these stay in the run dir)
rem   - links every resource folder of Complete\ as a directory junction (read by the game, not copied)
rem   - the *.res packs are NOT linked: they carry a newer pack signature than the Jan03 code reads,
rem     the game falls back to the loose files in the resource folders
setlocal enabledelayedexpansion
if "%~3"=="" ( echo usage: %~nx0 run-dir path-to-Complete path-to-built-bin & exit /b 1 )
set "RUN=%~f1"
set "DATA=%~f2"
set "BIN=%~f3"
if not exist "%DATA%\game.db" ( echo %DATA%\game.db not found & exit /b 1 )
if not exist "%RUN%" mkdir "%RUN%"
if not exist "%RUN%\cfg" mkdir "%RUN%\cfg"
copy /y "%DATA%\game.db" "%RUN%\game.db" >nul
copy /y "%DATA%\cfg\*.cfg" "%RUN%\cfg\" >nul
for /d %%d in ("%DATA%\*") do (
	if /i not "%%~nxd"=="cfg" if not exist "%RUN%\%%~nxd" mklink /J "%RUN%\%%~nxd" "%%d" >nul
)
copy /y "%BIN%\*.dll" "%RUN%\" >nul
copy /y "%BIN%\Game.exe" "%RUN%\" >nul
if not exist "%RUN%\start.cfg" echo // console commands executed on the intermission screen> "%RUN%\start.cfg"
echo run directory ready: %RUN%

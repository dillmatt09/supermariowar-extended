@echo off
rem Runs the game built in the build folder, with the data folder of this repository.
rem Any extra arguments are passed on to the game.
cd /d "%~dp0"
if not exist "build\smw.exe" (
    echo build\smw.exe was not found. Build the game first:
    echo     cmake --build build
    pause
    exit /b 1
)
"build\smw.exe" --datadir data %*

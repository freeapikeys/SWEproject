@echo off
REM ==========================================================================
REM   AURA  --  Airport Unified Resource Administration
REM   Trois Freres Systems Ltd.   SIS 2075 Software Engineering 1
REM
REM   Builds the application with MinGW-w64 gcc (MSYS2 UCRT64).
REM   Usage:   build.bat          release build
REM            build.bat debug    debug build with warnings as information
REM ==========================================================================

setlocal

set GCC_DIR=C:\msys64\ucrt64\bin
if exist "%GCC_DIR%\gcc.exe" set PATH=%GCC_DIR%;%PATH%

where gcc >nul 2>nul
if errorlevel 1 (
    echo.
    echo   gcc was not found on PATH.
    echo   Install MSYS2 and the UCRT64 toolchain, or edit GCC_DIR above.
    echo.
    exit /b 1
)

set OUT=aura.exe
set FLAGS=-O2 -std=c11 -Wall -Wextra -Wno-unused-parameter
if "%1"=="debug" set FLAGS=-g -O0 -std=c11 -Wall -Wextra -Wno-unused-parameter

set SRC=src\main.c ^
 src\engine\canvas.c src\engine\text.c src\engine\anim.c ^
 src\engine\icons.c  src\engine\ui.c ^
 src\core\model.c    src\core\store.c  src\core\sim.c ^
 src\ai\ai_chat.c    src\ai\ai_delay.c src\ai\ai_bagscan.c ^
 src\ai\ai_stand.c   src\ai\ai_flow.c ^
 src\screens\common.c        src\screens\screen_airfield.c ^
 src\screens\screen_board.c  src\screens\screen_checkin.c ^
 src\screens\screen_baggage.c src\screens\screen_flow.c ^
 src\screens\screen_ai.c     src\screens\screen_records.c

echo.
echo   Building AURA ...
if not exist build mkdir build
if not exist data  mkdir data

gcc %FLAGS% -Iinclude -Isrc %SRC% -o %OUT% -mwindows -lgdi32 -luser32 -lmsimg32 -lm
if errorlevel 1 (
    echo.
    echo   BUILD FAILED
    exit /b 1
)

echo   Built %OUT%
echo.
echo   Run it with:  aura.exe
echo.
endlocal

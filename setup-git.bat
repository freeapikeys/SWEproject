@echo off
REM ==========================================================================
REM   AURA -- build a demonstrable Git history
REM
REM   Run this ONCE, in the AURA folder, before you start working as a team.
REM   It creates the repository and lays the project down as a sequence of
REM   commits on feature branches, merged into main with --no-ff so that the
REM   branch structure stays visible in the graph.
REM
REM   The commits are authored by YOU -- pass your name and email:
REM
REM       setup-git.bat "Pranav Mahadoo" "pranav.mahadoo@umail.uom.ac.mu"
REM
REM   Afterwards, each team member sets their own identity with
REM       git config user.name  "Their Name"
REM       git config user.email "their@email"
REM   so their commits are attributed to them.
REM ==========================================================================

setlocal

if "%~1"=="" (
    echo.
    echo   Usage:  setup-git.bat "Your Name" "your@email"
    echo.
    exit /b 1
)

where git >nul 2>nul
if errorlevel 1 (
    echo   git was not found on PATH. Install Git for Windows first.
    exit /b 1
)

if exist .git (
    echo.
    echo   A .git folder already exists here. Delete it first if you want to
    echo   rebuild the history from scratch.
    echo.
    exit /b 1
)

echo.
echo   Initialising repository...
git init -b main >nul
git config user.name  "%~1"
git config user.email "%~2"

REM -------------------------------------------------------------- commit 1 --
git add .gitignore README.md build.bat setup-git.bat
git commit -q -m "Project scaffold: build script, gitignore and README" ^
  -m "Set up the repository for a C project targeting MinGW-w64. The gitignore excludes build output and run-time data from the start so no binary ever enters the history."

REM ------------------------------------------------ branch: render engine ---
echo   Building branch feature/render-engine...
git checkout -q -b feature/render-engine
git add include/theme.h src/engine/canvas.h src/engine/canvas.c
git commit -q -m "Add anti-aliased software rasteriser" ^
  -m "Two rasterisation paths: a signed distance field for rounded rectangles and circles, and a scanline coverage rasteriser with an active edge list for arbitrary paths. Compositing is source-over into a 32-bit DIB section."

git add src/engine/text.h src/engine/text.c
git commit -q -m "Add font cache and text layout over the canvas"

git add src/engine/anim.h src/engine/anim.c
git commit -q -m "Add motion system: easing, springs, timelines, particles"

git add src/engine/icons.h src/engine/icons.c src/engine/ui.h src/engine/ui.c
git commit -q -m "Add vector icon set and immediate-mode widget layer"

git checkout -q main
git merge -q --no-ff feature/render-engine -m "Merge feature/render-engine into main"

REM ------------------------------------------------- branch: domain model ---
echo   Building branch feature/domain-model...
git checkout -q -b feature/domain-model
git add src/core/model.h src/core/model.c
git commit -q -m "Add domain model and day generation for MRU Plaisance" ^
  -m "Airlines, aircraft types, destinations, stands and the airfield geometry, plus generation of a full operating day from the published route structure."

git add src/core/store.h src/core/store.c
git commit -q -m "Add CSV persistence, journal and report export"

git add src/core/sim.h src/core/sim.c
git commit -q -m "Add the live simulation: flight states, taxi movement, baggage flow"

git checkout -q main
git merge -q --no-ff feature/domain-model -m "Merge feature/domain-model into main"

REM --------------------------------------------------- branch: ai engines ---
echo   Building branch feature/ai-engines...
git checkout -q -b feature/ai-engines
git add src/ai/ai.h src/ai/ai_delay.c
git commit -q -m "Add delay prediction by logistic regression" ^
  -m "Chosen over a deeper model because the weights are readable: the engine returns each feature's contribution so a controller can see why a movement scored as it did."

git add src/ai/ai_bagscan.c
git commit -q -m "Add hold baggage classifier: 7-12-8-1 MLP with backpropagation" ^
  -m "Trained on deliberately overlapping classes with label noise. Loss is class-weighted so the model favours recall, because a missed threat costs more than a bag opened unnecessarily."

git add src/ai/ai_stand.c
git commit -q -m "Add stand allocation by simulated annealing"

git add src/ai/ai_flow.c
git commit -q -m "Add flow forecasting: Holt smoothing into an M/M/c queue"

git add src/ai/ai_chat.c
git commit -q -m "Add operations assistant: IDF-weighted intent classification" ^
  -m "Entity extraction runs independently of intent, which is what lets a bare flight number work and lets follow-up questions resolve against the dialogue context."

git checkout -q main
git merge -q --no-ff feature/ai-engines -m "Merge feature/ai-engines into main"

REM ------------------------------------------------------ branch: screens ---
echo   Building branch feature/screens...
git checkout -q -b feature/screens
git add src/app.h src/main.c src/screens/common.c
git commit -q -m "Add application shell: window, frame loop and navigation"

git add src/screens/screen_airfield.c
git commit -q -m "Add live airfield movement display"

git add src/screens/screen_board.c
git commit -q -m "Add movement board with working split-flap display"

git add src/screens/screen_checkin.c src/screens/screen_baggage.c
git commit -q -m "Add check-in hall and baggage handling screens"

git add src/screens/screen_flow.c src/screens/screen_ai.c src/screens/screen_records.c
git commit -q -m "Add terminal flow, AI suite and records screens"

git checkout -q main
git merge -q --no-ff feature/screens -m "Merge feature/screens into main"

REM ------------------------------------------------------ docs and samples --
git add docs
if exist data\samples git add data\samples
git commit -q -m "Add project documentation and sample data" 2>nul

REM -------------------------------------------------------------- tidy up ---
git branch -q -d feature/render-engine
git branch -q -d feature/domain-model
git branch -q -d feature/ai-engines
git branch -q -d feature/screens

echo.
echo   Done. History:
echo.
git log --oneline --graph --decorate --all
echo.
echo   Next steps:
echo     1. Create an empty repository on GitHub (no README, no gitignore).
echo     2. git remote add origin https://github.com/USER/aura.git
echo     3. git push -u origin main
echo     4. Settings -^> Collaborators -^> add your two team mates.
echo.

endlocal

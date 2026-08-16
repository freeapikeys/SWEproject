# AURA — Airport Unified Resource Administration

**Sir Seewoosagur Ramgoolam International Airport (MRU / FIMP), Plaisance, Mauritius**

A native Windows desktop application written entirely in C.
Built for **SIS 2075 Software Engineering 1**, BSc (Hons) Data Science Level 2,
University of Mauritius.

---

## What this is

AURA is an airport operations application. It runs a live simulation of a full
operating day at Plaisance — 72 movements, 14 airlines, 33 destinations, 22
stands, ~360 passengers and ~370 bags — and gives the operations floor seven
working screens over that state, plus five decision-support engines.

It is a real Win32 application: one window, one 32-bit back buffer, and a
software rasteriser written from scratch. There is no game engine, no web
view, no Electron, and no third-party graphics, UI or machine-learning
library anywhere in the build. The only things linked are the Windows
system libraries (`gdi32`, `user32`, `msimg32`) and the C maths library.

```
     45 source files          ~11,600 lines of C
     0 external dependencies  1 executable, ~440 KB
```

---

## Building

Requires **MinGW-w64 GCC** (MSYS2 UCRT64 toolchain).

```bat
build.bat
```

Then run `aura.exe`. That is the whole build — no configure step, no package
manager, no lock file.

The script adds `C:\msys64\ucrt64\bin` to `PATH` automatically. If GCC lives
somewhere else, edit `GCC_DIR` at the top of `build.bat`.

To build by hand:

```bat
gcc -O2 -std=c11 -Iinclude -Isrc src/main.c src/engine/*.c src/core/*.c ^
    src/ai/*.c src/screens/*.c -o aura.exe -mwindows -lgdi32 -luser32 ^
    -lmsimg32 -lm
```

---

## Using it

| Key | Action |
|-----|--------|
| `1`–`7` | switch screen |
| `Space` | pause / resume the simulation clock |
| `F` | cycle clock speed (×1 → ×4 → ×8 → ×20 → ×60) |
| `A` | open the AURA assistant dock |
| `S` | write all records to disk |
| `Esc` | close the open panel |

The mouse works everywhere: drag the airfield to pan, scroll to zoom, click an
aircraft or a stand to select it, click a bag on the belt to trace it, click a
check-in desk to open or close it.

---

## The seven screens

**Airfield** — a top-down movement display of Plaisance drawn from real
geometry: one runway 14/32 (3,390 m), the parallel taxiway, the link taxiways,
the terminal frontage and 22 stands. Aircraft are placed by sampling the same
taxi routings the simulation drives them along, so what you see is the state
of the operation, not an animation playing over it.

**Movements** — arrivals and departures on a working split-flap board. Each
character sits in its own cell holding the glyph shown and the glyph it is
trying to reach; when they differ the cell rolls forward through the alphabet
one flap at a time. Because the board is driven from live flight state, a
status changing in the simulation makes the relevant cells physically flip.

**Check-in** — the 24-position desk bank with live queues, a searchable
passenger roster, a drawn boarding pass (including a barcode derived from the
booking reference), and a cabin plan built from the aircraft type.

**Baggage** — the hold baggage system: four check-in inputs merging onto the
main line, the screening tunnel, the diverter, the sortation loop and three
make-up carousels. Every bag on screen is a record in the register, and the
decision at the diverter is the live output of the neural classifier.

**Terminal Flow** — central search drawn as individual passengers queueing
through the archways. Lane count is driven by the queue model; close a lane
and the hall visibly backs up.

**AI Suite** — the five engines, each showing its own internals.

**Records** — the file layer. Each card is a real file under `data/`: its size
is read from disk, writing serialises live state, reading restores it.

---

## The five engines

All five are implemented in C and train on the machine at start-up. Nothing
calls out to a service; the application runs with the network cable out.

| # | Engine | Method | Measured result |
|---|--------|--------|-----------------|
| 1 | **Operations Assistant** | bag-of-words intent classification, IDF-weighted over a 27-intent corpus, with independent entity extraction and a dialogue context stack | 27 intents, resolves bare flight numbers and follow-up questions |
| 2 | **Delay Oracle** | logistic regression, batch gradient descent, L2 regularised, 8 features | 90.9 % accuracy, log loss 0.263 |
| 3 | **BagScan Neural** | 7-12-8-1 multilayer perceptron, backpropagation with momentum, class-weighted loss | 92.2 % accuracy, 83 % recall, 84 % precision |
| 4 | **Stand Allocator** | simulated annealing with relocation and swap moves, lexicographic feasibility | recovers a scrambled plan to 0 conflicts |
| 5 | **Flow Forecast** | Holt double exponential smoothing into an M/M/c queue | forecast MAPE reported live |

Three deliberate design decisions worth defending in the report:

- **The delay model is a logistic regression, not something deeper.** The
  weights are readable, so the model hands back the contribution of each
  feature and a duty manager can argue with it. A black box that says "68 %"
  and nothing else does not get used.

- **BagScan is trained on deliberately overlapping classes with label noise.**
  Cleanly separable training data trains to 100 % and teaches the network
  nothing about the false-alarm rate a screening hall actually lives with.
  The loss is class-weighted (positive examples count 3.4×) because a missed
  threat costs far more than a bag opened unnecessarily.

- **The stand allocator treats feasibility lexicographically.** Hard
  violations — a double-booked stand, an aircraft too large for its stand —
  are counted, not priced. Pricing them lets a large enough pile of soft
  savings buy an impossible plan, which is exactly the trade an annealer will
  find if you let it.

---

## Module structure

The project is split so that each module has one clear owner and a header that
is its contract. This is the division used for the group work.

```
include/theme.h              design tokens: the single source of truth for colour

src/engine/                  MODULE A — presentation engine
  canvas.h/.c                anti-aliased software rasteriser
  text.h/.c                  font cache and text layout
  anim.h/.c                  easing, springs, timelines, particles
  icons.h/.c                 76 hand-built vector icons
  ui.h/.c                    immediate-mode widget layer

src/core/                    MODULE B — domain and persistence
  model.h/.c                 airlines, aircraft, airports, stands, flights,
                             passengers, bags, staff; day generation
  store.h/.c                 CSV persistence, journal, report export
  sim.h/.c                   the live simulation

src/ai/                      MODULE C — decision support
  ai.h                       the contract for all five engines
  ai_chat.c                  intent classification and entity extraction
  ai_delay.c                 logistic regression
  ai_bagscan.c               multilayer perceptron
  ai_stand.c                 simulated annealing
  ai_flow.c                  Holt smoothing + M/M/c queue

src/screens/                 MODULE D — the application screens
  common.c, screen_*.c       one file per screen

src/app.h, src/main.c        application shell: window, loop, navigation
```

### Why the rasteriser is written by hand

The brief asks for a solution in C. Reaching for a graphics library would have
answered the letter of that and skipped the interesting part. `canvas.c`
implements two rasterisation paths:

1. A **signed-distance-field rasteriser** for rounded rectangles and circles.
   These are most of the interface, and an SDF gives mathematically exact
   anti-aliasing at a fraction of the cost of tessellating the shape.

2. A **scanline coverage rasteriser** with an active-edge list and 5×
   vertical supersampling with exact horizontal span coverage, non-zero
   winding, for arbitrary paths: the airfield, aircraft, icons and charts.

Compositing is source-over into a 32-bit BGRA DIB section whose device context
is shared with GDI, which is what lets text and vector art interleave freely.

---

## Files written by the application

Everything is plain comma-separated text under `data/`. Plain text was chosen
over a binary dump deliberately: the files can be opened, diffed and reviewed
in the repository, which is what makes them useful evidence.

| File | Contents |
|------|----------|
| `flights.csv` | movement schedule with stand, gate, times, live state, delay risk |
| `passengers.csv` | roster: booking reference, seat, bags, acceptance status |
| `baggage.csv` | every bag, its owner, position in the system, screening score |
| `staff.csv` | duty roster |
| `journal.log` | append-only audit trail |
| `daily_report.txt` | formatted end-of-day operations report |

`store.c` both writes and reads these back — the Records screen restores
saved state into the running application, so persistence is round-trip, not
write-only.

---

## Data provenance

Route structure, airlines, aircraft types, block times and the airport's
physical layout follow the real operation at Plaisance: the early-morning
European long-haul arrival bank, the regional wave through the middle of the
day (Rodrigues, Réunion, Antananarivo, Mahé), and the outbound long-haul stack
after 20:00. Passenger and staff names are drawn from name pools reflecting
the Mauritian population. Nothing in the application contains real personal
data.

---

## Known limitations

- The screening feature vectors are generated, not read from real tomography
  equipment. The classifier is real; the sensor is simulated.
- Weather is a smooth noise process, not a METAR feed.
- The stand allocator's cost weights (bussing, walking distance, tow cost)
  are estimates, not calibrated against Plaisance's own figures.
- Single-runway operation only; no de-icing, no slot coordination.

---

*Trois Frères Systems Ltd.*

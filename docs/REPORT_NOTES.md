# AURA — material for the report

Notes to write the SIS 2075 report from. This is not the report; it is the
evidence and the reasoning behind each decision, so the report can be written
in your own words with something real to say.

---

## 1. The problem chosen

Rather than take one of the suggested proposals, we chose an airport
management system for **Sir Seewoosagur Ramgoolam International Airport
(MRU / FIMP)** at Plaisance. The reasons are worth stating in the report:

- It is genuinely modular. An airport separates cleanly into airside
  movements, the terminal, baggage and resource allocation, so the work
  divides between three developers without artificial seams.
- It has real data behind it: a published route structure, real aircraft
  types with real wingspans, and a physical layout with hard constraints.
  That makes the system testable against something other than opinion.
- It has genuine decision problems in it — which stand, how many lanes, which
  bag to search — so the decision-support engines have real work to do rather
  than being decoration.

---

## 2. Modules and ownership

The brief asks for the project to be broken into modules assigned to members.
Each module is a folder with a header that acts as its contract; the modules
only meet at those headers.

| Module | Files | Lines | Owner |
|---|---|---|---|
| **A — Presentation engine** | `src/engine/*` (canvas, text, anim, icons, ui) | ~3,400 | Member 1 |
| **B — Domain and persistence** | `src/core/*` (model, store, sim) | ~2,200 | Member 2 |
| **C — Decision support** | `src/ai/*` (five engines) | ~1,900 | Member 3 |
| **D — Screens and shell** | `src/screens/*`, `src/main.c` | ~4,100 | All three |

Interfaces were agreed **before** implementation began. `canvas.h` was fixed
first, which let the screens be written against a rasteriser that did not
exist yet; `ai.h` was fixed second, which let the AI Suite screen be built
while the models were still being trained.

This is the practical argument for header-first design in C, and it is worth
a paragraph: a header is a contract, and once it is agreed, two people can
work on either side of it at the same time.

---

## 3. Use of the required C features

**Own libraries (`.h` files).** Fourteen headers, each the public interface to
one translation unit. Every one uses include guards. Notable examples:

- `canvas.h` — the drawing contract: types (`Color`, `Paint`, `Path`,
  `Canvas`) and about 40 functions. Nothing outside `canvas.c` touches pixels.
- `ai.h` — the contract for all five engines, including the structs that
  carry their trained state and measured metrics.
- `model.h` — the domain: 12 structs and the `World` that holds the whole
  operation.

**Functions.** Roughly 340 across the project. Internal helpers are `static`
so they do not leak into the global namespace — worth mentioning as a
deliberate use of C's translation-unit scoping.

**Files.** `store.c` implements both directions. Writing serialises live state
to CSV; reading parses it back into the running application. The Records
screen exercises both, so persistence is demonstrably round-trip rather than
write-only. There is also an append-only `journal.log` and a formatted
`daily_report.txt`.

**Structs, enums, unions of behaviour.** `FlightState` and `BagState` are
enumerations driving state machines in `sim.c`. Function pointers were
deliberately avoided; the screen router is a `switch`, which is easier to
follow and to debug.

---

## 4. Testing — what was actually found

Testing was not a formality on this project. Every one of these was a real
defect found by testing and fixed, and each makes a good paragraph because it
shows the process working.

| # | Defect | How it was found | Fix |
|---|---|---|---|
| 1 | Every colour rendered wrong — yellow background, green circles | Rendered one frame to a BMP and looked at it | `COL_R/G/B/A` returned `unsigned`, so `COL_R(b) - COL_R(a)` wrapped to ~4 billion instead of going negative. Made the accessors return `int` and clamped in `col_mix` |
| 2 | Thick curves drew as dashed lines | Visual inspection of the smoke test | Stroke quads and round joins had opposite winding, so under non-zero fill the join cancelled the quad. Made the join wind the same way |
| 3 | 43 of 376 bags flagged for manual search, most scoring 1.00 | Headless test harness printing the flag rate | Live feature vectors were drawn from a different distribution than the training set. Moved both to one shared `FEAT_RANGE` table |
| 4 | Classifier reported 100 % on everything | Same harness | Training classes were perfectly separable. Widened the ranges to overlap and added 4.5 % label noise |
| 5 | Recall (79 %) lower than precision, contradicting the stated design | Reading the numbers against the design intent | Moving the threshold barely helped once outputs saturated. Added class-weighted loss (positive examples ×3.4) |
| 6 | Half the flights had no stand | Headless allocator test | An arrival and its departure were given **different** registrations, so turnaround pairing matched by coincidence. One registration per rotation |
| 7 | Air Mauritius rotations still mispaired | Printing the per-flight breakdown | `make_reg` drew MK tails from a pool of only 8 for ~19 rotations. Widened the pool |
| 8 | Optimiser returned plans *worse* than its input | Comparing greedy vs annealed cost | The cost function counted a turnaround as a conflict with itself; hard violations were priced, so soft savings could buy an impossible plan. Made feasibility lexicographic and added a guard |
| 9 | Stat tile captions overlapped their values | Screenshot review | Fixed layout at a fixed height; tiles are reused at several heights. Made the layout compute from the tile height |

**Test method.** Because the application is a GUI, two harnesses were built:

1. `smoke.c` — renders one frame to a BMP so the rasteriser can be inspected
   pixel by pixel without launching the app.
2. `chattest.c` / `standtest.c` — link the core and AI modules into a console
   program, run the models and print measured metrics. This is what caught
   defects 3–8, none of which are visible by clicking around.

That separation — engine and logic testable without the interface — is worth
making explicitly in the report. It is the practical payoff of keeping the
modules independent.

---

## 5. Measured results

Reproduce with the console harness; these are measured, not asserted.

```
delay model : accuracy 90.9 %   log loss 0.263   900 samples   640 epochs
bag network : accuracy 92.2 %   recall 83 %   precision 84 %   1600 samples
              flag rate in service 6-10 %, consistent with level 1 screening
stand plan  : 72 movements, 26 stands, 0 conflicts, 0 span violations,
              0 unassigned; annealer recovers a scrambled plan to 0 conflicts
```

Note in the report that BagScan's recall sits at roughly the ceiling the
4.5 % label noise allows — a model reporting 100 % on noisy labels would be
evidence of a broken evaluation, not a good classifier.

---

## 6. Software development lifecycle

Map the work onto the phases the brief describes:

- **Requirements** — the brief plus the constraint that it must be C, and the
  self-imposed constraint of no external libraries.
- **Design** — headers first. `canvas.h`, `ai.h` and `model.h` were written
  and agreed before their implementations.
- **Implementation** — iterative, one module at a time, each on its own
  branch.
- **Testing** — the two harnesses above, run after every change to a model.
- **Integration** — feature branches merged into `main` with `--no-ff`, so
  the history shows where each piece came from.
- **Deployment** — a single `build.bat` producing one self-contained
  executable with no runtime dependencies.

The Agile argument the brief asks about is easy to make honestly here: the
stand allocator took **four** attempts. Each attempt shipped, was measured,
and was found wanting on evidence; the requirement itself was refined as the
model taught us what "a good plan" meant. A waterfall approach would have
specified the cost function once, at the start, when we knew least.

---

## 7. Suggested demo running order

1. Launch. The splash trains the models — say so out loud; that is real work,
   not a progress bar.
2. **Create an account** — registration is open, so do it live in front of
   them. Say out loud that no password is stored anywhere: the digest is a
   salted SHA-256 iterated a hundred thousand times, and that iteration count
   is why the button takes a noticeable moment. Tick *Airport staff* so it
   lands on the operations floor.
3. **Operations -> Gates** — the strongest single feature. Put a flight on a
   gate that is already taken; the clash appears in red on the Gantt and in
   the panel beside it, named, timed, and with a gate that would clear it.
   Press the button and watch the conflict count go to zero. Then say that a
   *delayed* flight produces the same clash on its own, which is the point.
4. **Operations -> Runway** — one runway, so it is the hard limit on the
   airport. The published timetable is slot-coordinated, so every conflict
   shown is caused by a delay somebody can act on.
5. **Emergency** — raise an incident. Severity decides where it sits in the
   list, what the protocol says, and how long it may go unacknowledged; the
   badge appears on the sidebar from wherever you are. Passengers are told
   automatically when a gate moves or a flight is cancelled.
6. **Analytics** — every number is recomputed from the live world rather
   than stored, so it cannot drift from the screens it describes.
3. **Airfield** — pan and zoom, click a moving aircraft, note that its routing
   is the same one it is being driven along. The clock is the real one, so
   pick a movement that is happening now; the topbar says how many are moving.
4. **Movements → Schedule** — page forward through the next ninety-two days.
   None of it is stored: each date seeds its own generator, so the same date
   always produces the same programme.
5. **Movements** — let a status change and point out the flaps physically
   turning.
6. **Baggage** — follow a bag from check-in through the screening tunnel and
   watch the live scan readout produce the score at the diverter.
7. **AI Suite → BagScan** — the network with live activations; press *Scan
   another* to run a different bag through it.
8. **AI Suite → Stand Allocator** — press **Scramble**, show the wreckage on
   the Gantt, then **Run optimiser** and show it recovered. This is the
   strongest single moment in the demo.
9. **AI Suite → Assistant** — ask "where is MK046", then "what gate?" to show
   the follow-up resolving against context.
10. **Records** — write the files, then open `data/flights.csv` in VS Code
   beside the app.
11. **Git** — `git log --oneline --graph --decorate --all`, then the GitHub
   Network graph.

---

## 8. Honest limitations

Include these. Naming your own limitations is what separates a report from a
brochure, and markers reward it.

- Screening feature vectors are generated, not read from tomography equipment.
  The classifier is real; the sensor is simulated.
- The delay model trains on a generated history whose structure we chose. It
  demonstrates the method correctly but its weights are not evidence about
  the real Plaisance operation.
- Stand allocator cost weights (bussing, walking distance, tow cost) are
  estimates.
- Single-runway operation only; no slot coordination, no de-icing, no
  cargo handling beyond two parking positions.
- The rasteriser is single-threaded and CPU-only. It holds a comfortable
  frame rate at 1440×900 — the Movements board is the heaviest screen, at
  around 28 fps, because GDI rasterises roughly 800 individual glyphs a
  frame; a glyph atlas would fix that. It would not scale to a 4K display
  without work.
- The security queue is a real FIFO with a priority lane, but the arrival
  process is a per-tick probability rather than a fitted distribution.
- Gate conflict detection assumes the occupancy windows in `ops_gate_window`.
  They are reasonable -- a widebody holds a gate longer than a turboprop --
  but they are estimates, not Plaisance's published turnaround times.
- Reviews are written for this application. They are not real passengers'
  words lifted from a real company's website, which would be passing off
  somebody else's writing and their name.
- **The account system protects the application, not the machine.** The
  password handling is done properly — per-account salt, SHA-256 iterated
  100,000 times, constant-time comparison, no password stored anywhere — but
  anybody with the computer can delete `data/accounts.dat` or rebuild from
  source. It is an operational control, in the way an office door lock is;
  it is not a security boundary.
- It is **not** federated sign-in. Google or Microsoft SSO needs a browser,
  a network round trip and a client secret, none of which a self-contained C
  application with no third-party libraries has. A screen that imitated
  Google while collecting a Google password would be a phishing page, so the
  gate is deliberately AURA's own and is labelled as such.
- The three-month programme is generated, not published by the airport. The
  route list, block times and turnarounds are taken from the real boards, but
  the weekly frequencies are derived from a hash of the flight number rather
  than from a filed schedule.

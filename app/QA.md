# Interval Coach — QA plan

How Interval Coach is tested before it is called done. The behaviour under test
is [SPEC.md](SPEC.md) (v1, including pause).

Sources: `$UNA_SDK/Docs/platform-overview.md`, `Simulator.md`, `unit-testing.md`,
`SensorsLayer.md`, `touchgfx-widgets.rst` (+ `Templates/TouchGFX-Widgets/docs/Buttons.md`),
`figma-ui-kit.rst`, `service-lifecycle.md`, and the Running example
(`Docs/Examples/Running-Architecture.md`, `Examples/Apps/Running`).

## Definition of done

The app is reported done only when all three gates pass in one run, on the
same commit:

| Gate | Pass condition | How it is checked |
|---|---|---|
| **G1 Host tests** | Every GoogleTest in `Tests/Host` is green. | `ctest` exit code 0, output pasted in the report |
| **G2 Simulator** | Every check in section B is marked PASS, with its evidence. | Checklist in section B, screenshots and log excerpts |
| **G3 Watch build** | The `.uapp` builds, and **0 compiler warnings come from Interval Coach sources**. The only warnings allowed are the known SDK baseline (below). | `qa/check_warnings.ps1` exit code 0 |

**SDK warning baseline.** Measured 7-nov-2026 on a clean build of the stock
Running example (SDK `apps-v1.6.2`, ST GCC 14.3.1):

- 414 linker warnings `Forcing branch to absolute symbol in Thumb mode`. These
  come from the SDK's shared-libc / position-independent design, and every app
  gets them.
- 1 compiler warning in `una-sdk/Libs/Source/Fit/RecordingMarker.cpp` (`-Wformat`).

`check_warnings.ps1` fails on any warning that is not one of those two kinds,
and on any warning whose path is under `IntervalCoach/`. The SDK is not edited
and no warning flags are suppressed.

**Not part of done, reported separately:** W1, the session continuing in the
background on a real watch. It stays *pending* until a watch is available, and
the report says so; it is never claimed from the simulator.

---

## 0. Code layout that makes this testable

The interval rules live in one plain C++17 class with **no SDK, kernel or
TouchGFX includes**, so GoogleTest drives it directly and quickly:

```
Software/Libs/
  Header/IntervalEngine.hpp   pure logic: state machine + summary
  Header/SettingsStore.hpp    load / save / clamp settings.json
  Header/Lifecycle.hpp        shouldExit(sessionActive, guiUp, graceOver)
  Sources/...
  Header/Service.hpp          thin adapter: sensors → engine, events → vibro/buzzer/backlight/GUI
```

- **Inputs** to `IntervalEngine`: `start(settings, nowMs)`,
  `onTick(nowMs)`, `onGps(nowMs, fix, speedMps, odometerM)`,
  `onHeartRate(nowMs, bpm, trust)`, `onButton(nowMs, Button)`.
  Time is passed in, never read, so a 5-minute rest runs in microseconds and
  there are no sleeps in tests.
- **Outputs**: `state()` (SETUP, GPS_WAIT, REP, PAUSED, REST, CONFIRM_END,
  SUMMARY), `view()` (the numbers each screen shows), `takeEvents()` (REP_DONE,
  GO, SESSION_DONE, GPS_LOST, GPS_BACK, PAUSED, RESUMED, in order), and
  `summary()`.
- `Service` holds no rules of its own. It maps each engine event to the
  vibration/buzzer/backlight pattern in SPEC §4, and logs one line per event:
  `I- IC.Event: REP_DONE rep=2`. Section B relies on these lines.

**Definitions the tests pin down** (taken from SPEC.md and Running):

| Term | Meaning |
|---|---|
| Valid HR sample | `trust` 1–3 and `bpm > 20` (Running's rule, `Running-Architecture.md` §6) |
| HR below target | `bpm < target`, strictly. A sample equal to the target does **not** count. |
| Rest ends by HR | Valid HR below target for **3 consecutive seconds** (an invalid or ≥ target sample resets the count), and rest ≥ **15 s**. |
| GPS lost | `fix == false`, or no GPS sample for 2 s (samples come every 1 s, as in Running) |
| GPS gap flag | Set on a rep only when a loss **lasts more than 10 s** (SPEC §5 / criterion 11) |
| Odometer re-baseline | On the first fixed sample after a loss, the distance baseline is reset, so the odometer jump covering the lost stretch is **not** added. Only bridged distance counts. |

### Host test build

The tests are a separate CMake project in `IntervalCoach/Tests/Host/`. It
reuses the SDK harness **by path, without editing the SDK**:
`$UNA_SDK/Tests/Host/cmake/FetchGoogleTest.cmake` (GoogleTest pinned to commit
`52eb810`) and `$UNA_SDK/Tests/Host/support/KernelTestDoubles.*`
(`KernelFixture`, `InMemoryFileSystem`).

- Flags: `-Wall -Wextra -Wpedantic -Werror` on Interval Coach sources (`/W4 /WX` under MSVC).
- **Compiler: the MinGW g++ 13.2 that ships with TouchGFX**
  (`D:\TouchGFX\4.26.1\env\MinGW\bin`), plus a test-only link shim.
  - The problem: on 7-nov-2026 any GoogleTest binary built with that MinGW,
    the SDK's own suite included, failed to link with `undefined reference to
    '_imp__quick_exit'` and `'_imp___Exit'`, from
    `libmingw32.a(ucrt_exit_wrappers.o)`. That MinGW build has a C-runtime
    mismatch.
  - The fix: `Tests/Host/support/mingw_crt_shim.c` supplies both imports,
    routes them to `_exit`, and is compiled only for 32-bit MinGW. It is never
    part of the `.uapp`.
  - WinLibs and MSVC were considered and are not needed.
- **Non-vacuous check:** two engine mutations were tried and each was caught
  by the expected tests, then reverted. Changing `<` to `<=` for the HR target
  failed 2 tests; a 20 s GPS bridge instead of 10 s failed 3.

```powershell
pwsh -File qa\run-host-tests.ps1     # configure + build + ctest (uses TouchGFX MinGW)
```

Windows PowerShell 5 blocks unsigned scripts by default, so use `pwsh` (or
`powershell -ExecutionPolicy Bypass -File …`).

---

## A. Host unit tests (GoogleTest)

The test IDs are the GoogleTest names. **A1–A5 are the cases you asked for;
A6–A10 cover the rest of SPEC.md.** Unless a test says otherwise it uses
`reps=3, distance=400 m, HR target=120, max rest=2:00`, GPS fixed at 4 m/s,
and one sample per second.

### A1 `GpsLoss.*`: GPS lost mid-rep
| Test | Given → When → Then |
|---|---|
| `ShortLossIsBridged` | Rep at 200 m, 4 m/s → GPS lost 6 s → distance rises to 224 m (6 × 4), no GPS_LOST event, no gap flag. |
| `LongLossFreezesAfter10s` | Rep at 200 m → GPS lost 25 s → distance stops at 240 m (10 s bridged), exactly 1 GPS_LOST at t+10 s, gap flag set. |
| `FixReturnResumesWithoutJump` | After `LongLoss…`, fix returns with the odometer 100 m ahead → 1 GPS_BACK; the distance continues from 240 m and does not jump to 340 m. |
| `BridgeCanFinishRep` | Rep at 380 m → GPS lost → REP_DONE fires during the bridge at 400 m. |
| `LossDuringRestIsIgnored` | GPS lost all rest → no GPS_LOST/GPS_BACK, no flags; rest ends normally. |
| `RepStartsWithoutFix` | GO with no fix → distance stays 0 (no last speed carried over from rest), GPS_LOST after 10 s. |
| `SingleDroppedSamples` | 2% random single-sample dropouts over a 400 m rep (seeded) → the distance error is under 1%, no GPS_LOST. Mirrors the simulator's own dropouts. |

### A2 `HrAtTarget.*`: heart rate hovering at the target
| Test | Given → When → Then |
|---|---|
| `EqualNeverEndsRest` | HR exactly 120 for the whole rest → no end by HR; ends at 2:00 with reason MAX. |
| `ThreeSecondsBelowEnds` | From t=20 s: 121, 120, 119, 118, 117 → rest ends on the 3rd sample below (117), reason HR, exactly 1 GO. |
| `ShortDipDoesNotEnd` | 119, 118, 121, 119, 118, 121 … (never 3 below in a row) → no end by HR; ends at MAX. |
| `LowTrustResetsCount` | 119, 118, then 110 with trust 0, then 119 → the count restarts; ends only after 3 more valid samples below. |
| `OneGoOnly` | HR keeps dropping after the end → exactly 1 GO, and the next rep is running. |
| `MinimumRest15s` | HR 100 from the first second → rest ends at exactly 15 s, reason HR. |
| `BelowJustBefore15s` | Below from t=13 s → 3 s reached at t=15 s → ends at 15 s. |

### A3 `MaxRest.*`: heart rate never drops
| Test | Given → When → Then |
|---|---|
| `HighHrEndsAtMax` | HR 150 throughout → rest ends at 120 s (± 1 tick), reason MAX, 1 GO. |
| `NoHrEndsAtMaxFlagged` | No HR samples → ends at 120 s, reason **no HR**; the view shows `HR --`. |
| `HrLostMidRest` | Valid 140 for 30 s, then nothing → ends at 120 s, reason no HR. |
| `MaxRestBounds` | max rest 0:30 and 5:00 → ends at exactly 30 s / 300 s. |

### A4 `Pause.*`: pause and resume
| Test | Given → When → Then |
|---|---|
| `PauseFreezesDistanceAndTime` | Rep at 100 m, 20 s → R1 → PAUSED; 30 s of GPS samples (120 m covered) → distance 100 m, rep time 20 s. |
| `ResumeContinues` | R1 again → RESUMED; the next samples add from 100 m, and the rep time continues from 20 s. |
| `PausedTimeExcludedFromTotals` | Rep with a 30 s pause → summary rep time and total session time both exclude the 30 s. |
| `NoBridgeWhilePaused` | GPS lost during a 20 s pause → no GPS_LOST, no gap flag. |
| `R1InRestDoesNotPause` | R1 during rest → SKIP (SPEC §3), never PAUSED. |
| `EndConfirmFromPause` | Paused → R2 → CONFIRM_END; R2 → back to PAUSED, values unchanged. |

### A5 `LastRep.*`: last rep shows the summary
| Test | Given → When → Then |
|---|---|
| `LastRepGoesToSummary` | reps=2 → full run → events in order `REP_DONE, GO, SESSION_DONE`; final state SUMMARY; no REST after rep 2. |
| `SummaryContents` | Same run → 2 rep rows (time, pace, avg HR), 1 rest row (length, reason). |
| `SingleRep` | reps=1 → `SESSION_DONE` straight from rep 1, 0 rest rows. |
| `EarlyEndKeepsCompletedReps` | reps=4, end confirmed in rep 3 → summary has 2 completed reps; the partial rep 3 is not listed (SPEC: "reps done so far"). |

### A6 `Rest.*`: skip and GO
- `SkipEndsRestAsSkip`: R1 in rest → reason SKIP, 1 GO, rep n+1 starts at 0 m.

### A7 `Setup.*`: setting editing
- `DefaultsWhenNoFile`: 6 / 400 / 120 / 2:00.
- `ClampAtLimits`: L1 at a maximum and L2 at a minimum do not change the value (no wrap), for all 4 fields.
- `StepSizes`: one press = 1 rep / 100 m / 5 bpm / 15 s.
- `StartNeedsFix`: in GPS_WAIT, R1 without a fix does nothing; with a fix it starts rep 1.

### A8 `SettingsStore.*` (uses `KernelFixture` + `InMemoryFileSystem`)
- `RoundTrip`: save → load returns the same 4 values.
- `CorruptFileGivesDefaults`: seeded garbage JSON → defaults, no crash, file rewritten on the next save.
- `OutOfRangeValuesClamped`: seeded `reps: 99` → 20.

### A9 `Lifecycle.*`: the session survives the GUI closing
- `StaysWhileSessionActive`: `shouldExit(active=true, guiUp=false, grace=true)` → false.
- `ExitsWhenIdleAndNoGui`: `(false, false, true)` → true.
- `NeverExitsBeforeGrace`: `(false, false, false)` → false (startup guard, `service-lifecycle.md` §5.2).
- `EngineAdvancesWithoutGui`: the engine runs a full rep and rest with no GUI attached and gives the same events as with one, so the session does not depend on GUI ticks (§6).

### A10 `Summary.*`: numbers
- `PaceFormat`: 400 m in 1:36 → `4:00 /km`.
- `AvgHrIgnoresInvalid`: trust-0 samples are not averaged.
- `TotalTimeIncludesRests`: total = reps + rests, minus pauses.

---

## B. Simulator checks

**Builds:** the app's own simulator, built the way Designer's F5 does it
(`make -f simulator/gcc/Makefile` with the TouchGFX MinGW and Ruby on `PATH`).

**Simulated sensors**, set in `simulator/ConfigurationSimulator.hpp` (SDK
mocks, `Docs/Simulator.md`): GPS 15–25 km/h with a 4 s fix, and HR 90–150 bpm,
so HR crosses the 120 bpm default target.

**Fault keys** (simulator only, compiled out of the watch build; G3 greps the
watch map files for any QA symbol). They replace the separate sensor-profile
builds planned first:

| Key | Toggles | Used for |
|---|---|---|
| `6` | GPS off / on in the service's GPS input | GPS wait with no fix; loss longer than 10 s (the stock mock only drops single samples) |
| `7` | HR off / on | `HR --` and rests that end with reason "no HR" |
| `8` | HR +60 bpm | HR that never drops below the target, so rest ends at MAX |

**Driving the simulator:** `pwsh -File qa/run-all-sim.ps1` builds the
simulator, runs every script below and applies the checks, writing
`qa/out/sim-report.txt` and a contact sheet `qa/out/sim-shots.png`.
- **Script mode.** The app's `simulator/main.cpp` replays `IC_QA_SCRIPT`:
  - key presses 250 ms apart, because the HAL keeps one key per frame;
  - `until <STATE>` waits for a screen state, so runs don't depend on the
    simulated running speed;
  - screenshots are taken on the GUI thread;
  - it logs to `IC_QA_LOG` and quits at the end.
- **Working folder.** The simulator runs from `qa/out/sim/l1/l2/l3/l4/l5`,
  because the SDK mock file system sits at `../../../../../Output/` from the
  working directory. That keeps `settings.json` in `qa/out/sim/Output`.
- **Scripts:**
  - `session.txt`: normal session, 2 × 100 m
  - `faults.txt`: keys 6/7/8, early end
  - `lifecycle.txt`: B4
  - `persist-1/2/3.txt`: B5
  - `store.txt`: C

### B1 Every screen fits the round display and reads at a glance
Screens: Setup, GPS wait (no fix / fix), Rep, Paused, Rest, End session?,
Summary (top and scrolled), and the GPS LOST state.

- **Fits the circle.** The `qa/check_round.py` script runs on each screenshot.
  It fails if any non-black pixel lies outside the circle of radius 120 centred
  on the 240×240 frame, or if any text or number pixel lies outside radius 108.
  The only pixels allowed between radius 108 and 120 are the button-hint arcs
  (radius 113, at the angles in `Buttons.md`).
- **Glance rules**, checked by script where possible, otherwise by me on the
  screenshot:
  - One primary value per screen (meters left / HR / reps), in Poppins SemiBold
    ≥ 35 px.
  - Secondary values ≥ 18 px, at most 3 per screen.
  - Only the UI kit palette on black: `#C0C0C0` white, `#C08000` amber,
    `#C00000` red.
  - No truncated text at the widest values (`2000 m`, `20/20`, `5:00`, `HR 180`).
- **Readability.** I judge each screenshot at 100% size and say for each one
  what it reads as in one look. The screenshots are attached to the report so
  you can disagree.

### B2 Button hints match what each button does
Expected arcs (Buttons widget: hidden / white = neutral / amber = primary /
red = destructive):

| Screen | L1 | L2 | R1 | R2 |
|---|---|---|---|---|
| Setup | white | white | amber | white |
| GPS wait, no fix | — | — | — | white |
| GPS wait, fix | — | — | amber | white |
| Rep | — | — | white (pause) | red (end) |
| Paused | — | — | amber (resume) | red |
| Rest | — | — | amber (skip) | red |
| End session? | — | — | red (yes, end) | white (no) |
| Summary | white if it can scroll up, else — | white if it can scroll down, else — | white | white |

- `check_round.py --hints` samples the pixel colour at each arc's mid-angle and
  compares it with the table.
- Then, for every screen, every button is pressed in the script. A button with
  no hint must do nothing: the state is unchanged in the log. A button with a
  hint must do what SPEC §3 says.

### B3 Vibration patterns appear in the log
On the `session.txt` run (2 reps), the log must contain, in order, each event
line followed by its mock lines (`Mock.Vibro`, `Mock.Buzzer`, `Mock.Backlight`):

| Event | Required log |
|---|---|
| REP_DONE | `IC.Event: REP_DONE` + Vibro 2 notes (1 s effect, pause, 1 s) + Buzzer play + Backlight `on … timeout = 5000` |
| GO | `IC.Event: GO` + Vibro 3 strong clicks + Buzzer play + Backlight on |
| SESSION_DONE | `IC.Event: SESSION_DONE` + Vibro 1 long + 3 short, **no** Buzzer + Backlight on |
| GPS_LOST / GPS_BACK | key `6` run: Vibro 1 soft bump each |
| PAUSED / RESUMED | Vibro 1 soft bump each |

`qa/check_log.py` checks the order and the counts: exactly 1 REP_DONE, 1 GO and
1 SESSION_DONE for 2 reps, and no Buzzer line on SESSION_DONE.

### B4 The session keeps running with the app closed
- **Simulator limit.** In the simulator, closing the window stops both
  processes: `AppCore.cpp` sends `COMMAND_APP_NOTIF_GUI_STOP` to the service,
  then `COMMAND_APP_STOP` about 20 ms later. It has no key to suspend the GUI.
  So the simulator can only show the decision, not the session surviving.
- **Simulator check.** Close the window mid-rep. The log must show
  `IC.Life: GUI_STOP, session active, staying` **before** the `APP_STOP` line,
  and must **not** show the service starting its own exit.
- **Host proof.** A9 shows the exit rule keeps the service up, and that the
  engine advances with no GUI attached.
- **W1, on the watch (pending until a watch is on hand):**
  1. Start a session.
  2. Leave the app with the watch's own navigation mid-rep.
  3. Keep moving for 2 min.
  4. Expect the REP DONE / GO vibrations while away.
  5. Reopen the app: it shows the live state, not a fresh one.

### B5 Settings survive a restart
1. Set 8 reps / 600 m / 135 bpm / 3:15, then go to GPS wait (this saves).
2. Exit the app (R2 on the first Setup field).
3. Start `simulator.exe` again. Setup must show 8 / 600 / 135 / 3:15 (screenshot).
4. Show the contents of `settings.json` in the simulator's file area.
5. Write garbage into `settings.json`, then relaunch. Setup must show the
   defaults, and there must be no crash.

### B6 SPEC walk-through
Acceptance criteria 1–13 of SPEC.md, each run in the simulator, each with
PASS/FAIL and its evidence (log excerpt or screenshot). Criteria that are only
practical on the host (exact timings, for example) point to their A test.

---

## C. Store screenshots

Five clean screenshots, captured by the QA script from the simulator with
realistic mid-session values:

| # | Screen | State shown |
|---|---|---|
| 1 | Setup | Defaults, "Reps" highlighted |
| 2 | GPS wait | Fix acquired, Start hint lit |
| 3 | Rep | Rep 3/6, e.g. 180 m left, pace 4:05 /km, HR 162 |
| 4 | Rest | HR 131 vs target 120, max-rest countdown 1:12, next 4/6 |
| 5 | Summary | 6/6 reps, rep rows visible |

"Clean" means:
- no debug overlays (F2 invalidated areas off);
- no error state (GPS LOST, `HR --`);
- button hints as in B2;
- the B1 checks pass;
- output is 240×240 PNG, converted from the simulator BMP, in
  `IntervalCoach/Store/screenshots/01-setup.png` … `05-summary.png`.

`deploy.md` gives no size rules for the store. If the store upload asks for a
different size, the PNGs are rescaled with nearest-neighbour (no blur), and the
240×240 originals are kept.

---

## SPEC → test traceability

| SPEC criterion | Host | Simulator |
|---|---|---|
| 1 Defaults | A7 `DefaultsWhenNoFile` | B6 |
| 2 Settings persist | A8 `RoundTrip` | B5 |
| 3 Limits, no wrap | A7 `ClampAtLimits` | B6 |
| 4 Start needs fix | A7 `StartNeedsFix` | B2, B6 |
| 5 Rep done → Rest | A5 | B3 |
| 6 Rest ends by HR | A2 `ThreeSecondsBelowEnds` | B3 (`session.txt`) |
| 7 Rest ends by MAX / no HR | A3 | B6 (`faults.txt`, keys 8 and 7) |
| 8 Skip | A6 | B2 |
| 9 Last rep → Summary | A5 | B3, B6 |
| 10 End session? | A4 `EndConfirmFromPause`, A5 `EarlyEnd…` | B2 |
| 11 GPS lost | A1 | B3 (key `6`), B1 |
| 12 Summary correct | A5, A10 | B6 |
| 13 Pause | A4 | B2, B3 |
| Build notes: survives GUI stop | A9 | B4, W1 |

## Decisions taken while writing the tests

1. **HR debounce:** rest ends by HR only after 3 consecutive seconds below the
   target (decided 7-nov-2026; previously one sample was enough).
2. **Minimum rest:** 15 s (decided 7-nov-2026; previously none).

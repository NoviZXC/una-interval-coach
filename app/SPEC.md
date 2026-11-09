# Interval Coach — Spec v1

Interval training app for the UNA watch. The runner sets reps and rep distance.
GPS measures each rep and the watch vibrates when the distance is reached. Rest
ends when heart rate drops below a target, with a maximum rest time as a backup,
and the watch vibrates to start the next rep. A summary is shown at the end.

App type: `Activity`. GUI: TouchGFX. 240x240 screen, 4 buttons, no touch.

## 1. Settings (on the watch)

Edited on the Setup screen before each session. The last used values are saved
in a file in the app's directory and restored at the next launch.

| Setting         | Range        | Step   | Default |
|-----------------|--------------|--------|---------|
| Reps            | 1–20         | 1      | 6       |
| Rep distance    | 100–2000 m   | 100 m  | 400 m   |
| Rest HR target  | 90–160 bpm   | 5 bpm  | 120     |
| Max rest        | 0:30–5:00    | 15 s   | 2:00    |

Distance is always shown in meters, even when the watch is set to imperial.

## 2. Screens and flow

```
Setup → GPS wait → Rep 1 → Rest → Rep 2 → Rest → … → Rep N → Summary
```

1. **Setup**: the 4 settings, one highlighted at a time.
2. **GPS wait**: shows "Searching GPS". Start works only once GPS has a fix.
3. **Rep**: meters left to go (big). Smaller: rep n/N, rep time, current pace
   (min/km), HR.
4. **Rest**: current HR and the target (big). Smaller: countdown to max rest,
   "next rep n/N".
5. **End session?**: confirmation, reached from Rep or Rest.
6. **Summary**: shown after the last rep, or after a confirmed early end.

The first rep starts when the runner presses Start. Every later rep starts
automatically the moment rest ends (the GO vibration). After the last rep there
is no rest: the app goes straight to Summary.

**Rest ends** at the first of these:
- HR, with valid trust, stays below the target for **3 consecutive seconds**,
  and at least **15 s** of rest have passed → ended by **HR**. If HR has been
  below the target for 3 s before 15 s are up, rest ends at 15 s.
- Max rest time is reached → ended by **MAX**.
- The runner presses Skip (R1) → ended by **SKIP**.

## 3. Buttons

L1 = Up (top left) · L2 = Down (bottom left) · R1 = Select (top right) · R2 = Back (bottom right)

| Screen        | L1        | L2        | R1                                  | R2                                 |
|---------------|-----------|-----------|-------------------------------------|------------------------------------|
| Setup         | value +   | value −   | next field; after the last → GPS wait | previous field; on the first → exit app |
| GPS wait      | —         | —         | Start (only with a fix)             | back to Setup                      |
| Rep           | —         | —         | pause / resume                      | → End session?                     |
| Rest          | —         | —         | skip rest (next rep starts now)     | → End session?                     |
| End session?  | —         | —         | yes → Summary                       | no → back where you were           |
| Summary       | scroll up | scroll down | exit app                          | exit app                           |

**Pause** (Rep screen only): R1 pauses and R1 again resumes. While paused, the
rep distance and rep time are frozen (GPS distance is not added, and the 10 s
GPS bridge does not run), the Rep screen shows "PAUSED", and each toggle gives
1 soft bump. Rest cannot be paused. R2 still opens End session?. Total session
time excludes paused time.

## 4. Vibration and sound

Every alert also turns the backlight on for 5 s.

| Event                              | Vibration                       | Buzzer |
|------------------------------------|---------------------------------|--------|
| REP DONE (distance reached)        | 2 long buzzes (1 s, pause, 1 s) | beep   |
| GO (rest over, next rep started)   | 3 short strong clicks           | beep   |
| SESSION DONE (after the last rep)  | 1 long + 3 short                | —      |
| GPS LOST / GPS BACK (during a rep) | 1 soft bump                     | —      |
| PAUSE / RESUME                     | 1 soft bump                     | —      |

## 5. GPS and heart-rate dropouts

- **GPS lost during a rep**: for up to 10 s, keep adding distance at the last
  good speed. After 10 s, stop adding, show "GPS LOST" and give a soft bump.
  When the fix comes back, resume counting (soft bump). The rep is marked
  **GPS gap** in the summary.
- **GPS lost during rest**: ignored. It only matters once the next rep starts.
- **HR missing or low trust during rest**: show "HR --". Rest can then end only
  by max rest time or Skip, and that rest is marked **no HR** in the summary.

## 6. Summary

- Header: reps done / planned, and total session time.
- One row per rep: rep #, rep time, average pace (min/km), average HR, and the
  GPS gap flag if it applies.
- Under each rep: rest length and how it ended (HR / MAX / SKIP / no HR).
- L1/L2 scroll. Shown on the watch only; nothing is saved after exit.

## 7. Out of v1

Manual lap · warm-up/cool-down · mixed rep distances (pyramids) ·
time-based reps · FIT file / phone sync · saved history · imperial units ·
phone-app settings · 3-2-1 rest warning · step-cadence distance estimate ·
LVGL version.

## 8. Acceptance criteria (checkable in the simulator)

1. Given defaults, when the app opens, then Setup shows 6 reps / 400 m / 120 bpm / 2:00.
2. Given a changed setting, when the app is exited and reopened, then the changed value is shown.
3. Given Setup, L1/L2 change the value and stop at the range limits (no wrap).
4. Given GPS wait without a fix, R1 does nothing; once there is a fix, R1 starts rep 1.
5. Given a rep, when GPS distance reaches the rep distance, then REP DONE plays and Rest shows.
6. Given rest with valid HR below the target for 3 consecutive seconds (and at least 15 s of rest), then rest ends by HR, GO plays and the next rep starts. A dip shorter than 3 s does not end rest.
7. Given rest with HR above the target (or missing), then rest ends at max rest by MAX (or no HR), and GO plays.
8. Given rest, R1 ends it at once (SKIP), and GO plays.
9. Given the last rep, when its distance is reached, then SESSION DONE plays and Summary shows. There is no rest after it.
10. Given Rep or Rest, R2 opens End session?; R1 there shows Summary with the reps done so far; R2 returns unchanged.
11. Given GPS lost for under 10 s mid-rep, the distance keeps rising; past 10 s it freezes and "GPS LOST" shows; the rep is flagged GPS gap.
12. The summary rows match the session: rep times, paces, avg HR, rest lengths and end reasons.
13. Given a rep, R1 pauses: the distance and rep time stop and "PAUSED" shows; R1 again resumes from the same values. Paused time is not in the rep time or the total session time.

## 9. Build notes

- Session logic, sensors (GPS location, speed and distance; heart rate with
  trust), vibration and buzzer live in the **service**, so the session keeps
  running when the screen is suspended. The GUI only displays state and
  forwards button presses.
- If the GUI stops (navigated away or unloaded) while a session is active, the
  service stays resident and keeps running the session (Stopwatch pattern in
  `Docs/service-lifecycle.md` §5.4). When a GUI comes back
  (`COMMAND_APP_NOTIF_GUI_RUN`), the service sends it the full current state.
  The service exits only when no session is active and no GUI is up.
- Output: `.uapp` for the watch, plus the TouchGFX simulator build.

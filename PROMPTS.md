# Prompts from the video

These are the prompts used in the video, in order. Paste them into Claude Code or the Claude desktop app.

Replace the example paths with your own:
- `C:\una-video\una-sdk`: your SDK clone
- `C:\una-video\...`: your project folder
- the installer folder

## 1 · Set up your environment
**Prompt 1A**: install everything
```
Read the UNA SDK setup guide at https://github.com/UNAWatch/una-sdk/blob/main/Docs/sdk-setup.md. Tell me which programs I need on Windows and every step to get a working environment. Then check what's already on this PC and install whatever is missing, one step at a time. The SDK is already cloned in C:\una-video\una-sdk. Tell me before anything that needs admin rights.
```

**Reply** for when Claude says the ST tools need an account:
```
Downloading from ST needs a free account, so I already did that part. Both installers are in C:\una-video-installers. Install STM32CubeIDE to C:\ST\STM32CubeIDE_2.2.0 and TouchGFX Designer to D:\TouchGFX\4.26.1.
```

**Prompt 1B**: prove the setup works
```
Great. Now help me prove the setup works: create a tiny demo app in C:\una-video\demo, starting from the simulator-ready example in the SDK that fits best, build it for the watch (.uapp) and run it in the TouchGFX simulator. Tell me which keys are the four watch buttons.
```

## 2 · Discovery: what can the watch do?
```
Look at $UNA_SDK/Libs/Header/SDK/SensorLayer/SensorTypes.hpp and the docs in $UNA_SDK/Docs, and list what an app on this watch can use: sensors, vibration, buttons, screen, storage. One line each, plain English.
```

## 4 · Writing the spec (with the Grill Me skill)
```
I want to build Interval Coach, an interval training app for the UNA watch. I set the number of reps and the distance of each rep. GPS measures the rep, and the watch vibrates when I hit the distance. Then the rest ends when my heart rate drops below a target, with a maximum rest time as a backup, and the watch vibrates to tell me to go again. At the end I get a summary.

Use your grill-me skill: interview me one question at a time — settings, screens, what each of the 4 buttons does, vibration patterns, what happens when GPS or heart rate drops out, and what is OUT of version 1. When we agree, write it to SPEC.md and wait for my OK before any code.
```

## 5 · Setting up QA
Tip: use your strongest model for this step.
```
Before any code, write QA.md: how you will test this app before telling me it's done.

Context: this is a UNA watch. Read $UNA_SDK/Docs/platform-overview.md, Simulator.md, unit-testing.md, SensorsLayer.md, touchgfx-widgets.rst and figma-ui-kit.rst (the official UI kit: screen geometry, button hints, visual language). Use the SDK's Running example as the reference for GPS, heart rate and activity screens.

QA.md must cover:
(A) Host unit tests (GoogleTest) for the interval logic: GPS lost mid-rep, heart rate hovering right at the target, heart rate that never drops (max rest kicks in), pause and resume, last rep shows the summary.
(B) Simulator checks: every screen fits inside the round display and is readable at a glance; button hints match what each button does; vibration patterns appear in the log; the session keeps running with the app closed; settings survive a restart.
(C) Store screenshots: 5 clean screenshots from the simulator.
Done = all host tests green + every simulator check passed + the .uapp builds with no warnings.
```

## 6 · Building the app
```
SPEC.md and QA.md are approved. Build version 1 of Interval Coach:
1. Create the project in C:\una-video\IntervalCoach, starting from the SDK's Running example. Init git there with core.autocrlf false (the TouchGFX Makefiles must keep LF line endings).
2. Interval logic first: a pure C++ class with no SDK dependencies, with the host tests from QA.md. Run them until they're all green.
3. Then the service: GPS distance, heart rate, vibration patterns, saved settings.
4. Then the TouchGFX screens from SPEC.md: big, readable numbers on a 240x240 round screen.
5. Run the QA.md checks yourself in the simulator and fix what fails.
Commit in small steps. Stop when version 1 runs in the simulator, and tell me what to try first.
```

## 7 · Testing and iterating
Feedback prompts for after you try the app in the simulator. Edit the details, then paste.
```
The rep screen is hard to read at a glance: the pace and the heart rate are too small. Make the main number bigger and drop anything I don't need mid-run.
```
```
On the rest screen I can't tell how far I am from the target heart rate. Show it as a bar or an arc, not just two numbers.
```
```
Here's a bug I saw: <describe it>. First write a test that reproduces it, then fix it.
```
```
Run the QA.md checks again after these changes and show me the screenshots.
```

## 8 · Publishing to the UNA store (with Claude in Chrome)
```
Use Chrome. I'm logged in at apps.unawatch.com. Create a new app listing for Interval Coach:
1. Add a new app, generate its App ID, and put it in CMakeLists.txt (APP_ID, APP_NAME, APP_USER_NAME). Rebuild and show me the "ID :" line in the build log.
2. Stamp app-manifest.json with $UNA_SDK/Utilities/Scripts/app_packer/min_kernel_version.py --stamp, then zip the .uapp, icon.png and app-manifest.json.
3. Write the description: what it does, the controls, and good-to-know notes. No medical claims, and don't mention other watch brands.
4. Upload the 5 simulator screenshots and version 1.0.0 with short release notes.
Do NOT press Release or Publish. Leave it as a draft for me to review.
```

## 9 · Staying up to date
```
Check what changed in the una-sdk releases since the version this app uses, and tell me which new APIs or fixes matter for Interval Coach.
```

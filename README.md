# Interval Coach: build a UNA Watch app with AI agents

Everything from the video **"Build your first UNA Watch app with AI agents"**, free to use:
- the full source of **Interval Coach**
- the exact prompts used in the video
- the Claude skills
- every tool and link mentioned

▶️ Video: **[YouTube link]**

**Interval Coach** is an interval-training app for the UNA Watch. You set your reps and the distance of each rep. GPS measures every rep, and **your heart rate decides when the rest ends**: rest is over when your HR drops below your target, with a maximum rest time as a backup. The watch vibrates to tell you to go again, and you get a summary at the end.

## What's in this repo

| Path | What |
|---|---|
| [`PROMPTS.md`](PROMPTS.md) | Every prompt from the video, in order, ready to paste |
| [`skills/grill-me/`](skills/grill-me/SKILL.md) | The Grill Me skill used to write the spec |
| [`app/SPEC.md`](app/SPEC.md) | The spec Claude and I agreed on with Grill Me |
| [`app/QA.md`](app/QA.md) | The QA plan: host tests, simulator checks, store screenshots |
| [`app/Software/`](app/Software) | App source: service, TouchGFX GUI, CMake |
| [`app/Tests/Host/`](app/Tests/Host) | GoogleTest host tests for the interval engine |
| [`app/qa/`](app/qa) | Simulator QA scripts and checkers Claude wrote |
| [`app/Store/`](app/Store) | Store icon, manifest and the 5 screenshots |

## Tools used in the video

**UNA**
- UNA SDK: https://github.com/UNAWatch/una-sdk
- SDK setup guide: https://github.com/UNAWatch/una-sdk/blob/main/Docs/sdk-setup.md
- UNA developer portal (submit your apps): https://apps.unawatch.com
- UNA developer docs: https://developers.unawatch.com

**Build tools**
These need a free ST account to download, so that's the one step you do yourself.
- STM32CubeIDE 2.2.0: https://www.st.com/en/development-tools/stm32cubeide.html
- TouchGFX Designer 4.26.1 (GUI and simulator): https://www.st.com/en/development-tools/touchgfxdesigner.html

**AI agent**
- Claude Code / Claude desktop app: https://claude.com/claude-code
- Claude in Chrome (used to fill in the store listing): https://claude.com/chrome

## Claude skills
Skills are small instruction packs that change how Claude works. These three make a big difference:

| Skill | What it does | Link |
|---|---|---|
| **Karpathy guidelines** | Think before coding, state assumptions, keep changes small, define what "done" means | https://github.com/multica-ai/andrej-karpathy-skills |
| **Caveman** | Very short answers, no filler: saves tokens and time | https://github.com/JuliusBrussee/caveman |
| **Grill Me** | Claude interviews you one question at a time until you agree on a spec, before any code | Original by Matt Pocock: https://github.com/mattpocock/skills/tree/main/skills/productivity/grill-me · The version I used is in [`skills/grill-me/`](skills/grill-me/SKILL.md) |

**Installing a skill:** copy its folder (the one with `SKILL.md`) into `~/.claude/skills/`. Then run `/skills` in Claude to check it's loaded.

## The workflow
1. **Setup.** Claude reads the SDK setup guide, checks your PC and installs what's missing. Then it builds a tiny demo app and runs it in the simulator. Keys 1-2-3-4 are the four watch buttons.
2. **Discovery.** Start from a problem you actually have, what the watch can do, and what already exists on the UNA store.
3. **Spec.** Grill Me interviews you and writes `SPEC.md`. No code until you approve it.
4. **QA plan.** Claude writes `QA.md` *before* coding, so "done" means done. Use your strongest model for planning and QA.
5. **Build.** Logic and tests first, then the service, then the screens. Claude runs the QA itself in the simulator and fixes what fails.
6. **Publish.** Claude in Chrome creates the listing at apps.unawatch.com and uploads the build and screenshots. You review and press Publish, then UNA reviews the app.

## Build it yourself
**Requirements:** a working UNA SDK environment (see the setup guide above), with `UNA_SDK` pointing to your SDK clone.

1. **App ID:** create your own app at https://apps.unawatch.com and replace `APP_ID` in `app/Software/Apps/IntervalCoach-CMake/CMakeLists.txt`. The ID in this repo belongs to my listing.
2. **Build and run:** build and run it in the TouchGFX simulator the same way as the SDK examples. Or ask Claude: *"Build this app for the watch (.uapp) and run it in the simulator."*
3. **Host tests:** `app/qa/run-host-tests.ps1`.
4. **Simulator QA:** `app/qa/run-all-sim.ps1`.

**Line endings:** keep git `core.autocrlf false`. The TouchGFX Makefiles need LF line endings.

## License
MIT. See [LICENSE](LICENSE).

Parts of `app/` started from the UNA SDK's *Running* example, © UNA Watch Ltd, MIT ([app/LICENSE-UNA-SDK](app/LICENSE-UNA-SDK)).

Made by Hans Quiroz.

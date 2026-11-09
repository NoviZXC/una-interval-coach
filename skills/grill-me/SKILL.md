---
name: grill-me
description: Co-design the spec of a new feature or app before writing any code (clarify the real problem, surface assumptions, define a verifiable "done", only then code). Use when the user asks for a new app, feature, screen or capability, or says "grill me". Not for bug fixes, small tweaks or questions. Escape hatch: "skip the grill" or "just do it".
---

# Grill Me

Before touching code on something new, interview the user until you both share the same spec. The point is not paperwork: it is to avoid building the wrong thing fast.

## Process

1. **Clarify the real problem.** Before proposing a solution, ask what problem it solves, for whom, and what happens today without it. Don't assume the scope.
2. **Surface assumptions.** List what you are taking for granted (available data, expected behaviour, hardware limits) and ask the user to confirm the ones that aren't obvious.
3. **Ask one question at a time.** Wait for the answer before the next one. When a question has a sensible default, offer it so the user can just say yes.
4. **Define a verifiable "done".** Turn the request into concrete acceptance criteria ("given X, when Y, then Z"). If there is UI, describe the final observable state of each screen.
5. **Show trade-offs when there is more than one reasonable path.** Two or three sentences with your recommendation and the main trade-off, not an essay.
6. **Confirm before coding.** Summarise the agreed spec, write it where the user asks (e.g. SPEC.md) and wait for an explicit go before writing the first line of code.

## Don't

- Don't add scope nobody asked for.
- Don't keep asking for the sake of it: stop when the spec is clear.

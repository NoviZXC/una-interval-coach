"""QA.md B3: every engine event in a simulator log is followed by the
vibration / buzzer / backlight pattern SPEC.md section 4 asks for.

usage: python -I qa/check_log.py <log> [EVENT=count ...]
Exit code 0 = pass. Prints one line per event.
"""
import re
import sys

# Vibro notes as (effect) list; 0 = pause. SPEC.md section 4.
EXPECTED = {
    "REP_DONE":     ([16, 0, 16], True),
    "GO":           ([1, 0, 1, 0, 1], True),
    "SESSION_DONE": ([16, 0, 1, 0, 1, 0, 1], False),
    "GPS_LOST":     ([7], False),
    "GPS_BACK":     ([7], False),
    "PAUSED":       ([7], False),
    "RESUMED":      ([7], False),
}

EVENT = re.compile(r"IC\.Event: (\w+)")
EFFECT = re.compile(r"\[\d+\] effect=(\d+)")
SINGLE = re.compile(r"play\(effect = (\d+)\)")
BUZZ = re.compile(r"Mock\.Buzzer.*(short beep|note -|melody\[)")
LIGHT = re.compile(r"Mock\.Backlight.*on backlight, timeout = (\d+)")


def main():
    path, expects = sys.argv[1], dict(a.split("=") for a in sys.argv[2:])
    lines = open(path, encoding="utf-8", errors="replace").read().splitlines()
    starts = [(i, m.group(1)) for i, l in enumerate(lines) if (m := EVENT.search(l))]
    ok = True
    counts = {}
    for n, (i, name) in enumerate(starts):
        counts[name] = counts.get(name, 0) + 1
        if name not in EXPECTED:
            print(f"  {name:13} (no alert expected)")
            continue
        end = starts[n + 1][0] if n + 1 < len(starts) else min(len(lines), i + 40)
        block = lines[i + 1:end]
        effects = [int(m.group(1)) for l in block for m in [EFFECT.search(l) or SINGLE.search(l)] if m]
        buzz = any(BUZZ.search(l) for l in block)
        light = [int(m.group(1)) for l in block if (m := LIGHT.search(l))]
        want_fx, want_buzz = EXPECTED[name]
        good = effects == want_fx and buzz == want_buzz and light == [5000]
        ok &= good
        print(f"{'PASS' if good else 'FAIL'} {name:13} vibro={effects} buzzer={'yes' if buzz else 'no'} "
              f"backlight={light}" + ("" if good else f"   want vibro={want_fx} buzzer={'yes' if want_buzz else 'no'} backlight=[5000]"))
    for name, want in expects.items():
        got = counts.get(name, 0)
        good = got == int(want)
        ok &= good
        print(f"{'PASS' if good else 'FAIL'} count {name} = {got} (want {want})")
    print("RESULT:", "PASS" if ok else "FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())

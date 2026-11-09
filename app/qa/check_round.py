"""QA.md B1/B2 on simulator screenshots.

usage: python -I qa/check_round.py <shot.bmp> [--hints L1=none,L2=white,R1=amber,R2=red]

B1  - no lit pixel outside radius 120 (the round display);
    - nothing but the button-hint arcs between radius 108 and 120;
    - only UI-kit colours: greys/white, amber, red (anti-aliasing allowed).
B2  - the colour of each hint arc matches --hints.
Exit code 0 = pass.
"""
import math
import sys

from PIL import Image

# Buttons widget (Docs/Templates/TouchGFX-Widgets/docs/Buttons.md): radius 113,
# angles clockwise from 12 o'clock.
ARCS = {"L1": (291, 308), "L2": (232, 249), "R2": (111, 128), "R1": (52, 69)}
R_SCREEN, R_TEXT, R_ARC = 120.0, 108.0, 113.0
DARK = 40  # channel value under which a pixel counts as black


def angle(dx, dy):
    return math.degrees(math.atan2(dx, -dy)) % 360.0


def in_arc(a, tol=3.0):
    return any(lo - tol <= a <= hi + tol for lo, hi in ARCS.values())


def classify(rgb):
    """The display is ABGR2222 (channels 0/85/170/255). Anti-aliased edges of
    amber (#C08000) and red (#C00000) text land on dim warm values such as
    (85,0,0) and (85,85,0): those are 'warm', allowed but not a hint colour."""
    r, g, b = rgb
    if max(rgb) < DARK:
        return "none"
    if max(rgb) - min(rgb) < 24:
        return "white"
    if r > 100 and g < 0.25 * r and b < 0.25 * r:
        return "red"
    if r > 100 and 0.3 * r <= g <= 0.8 * r and b < 0.25 * r:
        return "amber"
    if b < 40 and r >= g:
        return "warm"
    return "other"


def main():
    path = sys.argv[1]
    hints = {}
    if "--hints" in sys.argv:
        spec = sys.argv[sys.argv.index("--hints") + 1]
        hints = dict(kv.split("=") for kv in spec.split(","))

    img = Image.open(path).convert("RGB")
    w, h = img.size
    px = img.load()
    cx, cy = (w - 1) / 2.0, (h - 1) / 2.0
    outside = ring = off_palette = 0
    for y in range(h):
        for x in range(w):
            p = px[x, y]
            if max(p) < DARK:
                continue
            r = math.hypot(x - cx, y - cy)
            if r > R_SCREEN:
                outside += 1
            elif r > R_TEXT and not in_arc(angle(x - cx, y - cy)):
                ring += 1
            if classify(p) == "other":
                off_palette += 1

    ok = outside == 0 and ring == 0 and off_palette == 0
    print(f"{'PASS' if outside == 0 else 'FAIL'} outside r>120: {outside} px")
    print(f"{'PASS' if ring == 0 else 'FAIL'} non-hint pixels in 108<r<=120: {ring} px")
    print(f"{'PASS' if off_palette == 0 else 'FAIL'} off-palette pixels: {off_palette} px")

    for name, want in hints.items():
        lo, hi = ARCS[name]
        mid = math.radians((lo + hi) / 2.0)
        seen = "none"
        for rr in (111, 112, 113, 114, 115):
            x = round(cx + rr * math.sin(mid))
            y = round(cy - rr * math.cos(mid))
            c = classify(px[x, y])
            if c != "none":
                seen = c
                break
        good = seen == want
        ok &= good
        print(f"{'PASS' if good else 'FAIL'} hint {name}: {seen} (want {want})")

    print("RESULT:", "PASS" if ok else "FAIL", f"({w}x{h})")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())

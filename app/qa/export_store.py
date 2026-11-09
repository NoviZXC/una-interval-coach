"""QA.md C: copy the store screenshots (c-0N-*.bmp) to Store/screenshots as
240x240 PNG, named 01-setup.png ... 05-summary.png.

usage: python -I qa/export_store.py <screenshots dir> <Store/screenshots dir>
"""
import pathlib
import sys

from PIL import Image

src, dst = pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2])
dst.mkdir(parents=True, exist_ok=True)
names = {"c-01-setup": "01-setup", "c-02-gps-ready": "02-gps-ready", "c-03-rep": "03-rep",
         "c-04-rest": "04-rest", "c-05-summary": "05-summary"}
for stem, out in names.items():
    img = Image.open(src / f"{stem}.bmp").convert("RGB")
    assert img.size == (240, 240), (stem, img.size)
    img.save(dst / f"{out}.png", optimize=True)
    print(dst / f"{out}.png")

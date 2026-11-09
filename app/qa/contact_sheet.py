"""Tiles simulator screenshots into one PNG with their names, for review.

usage: python -I qa/contact_sheet.py <out.png> <shot1.bmp> [<shot2.bmp> ...]
"""
import sys

from PIL import Image, ImageDraw

out, shots = sys.argv[1], sys.argv[2:]
cols = 4
cell_w, cell_h = 260, 280
rows = (len(shots) + cols - 1) // cols
sheet = Image.new("RGB", (cols * cell_w, rows * cell_h), (40, 40, 40))
draw = ImageDraw.Draw(sheet)
for i, path in enumerate(shots):
    img = Image.open(path).convert("RGB")
    x, y = (i % cols) * cell_w + 10, (i // cols) * cell_h + 10
    sheet.paste(img, (x, y))
    # The round display edge, for judging fit.
    draw.ellipse([x, y, x + img.width - 1, y + img.height - 1], outline=(0, 90, 160))
    name = path.replace("\\", "/").split("/")[-1].rsplit(".", 1)[0]
    draw.text((x, y + img.height + 6), name, fill=(220, 220, 220))
sheet.save(out)
print(out, sheet.size)

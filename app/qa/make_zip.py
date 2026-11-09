"""Store package (Docs/deploy.md): IntervalCoach_<ver>.uapp, icon.png and
app-manifest.json, flat in one zip.

usage: python -I qa/make_zip.py <repo root>
"""
import json
import pathlib
import shutil
import sys
import zipfile

root = pathlib.Path(sys.argv[1])
pkg = root / "Store" / "package"
manifest = json.loads((pkg / "app-manifest.json").read_text(encoding="utf-8"))
uapp = root / "Output" / manifest["binary"]
assert uapp.exists(), uapp
shutil.copyfile(root / "Resources" / "icon_60x60.png", pkg / "icon.png")

out = root / "Store" / f"IntervalCoach_{manifest['appVersion']}.zip"
with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as z:
    z.write(uapp, uapp.name)
    z.write(pkg / "icon.png", "icon.png")
    z.write(pkg / "app-manifest.json", "app-manifest.json")
with zipfile.ZipFile(out) as z:
    for i in z.infolist():
        print(f"{i.file_size:>8}  {i.filename}")
print(out, out.stat().st_size, "bytes")

#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The LOOM colour theme's check (tools/theme_check.sh runs it): OUT holds the screens of tests/sim_cap.c as raw
RGB565, from the reference firmware (ref_red, ref_inv) and from this one (new_red, new_inv, loom).
1. The two-colour themes are untouched: every screen byte for byte the reference's.
2. LOOM loses nothing: wherever the reference (RED INV) has an edge between ink and ground, LOOM's two pixels differ
   by at least 40 of 255 in some channel. MENU > COLOR's value (the theme's own name) is left out.
Writes a PNG of every LOOM screen and contact sheets into OUT/png."""
import sys
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw

def rgb(p):
    a = np.fromfile(p, dtype="<u2").reshape(240, 240)
    return np.dstack([((a >> 11) & 31) * 255 // 31, ((a >> 5) & 63) * 255 // 63, (a & 31) * 255 // 31]).astype(np.uint8)

def main():
    out = Path(sys.argv[1])
    names = (out / "ref_red/index.txt").read_text().split()
    fails = 0
    for a, b in (("ref_red", "new_red"), ("ref_inv", "new_inv")):
        assert (out / b / "index.txt").read_text().split() == names
        bad = [n for n in names if (out / a / (n + ".raw")).read_bytes() != (out / b / (n + ".raw")).read_bytes()]
        print(f"{'ok  ' if not bad else 'FAIL'}  {b}: {len(names) - len(bad)}/{len(names)} screens identical to the reference", *bad[:8])
        fails += len(bad)
    assert (out / "loom/index.txt").read_text().split() == names
    lost_all = []
    for n in names:
        m = np.fromfile(out / "ref_inv" / (n + ".raw"), dtype="<u2").reshape(240, 240).astype(int)
        c = rgb(out / "loom" / (n + ".raw")).astype(int)
        if "menu-row" in n:
            m[37:51, 160:227] = m[37, 160]
            c[37:51, 160:227] = c[37, 160]
        lost = 0
        for ax in (0, 1):
            lost += int(((np.diff(m, axis=ax) != 0) & (np.abs(np.diff(c, axis=ax)).max(axis=2) < 40)).sum())
        if lost:
            lost_all.append((lost, n))
    print(f"{'ok  ' if not lost_all else 'FAIL'}  loom: {len(names) - len(lost_all)}/{len(names)} screens keep every ink / ground edge",
          *sorted(lost_all, reverse=True)[:8])
    png = out / "png"
    png.mkdir(exist_ok=True)
    for n in names:
        Image.fromarray(rgb(out / "loom" / (n + ".raw"))).save(png / (n + ".png"))
    for k in range(0, len(names), 60):                 # contact sheets, 60 screens each
        part = names[k:k + 60]
        im = Image.new("RGB", (6 * 250 + 10, ((len(part) + 5) // 6) * 266 + 10), (40, 40, 40))
        d = ImageDraw.Draw(im)
        for i, n in enumerate(part):
            x, y = 10 + (i % 6) * 250, 10 + (i // 6) * 266
            im.paste(Image.open(png / (n + ".png")), (x, y))
            d.text((x, y + 242), n[:38], fill=(255, 255, 255))
        im.save(out / f"sheet-{k // 60 + 1:02d}.png")
    return 1 if fails or lost_all else 0

if __name__ == "__main__":
    sys.exit(main())

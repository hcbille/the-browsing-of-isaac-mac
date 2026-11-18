#!/usr/bin/env python3
"""Draw a plain placeholder app icon (no game art) and write it as an .icns.

  python scripts/mac/make_icon.py <out.icns>

Needs Pillow and macOS's iconutil. Used by scripts/mac/build.sh; the build goes on
without an icon when this fails.
"""
import subprocess
import sys
import tempfile
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


def draw(size: int) -> Image.Image:
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    pad = size // 16
    d.rounded_rectangle([pad, pad, size - pad, size - pad], radius=size // 5, fill=(58, 18, 22, 255))
    d.ellipse([size * 0.2, size * 0.2, size * 0.8, size * 0.8], outline=(222, 205, 190, 255),
              width=max(2, size // 40))
    try:
        font = ImageFont.load_default(size=int(size * 0.34))
    except TypeError:
        font = ImageFont.load_default()
    d.text((size / 2, size / 2), "R+", fill=(222, 205, 190, 255), font=font, anchor="mm")
    return img


def main() -> int:
    if len(sys.argv) != 2:
        print(__doc__)
        return 2
    out = Path(sys.argv[1])
    with tempfile.TemporaryDirectory() as t:
        iconset = Path(t) / "AppIcon.iconset"
        iconset.mkdir()
        for base in (16, 32, 128, 256, 512):
            draw(base).save(iconset / f"icon_{base}x{base}.png")
            draw(base * 2).save(iconset / f"icon_{base}x{base}@2x.png")
        subprocess.run(["iconutil", "-c", "icns", str(iconset), "-o", str(out)], check=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())

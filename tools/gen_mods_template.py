"""Generates romfs/modTemplate.zip for Fruit Slicer 2.1.0 native mod support."""
import os
import zipfile
from PIL import Image, ImageDraw

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT_ZIP = os.path.join(ROOT, "romfs", "modTemplate.zip")


def make_template_icon():
    img = Image.new("RGBA", (64, 64), (24, 34, 52, 255))
    d = ImageDraw.Draw(img)
    d.rounded_rectangle([2, 2, 61, 61], radius=10, outline=(255, 210, 80, 255), width=3)
    d.ellipse([14, 14, 50, 50], fill=(235, 74, 82, 255), outline=(255, 244, 214, 255), width=3)
    d.polygon([(32, 16), (42, 8), (38, 18)], fill=(96, 210, 92, 255))
    import io
    buf = io.BytesIO()
    img.save(buf, format="PNG")
    return buf.getvalue()


def main():
    os.makedirs(os.path.dirname(OUT_ZIP), exist_ok=True)
    with zipfile.ZipFile(OUT_ZIP, "w", compression=zipfile.ZIP_DEFLATED) as zf:
        zf.writestr(
            "mod.txt",
            "name=My Fruit Slicer Mod\n"
            "author=YourName\n"
            "version=1.0.0\n"
            "description=Starter template showing supported mod structure.\n",
        )
        zf.writestr("icon.png", make_template_icon())
        zf.writestr(
            "README.txt",
            "Fruit Slicer 2.1.0 Mod Template\n"
            "===============================\n"
            "1. Rename this .zip (or extract it into a subfolder) inside your mods/ folder.\n"
            "   (modTemplate.zip itself is always ignored by the game.)\n"
            "2. Edit mod.txt (name= is required; author=, version=, description= are optional).\n"
            "3. Optional icon.png (64x64 or 128x128 PNG) shows in the Mods menu.\n"
            "4. Override textures or audio using matching relative paths:\n"
            "   - Universal: gfx/png/sprites/<name>.png, gfx/board0.png, audio/<track>.ogg\n"
            "   - 3DS SDMC:  gfx/sprites.t3x, gfx/board0.t3x, audio/<track>.pcm\n",
        )
        zf.writestr("gfx/png/sprites/.keep", "")
        zf.writestr("audio/.keep", "")
    print(f"Wrote {OUT_ZIP} ({os.path.getsize(OUT_ZIP)} bytes)")


if __name__ == "__main__":
    main()

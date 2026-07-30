#!/usr/bin/env python3
"""Generate production-quality indexed BMPs + Butano JSON for Cosmic Crisis."""
from __future__ import annotations

import json
import math
import random
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
GFX = ROOT / "graphics"
GFX.mkdir(exist_ok=True)

# Index 0 = transparent for sprites / unused for opaque BGs
PAL = [
    (0, 0, 0),        # 0 transparent / black
    (10, 10, 24),     # 1 deep space
    (26, 16, 64),     # 2 purple dark
    (42, 32, 96),     # 3 purple mid
    (61, 74, 140),    # 4 blue mid
    (92, 225, 255),   # 5 cyan
    (255, 255, 255),  # 6 white
    (255, 212, 71),   # 7 gold
    (255, 107, 53),   # 8 orange
    (196, 30, 58),    # 9 crimson
    (46, 204, 113),   # 10 green
    (155, 89, 182),   # 11 violet
    (127, 140, 141),  # 12 gray
    (236, 240, 241),  # 13 light
    (230, 126, 34),   # 14 amber
    (52, 152, 219),   # 15 sky
]


def quantize(img: Image.Image) -> Image.Image:
    """Map RGB image to our 16-color palette; color 0 is pure black."""
    src = img.convert("RGBA")
    out = Image.new("P", src.size)
    # Build palette: 16 RGB triplets then pad to 768
    flat = []
    for r, g, b in PAL:
        flat.extend([r, g, b])
    flat.extend([0] * (768 - len(flat)))
    out.putpalette(flat)

    px = src.load()
    op = out.load()
    for y in range(src.height):
        for x in range(src.width):
            r, g, b, a = px[x, y]
            if a < 128:
                op[x, y] = 0
                continue
            best, bd = 0, 1e18
            for i, (pr, pg, pb) in enumerate(PAL):
                if i == 0:
                    continue  # don't pick transparent unless alpha low
                d = (r - pr) ** 2 + (g - pg) ** 2 + (b - pb) ** 2
                if d < bd:
                    bd, best = d, i
            # near-black opaque stays index 1 (space) not 0
            if r + g + b < 18:
                best = 1 if a >= 128 else 0
            op[x, y] = best
    return out


def _write_bmp(path: Path, q: Image.Image) -> None:
    """Write BMP via temp+copy so Windows locks on existing files still work."""
    import shutil

    tmp = path.with_suffix(".tmp.bmp")
    q.save(tmp, format="BMP")
    shutil.copyfile(tmp, path)
    tmp.unlink(missing_ok=True)


def save_sprite(name: str, img: Image.Image, height: int | None = None, width: int | None = None):
    q = quantize(img)
    path = GFX / f"{name}.bmp"
    _write_bmp(path, q)
    meta = {"type": "sprite", "bpp_mode": "bpp_4", "colors_count": 16}
    if height is not None:
        meta["height"] = height
    if width is not None:
        meta["width"] = width
    (GFX / f"{name}.json").write_text(json.dumps(meta, indent=4) + "\n", encoding="utf-8")
    print(f"  sprite {name} {q.size}")


def save_bg(name: str, img: Image.Image):
    # BGs: no transparency needed; keep black as index 1 mapped, force 0 unused
    q = quantize(img)
    # Remap remaining 0 pixels that are opaque black-ish to 1
    px = img.convert("RGBA").load()
    qp = q.load()
    for y in range(q.height):
        for x in range(q.width):
            if qp[x, y] == 0:
                qp[x, y] = 1
    path = GFX / f"{name}.bmp"
    _write_bmp(path, q)
    meta = {"type": "regular_bg", "bpp_mode": "bpp_4", "colors_count": 16}
    (GFX / f"{name}.json").write_text(json.dumps(meta, indent=4) + "\n", encoding="utf-8")
    print(f"  bg {name} {q.size}")


def new_rgba(w, h, fill=(0, 0, 0, 0)):
    return Image.new("RGBA", (w, h), fill)


def put(img, x, y, color):
    if 0 <= x < img.width and 0 <= y < img.height:
        img.putpixel((x, y), color)


def fill_rect(img, x, y, w, h, color):
    for yy in range(y, y + h):
        for xx in range(x, x + w):
            put(img, xx, yy, color)


def ellipse(img, cx, cy, rx, ry, color, fill=True):
    for yy in range(cy - ry, cy + ry + 1):
        for xx in range(cx - rx, cx + rx + 1):
            dx = (xx - cx) / max(rx, 1)
            dy = (yy - cy) / max(ry, 1)
            if dx * dx + dy * dy <= 1.05:
                if fill or abs(dx * dx + dy * dy - 1) < 0.25:
                    put(img, xx, yy, color)


def mix_rgb(c, toward, t: float):
    r, g, b, a = c
    tr, tg, tb = toward[:3]
    t = max(0.0, min(1.0, t))
    return (
        int(r + (tr - r) * t),
        int(g + (tg - g) * t),
        int(b + (tb - b) * t),
        a,
    )


def shade_rgb(c, amount: float):
    """amount > 0 lighten toward white, < 0 darken toward deep space."""
    if amount >= 0:
        return mix_rgb(c, (255, 255, 255, 255), amount)
    return mix_rgb(c, (10, 10, 24, 255), -amount)


def shade_form(img: Image.Image, strength: float = 0.55) -> Image.Image:
    """Top-left light: bright rim + dark rim + soft volume. Opaque pixels only."""
    w, h = img.size
    src = img.load()
    out = img.copy()
    dst = out.load()
    alpha = [[src[x, y][3] >= 128 for x in range(w)] for y in range(h)]
    cx, cy = (w - 1) / 2.0, (h - 1) / 2.0
    rad = max(max(w, h) * 0.55, 1.0)

    def opaque(x: int, y: int) -> bool:
        if x < 0 or y < 0 or x >= w or y >= h:
            return False
        return alpha[y][x]

    for y in range(h):
        for x in range(w):
            if not alpha[y][x]:
                continue
            c = src[x, y]
            lit_edge = (not opaque(x - 1, y)) or (not opaque(x, y - 1))
            sh_edge = (not opaque(x + 1, y)) or (not opaque(x, y + 1))
            nx = (x - cx) / rad
            ny = (y - cy) / rad
            ndot = nx * -0.65 + ny * -0.65

            if lit_edge and not sh_edge:
                dst[x, y] = shade_rgb(c, 0.62)
            elif sh_edge and not lit_edge:
                dst[x, y] = shade_rgb(c, -0.55)
            elif ndot > 0.2:
                dst[x, y] = shade_rgb(c, min(0.7, ndot * strength * 1.4))
            elif ndot < -0.12:
                dst[x, y] = shade_rgb(c, max(-0.65, ndot * strength * 1.2))
    return out


def draw_ship(frame: int, player: int) -> Image.Image:
    img = new_rgba(32, 16)
    hulls = [
        (92, 225, 255, 255),
        (255, 107, 53, 255),
        (46, 204, 113, 255),
        (155, 89, 182, 255),
        (255, 212, 71, 255),
    ]
    hull = hulls[player % 5]
    hull_hi = shade_rgb(hull, 0.45)
    hull_lo = shade_rgb(hull, -0.4)
    wing = (61, 74, 140, 255)
    wing_hi = shade_rgb(wing, 0.4)
    wing_lo = shade_rgb(wing, -0.45)
    glow = (255, 255, 255, 255)
    engine = (255, 107, 53, 255) if frame % 2 == 0 else (255, 212, 71, 255)

    fill_rect(img, 6, 6, 18, 4, hull)
    fill_rect(img, 10, 5, 12, 6, hull)
    fill_rect(img, 14, 4, 8, 8, hull)
    fill_rect(img, 10, 5, 12, 1, hull_hi)
    fill_rect(img, 10, 10, 12, 1, hull_lo)
    fill_rect(img, 6, 6, 2, 4, hull_lo)
    fill_rect(img, 22, 6, 6, 4, glow)
    fill_rect(img, 26, 7, 4, 2, (255, 212, 71, 255))
    fill_rect(img, 8, 2, 10, 3, wing)
    fill_rect(img, 8, 11, 10, 3, wing)
    fill_rect(img, 8, 2, 10, 1, wing_hi)
    fill_rect(img, 8, 13, 10, 1, wing_lo)
    fill_rect(img, 4, 1, 6, 2, wing_hi)
    fill_rect(img, 4, 13, 6, 2, wing_lo)
    fill_rect(img, 16, 6, 4, 4, (52, 152, 219, 255))
    put(img, 18, 7, glow)
    fill_rect(img, 1, 6, 5, 4, engine)
    put(img, 0, 7, engine)
    put(img, 0, 8, engine)
    for x, y in [(6, 5), (6, 10), (21, 4), (21, 11)]:
        put(img, x, y, (10, 10, 24, 255))
    return shade_form(img, 0.5)


def draw_meteor(size: int, frame: int) -> Image.Image:
    img = new_rgba(size, size)
    rng = random.Random(size * 17 + frame * 3)
    cx, cy = size // 2, size // 2
    rock = (127, 140, 141, 255)
    rock_hi = shade_rgb(rock, 0.5)
    rock_lo = shade_rgb(rock, -0.45)
    dark = (10, 10, 24, 255)
    lite = (236, 240, 241, 255)
    hot = (255, 107, 53, 255)
    r = size // 2 - 1
    ang0 = frame * (math.pi / 4)
    for yy in range(size):
        for xx in range(size):
            dx, dy = xx - cx, yy - cy
            dist = math.hypot(dx, dy)
            ang = math.atan2(dy, dx) + ang0
            jagged = r + int(2 * math.sin(ang * 3 + frame))
            if dist <= jagged:
                n = rng.random()
                ndot = ((xx - cx) / max(r, 1)) * -0.65 + ((yy - cy) / max(r, 1)) * -0.65
                if dist > jagged - 1.2:
                    put(img, xx, yy, dark)
                elif n > 0.88:
                    put(img, xx, yy, hot)
                elif n > 0.78:
                    put(img, xx, yy, lite)
                elif ndot > 0.25:
                    put(img, xx, yy, rock_hi)
                elif ndot < -0.2:
                    put(img, xx, yy, rock_lo)
                else:
                    put(img, xx, yy, rock)
    ellipse(img, cx - size // 5, cy - size // 6, max(1, size // 8), max(1, size // 10), dark)
    return shade_form(img, 0.45)


def draw_bullet(kind: int) -> Image.Image:
    img = new_rgba(8, 8)
    if kind == 0:
        fill_rect(img, 2, 3, 5, 2, (255, 212, 71, 255))
        fill_rect(img, 2, 3, 5, 1, shade_rgb((255, 212, 71, 255), 0.5))
        fill_rect(img, 5, 2, 2, 4, (255, 255, 255, 255))
    elif kind == 1:
        fill_rect(img, 1, 2, 5, 1, shade_rgb((92, 225, 255, 255), 0.35))
        fill_rect(img, 1, 5, 5, 1, shade_rgb((92, 225, 255, 255), -0.35))
        fill_rect(img, 3, 3, 4, 2, (255, 255, 255, 255))
    else:
        fill_rect(img, 1, 2, 6, 4, (255, 107, 53, 255))
        fill_rect(img, 1, 2, 6, 1, shade_rgb((255, 107, 53, 255), 0.45))
        fill_rect(img, 1, 5, 6, 1, shade_rgb((255, 107, 53, 255), -0.4))
        fill_rect(img, 3, 3, 4, 2, (255, 255, 255, 255))
    return shade_form(img, 0.4)


def draw_powerup(kind: int) -> Image.Image:
    img = new_rgba(16, 16)
    colors = {
        0: (92, 225, 255, 255),
        1: (155, 89, 182, 255),
        2: (255, 212, 71, 255),
        3: (255, 107, 53, 255),
        4: (46, 204, 113, 255),
    }
    c = colors[kind]
    c_hi = shade_rgb(c, 0.45)
    c_lo = shade_rgb(c, -0.4)
    ellipse(img, 8, 8, 6, 6, c)
    # spherical band
    for yy in range(2, 15):
        for xx in range(2, 15):
            dx, dy = (xx - 8) / 6.0, (yy - 8) / 6.0
            if dx * dx + dy * dy <= 1.0 and img.getpixel((xx, yy))[3] >= 128:
                ndot = dx * -0.65 + dy * -0.65
                if ndot > 0.25:
                    put(img, xx, yy, c_hi)
                elif ndot < -0.2:
                    put(img, xx, yy, c_lo)
    ellipse(img, 8, 8, 4, 4, (10, 10, 24, 255))
    if kind == 0:
        ellipse(img, 8, 8, 3, 3, c_hi, fill=False)
        ellipse(img, 8, 8, 5, 5, c, fill=False)
    elif kind == 1:
        fill_rect(img, 5, 7, 6, 2, c_hi)
        fill_rect(img, 7, 5, 2, 6, c)
    elif kind == 2:
        for i in range(8):
            ang = i * math.pi / 4
            put(img, int(8 + 4 * math.cos(ang)), int(8 + 4 * math.sin(ang)), c_hi)
        put(img, 8, 8, (255, 255, 255, 255))
    elif kind == 3:
        fill_rect(img, 4, 7, 8, 2, c)
        fill_rect(img, 9, 5, 3, 6, (255, 255, 255, 255))
    else:
        fill_rect(img, 5, 6, 2, 2, c_hi)
        fill_rect(img, 9, 6, 2, 2, c_hi)
        fill_rect(img, 6, 8, 4, 3, c)
        put(img, 7, 11, c_lo)
        put(img, 8, 11, c_lo)
    ellipse(img, 8, 8, 6, 6, (255, 255, 255, 180), fill=False)
    return shade_form(img, 0.45)


def draw_heart() -> Image.Image:
    img = new_rgba(8, 8)
    c = (196, 30, 58, 255)
    fill_rect(img, 1, 2, 2, 2, shade_rgb(c, 0.35))
    fill_rect(img, 4, 2, 2, 2, shade_rgb(c, 0.2))
    fill_rect(img, 1, 3, 5, 2, c)
    fill_rect(img, 2, 5, 3, 2, shade_rgb(c, -0.35))
    put(img, 3, 7, shade_rgb(c, -0.5))
    return shade_form(img, 0.5)


def draw_explosion(frame: int) -> Image.Image:
    img = new_rgba(32, 32)
    rng = random.Random(99 + frame)
    cx = cy = 16
    cols = [
        (255, 255, 255, 255),
        (255, 212, 71, 255),
        (255, 107, 53, 255),
        (196, 30, 58, 255),
        (10, 10, 24, 255),
    ]
    radius = 4 + frame * 3
    for _ in range(40 + frame * 20):
        ang = rng.random() * math.tau
        dist = rng.random() * radius
        x = int(cx + math.cos(ang) * dist)
        y = int(cy + math.sin(ang) * dist)
        put(img, x, y, cols[min(frame, len(cols) - 1)])
    ellipse(img, cx, cy, max(1, 6 - frame), max(1, 6 - frame), cols[0])
    return shade_form(img, 0.35)


def draw_button(label_pixels, selected: bool) -> Image.Image:
    """32x16 menu button (valid GBA sprite size)."""
    img = new_rgba(32, 16)
    bg = (42, 32, 96, 255) if not selected else (61, 74, 140, 255)
    border = (92, 225, 255, 255) if selected else (61, 74, 140, 255)
    fill_rect(img, 1, 1, 30, 14, bg)
    fill_rect(img, 1, 1, 30, 2, shade_rgb(bg, 0.35))
    fill_rect(img, 1, 12, 30, 3, shade_rgb(bg, -0.35))
    fill_rect(img, 0, 0, 32, 1, border)
    fill_rect(img, 0, 15, 32, 1, shade_rgb(border, -0.3))
    fill_rect(img, 0, 0, 1, 16, shade_rgb(border, 0.25))
    fill_rect(img, 31, 0, 1, 16, shade_rgb(border, -0.25))
    if selected:
        fill_rect(img, 2, 2, 28, 1, (255, 255, 255, 255))
    for x, y in label_pixels:
        put(img, x + 2, y + 5, (255, 255, 255, 255) if selected else (92, 225, 255, 255))
    return shade_form(img, 0.35)


# Tiny 3x5 caps for menu labels
GLYPHS = {
    "A": ["010", "101", "111", "101", "101"],
    "B": ["110", "101", "110", "101", "110"],
    "C": ["011", "100", "100", "100", "011"],
    "D": ["110", "101", "101", "101", "110"],
    "E": ["111", "100", "110", "100", "111"],
    "G": ["011", "100", "101", "101", "011"],
    "H": ["101", "101", "111", "101", "101"],
    "I": ["111", "010", "010", "010", "111"],
    "K": ["101", "101", "110", "101", "101"],
    "L": ["100", "100", "100", "100", "111"],
    "M": ["101", "111", "111", "101", "101"],
    "N": ["101", "111", "111", "111", "101"],
    "O": ["010", "101", "101", "101", "010"],
    "P": ["110", "101", "110", "100", "100"],
    "R": ["110", "101", "110", "101", "101"],
    "S": ["011", "100", "010", "001", "110"],
    "T": ["111", "010", "010", "010", "010"],
    "U": ["101", "101", "101", "101", "011"],
    "W": ["101", "101", "111", "111", "101"],
    "Y": ["101", "101", "010", "010", "010"],
    " ": ["000", "000", "000", "000", "000"],
    "-": ["000", "000", "111", "000", "000"],
}


def text_pixels(text: str, scale: int = 1):
    pixels = []
    cx = 0
    for ch in text:
        g = GLYPHS.get(ch, GLYPHS[" "])
        for y, row in enumerate(g):
            for x, bit in enumerate(row):
                if bit == "1":
                    for sy in range(scale):
                        for sx in range(scale):
                            pixels.append((cx + x * scale + sx, y * scale + sy))
        cx += 4 * scale
    return pixels


def draw_title_banner() -> Image.Image:
    """Single 64x64 title panel sprite (COSMIC / CRISIS stacked)."""
    img = new_rgba(64, 64)
    fill_rect(img, 2, 2, 60, 60, (26, 16, 64, 255))
    fill_rect(img, 2, 2, 60, 2, (92, 225, 255, 255))
    fill_rect(img, 2, 60, 60, 2, (92, 225, 255, 255))
    fill_rect(img, 2, 2, 2, 60, (92, 225, 255, 255))
    fill_rect(img, 60, 2, 2, 60, (92, 225, 255, 255))
    fill_rect(img, 4, 4, 56, 1, (61, 74, 140, 255))

    for word, oy in (("COSMIC", 18), ("CRISIS", 36)):
        px = text_pixels(word, scale=2)
        text_w = len(word) * 4 * 2
        ox = (64 - text_w) // 2
        for x, y in px:
            put(img, ox + x, oy + y, (255, 255, 255, 255))
            put(img, ox + x, oy + y + 1, (92, 225, 255, 255))
    return img


def draw_big_title_strip() -> Image.Image:
    """Single 64x64 emblem for center branding."""
    img = new_rgba(64, 64)
    ellipse(img, 32, 32, 28, 28, (26, 16, 64, 255))
    # volume on disc
    for yy in range(4, 61):
        for xx in range(4, 61):
            dx, dy = (xx - 32) / 28.0, (yy - 32) / 28.0
            if dx * dx + dy * dy <= 1.0:
                ndot = dx * -0.65 + dy * -0.65
                base = (26, 16, 64, 255)
                if ndot > 0.25:
                    put(img, xx, yy, shade_rgb(base, 0.45))
                elif ndot < -0.2:
                    put(img, xx, yy, shade_rgb(base, -0.4))
    ellipse(img, 32, 32, 28, 28, (92, 225, 255, 255), fill=False)
    ellipse(img, 32, 32, 22, 22, (10, 10, 24, 255))
    for ang in range(-60, 61, 2):
        rad = math.radians(ang + 180)
        for r in range(12, 18):
            col = shade_rgb((92, 225, 255, 255), 0.35 if ang < 0 else -0.25)
            put(img, int(32 + math.cos(rad) * r), int(32 + math.sin(rad) * r), col)
    for i in range(8):
        ang = i * math.pi / 4
        for r in range(4, 10):
            put(img, int(32 + math.cos(ang) * r), int(32 + math.sin(ang) * r), (255, 212, 71, 255))
    put(img, 32, 32, (255, 255, 255, 255))
    for x, y in text_pixels("CC", scale=2):
        put(img, 24 + x, 48 + y, (255, 255, 255, 255))
    return shade_form(img, 0.4)


def draw_starfield() -> Image.Image:
    img = new_rgba(256, 256, (10, 10, 24, 255))
    rng = random.Random(42)
    for _ in range(180):
        x, y = rng.randrange(256), rng.randrange(256)
        c = rng.choice([
            (255, 255, 255, 255),
            (92, 225, 255, 255),
            (61, 74, 140, 255),
            (236, 240, 241, 255),
        ])
        put(img, x, y, c)
        if rng.random() > 0.85:
            put(img, x + 1, y, c)
    for y in range(256):
        for x in range(256):
            n = (math.sin(x * 0.04) + math.cos(y * 0.03)) * 0.5
            if n > 0.7 and img.getpixel((x, y))[0] < 20:
                put(img, x, y, (26, 16, 64, 255))
    return img


def draw_shield_fx(frame: int) -> Image.Image:
    img = new_rgba(32, 32)
    r = 10 + (frame % 2)
    ellipse(img, 16, 16, r, r, (92, 225, 255, 200), fill=False)
    ellipse(img, 16, 16, r - 2, r - 2, (255, 255, 255, 120), fill=False)
    # bright arc top-left
    for a in range(-40, 50, 3):
        rad = math.radians(a + 220)
        put(img, int(16 + math.cos(rad) * r), int(16 + math.sin(rad) * r), (255, 255, 255, 255))
    return shade_form(img, 0.3)


def draw_cursor() -> Image.Image:
    img = new_rgba(8, 8)
    fill_rect(img, 0, 3, 6, 2, (255, 212, 71, 255))
    fill_rect(img, 0, 3, 6, 1, shade_rgb((255, 212, 71, 255), 0.45))
    fill_rect(img, 4, 1, 2, 6, (255, 212, 71, 255))
    put(img, 6, 3, (255, 255, 255, 255))
    put(img, 6, 4, (255, 255, 255, 255))
    return shade_form(img, 0.4)


def draw_weapon_icon(kind: int) -> Image.Image:
    img = new_rgba(16, 16)
    fill_rect(img, 1, 1, 14, 14, (26, 16, 64, 255))
    fill_rect(img, 1, 1, 14, 2, shade_rgb((26, 16, 64, 255), 0.4))
    fill_rect(img, 1, 12, 14, 3, shade_rgb((26, 16, 64, 255), -0.35))
    if kind == 0:
        fill_rect(img, 3, 7, 10, 2, (255, 212, 71, 255))
        fill_rect(img, 3, 7, 10, 1, shade_rgb((255, 212, 71, 255), 0.4))
    elif kind == 1:
        fill_rect(img, 3, 5, 10, 2, shade_rgb((92, 225, 255, 255), 0.3))
        fill_rect(img, 3, 9, 10, 2, shade_rgb((92, 225, 255, 255), -0.3))
    else:
        fill_rect(img, 3, 5, 10, 6, (255, 107, 53, 255))
        fill_rect(img, 3, 5, 10, 2, shade_rgb((255, 107, 53, 255), 0.4))
        fill_rect(img, 3, 9, 10, 2, shade_rgb((255, 107, 53, 255), -0.35))
    return shade_form(img, 0.35)


def draw_commander_portrait() -> Image.Image:
    """32x32 helmeted pilot bust — readable silhouette, no bare face."""
    C = {
        ".": (26, 16, 64, 255),
        "#": (10, 10, 24, 255),
        "g": (127, 140, 141, 255),
        "S": (236, 240, 241, 255),
        "c": (92, 225, 255, 255),
        "b": (52, 152, 219, 255),
        "U": (61, 74, 140, 255),
        "D": (42, 32, 96, 255),
        "Y": (255, 212, 71, 255),
        "W": (255, 255, 255, 255),
    }
    # Clean fighter-pilot look: steel dome, solid cyan visor, small jaw, wide suit.
    rows = [
        "................................",
        "................................",
        "...........gggggggg.............",
        ".........gggggggggggg...........",
        "........gg########gggg..........",
        ".......gg##########ggg..........",
        "......gg############gg..........",
        "......g##############g..........",
        "......g#cccccccccccc#g..........",  # visor band
        "......g#cbbWWWbbbccc#g..........",
        "......g#cbbbbbbbbbcc#g..........",
        "......g#ccbbbbbbcccc#g..........",
        "......g#cccccccccccc#g..........",
        "......gg############gg..........",
        ".......g###SSSSSS###g...........",  # pale jaw
        "........g##SSggSS##g............",
        ".........g#SggggS#g.............",
        "..........gSSSSSSg..............",
        ".........UUgSSSSgUU.............",
        "........UUUYUSSUYUUU............",
        ".......UUUUUUDDUUUUUU...........",
        ".....DUUUUUUUUUUUUUUUDD.........",
        "....DDUUUUUUUUUUUUUUUDDD........",
        "...DDDDDDDDDDDDDDDDDDDDDD.......",
        "...UUUccccccccccccccccUUU.......",
        "...UUUUUUUUUUUUUUUUUUUUUU.......",
        "...UUUUUUUUUUUUUUUUUUUUUU.......",
        "...UUUUUUUUUUUUUUUUUUUUUU.......",
        "...UUUUUUUUUUUUUUUUUUUUUU.......",
        "...UUUUUUUUUUUUUUUUUUUUUU.......",
        "...UUUUUUUUUUUUUUUUUUUUUU.......",
        "...UUUUUUUUUUUUUUUUUUUUUU.......",
    ]
    assert len(rows) == 32 and all(len(r) == 32 for r in rows)
    img = new_rgba(32, 32)
    for y, row in enumerate(rows):
        for x, ch in enumerate(row):
            put(img, x, y, C[ch])
    return img


def main():
    print("Generating Cosmic Crisis assets...")

    # Keep space / already-shaded art untouched.
    # save_bg("starfield", draw_starfield())
    # save_sprite("title_logo", draw_title_banner(), height=64)
    # save_sprite("portrait_commander", draw_commander_portrait(), height=32)

    save_sprite("title_emblem", draw_big_title_strip(), height=64)

    # Menu buttons: 32x16, normal + selected (2 frames tall) => 32x32 sheet
    for key, label in [("play", "PLAY"), ("multi", "MULTI"), ("options", "OPT"),
                       ("campaign", "STORY"), ("wireless", "WIFI"), ("online", "NET"),
                       ("cable", "CABLE"), ("back", "BACK")]:
        sheet = new_rgba(32, 32)
        short = label[:5]
        sheet.paste(draw_button(text_pixels(short), False), (0, 0))
        sheet.paste(draw_button(text_pixels(short), True), (0, 16))
        save_sprite(f"btn_{key}", sheet, height=16, width=32)

    ship_sheet = new_rgba(160, 32)
    for p in range(5):
        for f in range(2):
            ship_sheet.paste(draw_ship(f, p), (p * 32, f * 16))
    save_sprite("ships", ship_sheet, height=16, width=32)

    m16 = new_rgba(64, 16)
    for f in range(4):
        m16.paste(draw_meteor(16, f), (f * 16, 0))
    save_sprite("meteor16", m16, height=16, width=16)

    m32 = new_rgba(128, 32)
    for f in range(4):
        m32.paste(draw_meteor(32, f), (f * 32, 0))
    save_sprite("meteor32", m32, height=32, width=32)

    bsheet = new_rgba(24, 8)
    for k in range(3):
        bsheet.paste(draw_bullet(k), (k * 8, 0))
    save_sprite("bullets", bsheet, height=8, width=8)

    names = ["shield", "slow", "clear", "weapon", "life"]
    psheet = new_rgba(80, 16)
    for i, _n in enumerate(names):
        psheet.paste(draw_powerup(i), (i * 16, 0))
    save_sprite("powerups", psheet, height=16, width=16)

    save_sprite("heart", draw_heart(), height=8)

    ex = new_rgba(128, 32)
    for f in range(4):
        ex.paste(draw_explosion(f), (f * 32, 0))
    save_sprite("explosion", ex, height=32, width=32)

    sfx = new_rgba(64, 32)
    for f in range(2):
        sfx.paste(draw_shield_fx(f), (f * 32, 0))
    save_sprite("shield_fx", sfx, height=32, width=32)

    save_sprite("cursor", draw_cursor(), height=8)

    wsheet = new_rgba(48, 16)
    for k in range(3):
        wsheet.paste(draw_weapon_icon(k), (k * 16, 0))
    save_sprite("weapon_icons", wsheet, height=16, width=16)

    print("Done.")


if __name__ == "__main__":
    main()

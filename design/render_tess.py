#!/usr/bin/env python3
"""Render the Tess mockup's Mandelbrot view to PNG, at real resolution.

Same view, palette and smooth-colouring maths as design/ui-design.html, so the
output matches what the artifact draws — just without the browser scaling it.
Produces a clean render and a tile-progress render with per-host outlines.
"""
import struct
import zlib

import numpy as np

W, H = 1448, 1432                       # 2x the mockup canvas, same framing
CX, CY = -0.743643887037151, 0.131825904205330
SPAN = 0.0035
MAXIT = 512
TILE = 128                              # 64 px in the mockup, doubled

HOSTS = [                               # everyday shape: aurora on one brick
    ("lucy",   (0x4f, 0x8f, 0xbf), 1.00),
    ("arthur", (0xc8, 0x86, 0x3c), 2.67),
    ("aurora", (0x5f, 0x9e, 0x4a), 6.67),
]

STOPS = [
    (0.00, (6, 18, 42)), (0.18, (16, 54, 92)), (0.36, (28, 107, 140)),
    (0.52, (56, 160, 168)), (0.66, (127, 201, 189)), (0.79, (214, 176, 110)),
    (0.88, (242, 231, 208)), (1.00, (6, 18, 42)),
]


def build_lut(n=1024):
    lut = np.zeros((n, 3), np.uint8)
    xs = np.linspace(0.0, 1.0, n)
    for i, f in enumerate(xs):
        a, b = STOPS[0], STOPS[-1]
        for s in range(len(STOPS) - 1):
            if STOPS[s][0] <= f <= STOPS[s + 1][0]:
                a, b = STOPS[s], STOPS[s + 1]
                break
        span = (b[0] - a[0]) or 1.0
        k = (f - a[0]) / span
        lut[i] = [a[1][c] + (b[1][c] - a[1][c]) * k for c in range(3)]
    return lut


def render(w, h, step=1):
    """Escape-time with smooth colouring; step>1 renders a coarse grid."""
    lut = build_lut()
    scale = SPAN / W
    ys = np.arange(0, h, step)
    xs = np.arange(0, w, step)
    ci = CY + (ys - H / 2.0) * scale
    cr = CX + (xs - W / 2.0) * scale
    C = cr[None, :] + 1j * ci[:, None]
    Z = np.zeros_like(C)
    it = np.zeros(C.shape, np.int32)
    live = np.ones(C.shape, bool)
    for _ in range(MAXIT):
        Z[live] = Z[live] * Z[live] + C[live]
        mag = Z.real * Z.real + Z.imag * Z.imag
        escaped = live & (mag > 16.0)
        it[escaped] = _ + 1
        live &= ~escaped
        if not live.any():
            break
    it[live] = MAXIT

    mag = np.maximum(Z.real * Z.real + Z.imag * Z.imag, 1e-12)
    with np.errstate(all="ignore"):
        log_zn = np.log(mag) / 2.0
        nu = np.log(np.maximum(log_zn, 1e-12) / np.log(2.0)) / np.log(2.0)
        mu = it + 1 - nu
        f = (np.log(np.maximum(1.0 + mu, 1.0)) / np.log(1.0 + MAXIT) * 3.0) % 1.0
    f = np.nan_to_num(f, nan=0.0, posinf=0.0, neginf=0.0)
    idx = np.clip((f * 1023).astype(np.int32), 0, 1023)
    out = lut[idx]
    out[it >= MAXIT] = (0, 0, 0)

    if step > 1:                        # nearest-neighbour upscale to full size
        out = np.repeat(np.repeat(out, step, 0), step, 1)[:h, :w]
    return out


def write_png(path, rgb):
    h, w, _ = rgb.shape
    raw = b"".join(b"\x00" + rgb[y].tobytes() for y in range(h))

    def chunk(tag, data):
        c = struct.pack(">I", len(data)) + tag + data
        return c + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)

    png = (b"\x89PNG\r\n\x1a\n"
           + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
           + chunk(b"IDAT", zlib.compress(raw, 9))
           + chunk(b"IEND", b""))
    open(path, "wb").write(png)
    return len(png)


def tiles_centre_out():
    ts = []
    for ty in range(0, H, TILE):
        for tx in range(0, W, TILE):
            ts.append((tx, ty, min(TILE, W - tx), min(TILE, H - ty)))
    ts.sort(key=lambda t: (t[0] + t[2] / 2 - W / 2) ** 2 + (t[1] + t[3] / 2 - H / 2) ** 2)
    return ts


def outline(img, x, y, w, h, colour, weight=2):
    img[y:y + weight, x:x + w] = colour
    img[y + h - weight:y + h, x:x + w] = colour
    img[y:y + h, x:x + weight] = colour
    img[y:y + h, x + w - weight:x + w] = colour


def main():
    print("rendering full resolution ...")
    full = render(W, H, 1)
    n = write_png("tess-mandelbrot.png", full)
    print("  tess-mandelbrot.png  %.1f MB" % (n / 1e6))

    print("rendering tile-progress view ...")
    coarse = render(W, H, 16)
    ts = tiles_centre_out()
    total = len(ts)
    done_n = int(total * 0.62)

    # assign tiles to hosts in proportion to their compute weight
    wsum = sum(h[2] for h in HOSTS)
    owner = []
    acc = [0.0] * len(HOSTS)
    for i in range(total):
        for k, h in enumerate(HOSTS):
            acc[k] += h[2] / wsum
        k = max(range(len(HOSTS)), key=lambda j: acc[j])
        acc[k] -= 1.0
        owner.append(k)

    img = coarse.copy()
    for i, (x, y, w, h) in enumerate(ts):
        k = owner[i]
        if i < done_n:
            img[y:y + h, x:x + w] = full[y:y + h, x:x + w]
            if i >= done_n - 6:                     # just landed — fading outline
                outline(img, x, y, w, h, HOSTS[k][1])
        elif i < done_n + 3:                        # in flight — dimmed + outline
            img[y:y + h, x:x + w] = (img[y:y + h, x:x + w] * 0.55).astype(np.uint8)
            outline(img, x, y, w, h, HOSTS[k][1])
    n = write_png("tess-mandelbrot-tiles.png", img)
    print("  tess-mandelbrot-tiles.png  %.1f MB" % (n / 1e6))
    counts = [sum(1 for i in range(done_n) if owner[i] == k) for k in range(len(HOSTS))]
    print("  tiles: %s of %d" % (
        ", ".join("%s %d" % (HOSTS[k][0], counts[k]) for k in range(len(HOSTS))), total))


if __name__ == "__main__":
    main()

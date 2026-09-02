#!/usr/bin/env python3
"""Pack captured frames into a compact animated PNG.

ffmpeg's APNG encoder writes essentially full frames (~374 kB each here). Almost
nothing changes between two frames of this animation — a few tiles and some
digits — so this writes proper delta frames instead: each frame is cropped to the
bounding box of what changed, unchanged pixels inside that box are made fully
transparent, and the frame is composited with blend_op=OVER onto the previous one.

That is the same dirty-rectangle idea the design uses for its X11 blitting, which
is a pleasing coincidence rather than a requirement.
"""
import os
import struct
import subprocess
import sys
import zlib

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
FRAMEDIR = os.path.join(HERE, "frames")
OUT = os.path.join(HERE, "img", "tess-replay.apng")
W, H = 1280, 1024
DELAY_MS = 90            # per frame
HOLD_MS = 1600           # last frame, so the finished render is readable


def load_frames():
    """Decode the captured PNGs to RGB arrays via ffmpeg (no PIL here)."""
    names = sorted(f for f in os.listdir(FRAMEDIR) if f.startswith("f") and f.endswith(".png"))
    raw = subprocess.run(
        ["ffmpeg", "-v", "error", "-i", os.path.join(FRAMEDIR, "f%03d.png"),
         "-f", "rawvideo", "-pix_fmt", "rgb24", "-"],
        capture_output=True).stdout
    n = len(raw) // (W * H * 3)
    if n != len(names):
        print("warning: decoded %d frames, expected %d" % (n, len(names)))
    return np.frombuffer(raw, np.uint8)[: n * W * H * 3].reshape(n, H, W, 3)


def chunk(tag, data):
    return (struct.pack(">I", len(data)) + tag + data
            + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF))


def scanlines(rgba):
    """filter type 0 per row; zlib handles the long transparent runs."""
    h = rgba.shape[0]
    return b"".join(b"\x00" + rgba[y].tobytes() for y in range(h))


def main():
    frames = load_frames()
    n = len(frames)
    print("frames: %d  %dx%d" % (n, W, H))

    parts = [b"\x89PNG\r\n\x1a\n",
             chunk(b"IHDR", struct.pack(">IIBBBBB", W, H, 8, 6, 0, 0, 0)),
             chunk(b"acTL", struct.pack(">II", n, 0))]

    seq = 0
    # frame 0: full canvas, opaque, carried by IDAT
    delay = DELAY_MS if n > 1 else HOLD_MS
    parts.append(chunk(b"fcTL", struct.pack(">IIIIIHHBB", seq, W, H, 0, 0,
                                           delay, 1000, 0, 0)))
    seq += 1
    base = np.dstack([frames[0], np.full((H, W, 1), 255, np.uint8)])
    parts.append(chunk(b"IDAT", zlib.compress(scanlines(base), 9)))

    prev = frames[0]
    total_px = 0
    for i in range(1, n):
        cur = frames[i]
        diff = np.any(cur != prev, axis=2)
        if not diff.any():                       # nothing moved; hold instead
            ys = xs = np.array([0])
            y0 = x0 = 0
            y1 = x1 = 1
        else:
            ys, xs = np.where(diff)
            y0, y1 = ys.min(), ys.max() + 1
            x0, x1 = xs.min(), xs.max() + 1

        sub = cur[y0:y1, x0:x1]
        mask = diff[y0:y1, x0:x1]
        alpha = np.where(mask, 255, 0).astype(np.uint8)[:, :, None]
        rgba = np.dstack([np.where(mask[:, :, None], sub, 0).astype(np.uint8), alpha])
        total_px += int(mask.sum())

        d = DELAY_MS if i < n - 1 else HOLD_MS
        parts.append(chunk(b"fcTL", struct.pack(
            ">IIIIIHHBB", seq, x1 - x0, y1 - y0, x0, y0, d, 1000, 0, 1)))
        seq += 1
        parts.append(chunk(b"fdAT", struct.pack(">I", seq)
                           + zlib.compress(scanlines(rgba), 9)))
        seq += 1
        prev = cur
        sys.stdout.write("\r  packing %d/%d  box %dx%d" % (i + 1, n, x1 - x0, y1 - y0))
        sys.stdout.flush()
    print()

    parts.append(chunk(b"IEND", b""))
    data = b"".join(parts)
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    open(OUT, "wb").write(data)
    print("wrote %s  %.2f MB" % (OUT, len(data) / 1e6))
    print("changed pixels: %.1f%% of all frame pixels"
          % (100.0 * total_px / ((n - 1) * W * H)))


if __name__ == "__main__":
    main()

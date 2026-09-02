#!/usr/bin/env python3
"""Extract files from SGI IRIX 'inst' product images (magic im001V630P00).

Layout observed:
    12 bytes  magic  "im001V630P00"
    repeated: 2-byte big-endian path length, path bytes, then a
              UNIX compress (LZW, magic 1f 9d) stream holding the file.
The compressed stream has no explicit terminator, so entries are located by
scanning for the next plausible <len><printable-path><1f 9d> header.
"""
import os
import re
import struct
import subprocess
import sys

MAGIC = b"im001V630P00"
CMP = b"\x1f\x9d"


def find_entries(buf):
    """Return list of (path, data_start, header_start)."""
    out = []
    # a header is: 2-byte BE len, len printable path bytes, then 1f 9d
    for m in re.finditer(re.escape(CMP), buf):
        c = m.start()
        # walk back: path must end right at c
        for plen in range(4, 256):
            hs = c - plen - 2
            if hs < 0:
                break
            (n,) = struct.unpack(">H", buf[hs:hs + 2])
            if n != plen:
                continue
            path = buf[hs + 2:c]
            try:
                s = path.decode("ascii")
            except UnicodeDecodeError:
                continue
            if not all(32 <= b < 127 for b in path):
                continue
            if not re.match(r"^[A-Za-z0-9_./+@%,~=:-]+$", s):
                continue
            out.append((s, c, hs))
            break
    return out


def lzw_decompress(data):
    """Best-effort UNIX compress decode; tolerates trailing junk."""
    p = subprocess.run(["gzip", "-dc"], input=data,
                       stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
    return p.stdout


def main(img, outdir, want=None):
    buf = open(img, "rb").read()
    if not buf.startswith(MAGIC):
        print("warning: unexpected magic %r" % buf[:12])
    ents = find_entries(buf)
    print("found %d entries in %s" % (len(ents), img))
    bounds = [e[2] for e in ents] + [len(buf)]
    n = 0
    for i, (path, dstart, _hs) in enumerate(ents):
        if want and not re.search(want, path, re.I):
            continue
        raw = lzw_decompress(buf[dstart:bounds[i + 1]])
        if not raw:
            print("  !! empty  %s" % path)
            continue
        dest = os.path.join(outdir, path)
        os.makedirs(os.path.dirname(dest), exist_ok=True)
        open(dest, "wb").write(raw)
        print("  %8d  %s" % (len(raw), path))
        n += 1
    print("extracted %d files" % n)


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2], sys.argv[3] if len(sys.argv) > 3 else None)

#!/usr/bin/env python3
"""Build an animated capture of the Tess desktop mockup.

Generates a standalone 1280x1024 capture page from design/ui-design.html — same
markup, same CSS, same fractal maths — but with the animation replaced by a
deterministic function of a frame number passed in the URL hash. Then drives
headless Chrome once per frame and assembles the frames with ffmpeg.

    python3 make_apng.py            # capture + encode
    python3 make_apng.py --page     # regenerate the capture page only

Every frame is a fresh page load rendering a computed state, so the result is
reproducible rather than dependent on capture timing.
"""
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, "ui-design.html")
PAGE = os.path.join(HERE, "capture.html")
FRAMEDIR = os.path.join(HERE, "frames")
CHROME = "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome"

FRAMES = 46          # 44 animating + 2 held at the end
DT = 90              # simulated ms per frame
FPS = 11

DRIVER = r"""
<script>
(function () {
  "use strict";
  /* ---- deterministic replacement for the live animation ---------------- */
  var FRAME = 0, DT = __DT__, FRAMES = __FRAMES__;
  var m = location.hash.match(/f=(\d+)/);
  if (m) FRAME = parseInt(m[1], 10);

  function mulberry32(a) {
    return function () {
      a |= 0; a = a + 0x6D2B79F5 | 0;
      var t = Math.imul(a ^ a >>> 15, 1 | a);
      t = t + Math.imul(t ^ t >>> 7, 61 | t) ^ t;
      return ((t ^ t >>> 14) >>> 0) / 4294967296;
    };
  }

  var cv = document.getElementById("fractal");
  var ctx = cv.getContext("2d");
  var W = cv.width, H = cv.height, TILE = 64;
  var CX = -0.743643887037151, CY = 0.131825904205330;
  var SPAN = 0.0035, MAXIT = 512;

  var LUT = (function () {
    var stops = [
      [0.00, 6, 18, 42], [0.18, 16, 54, 92], [0.36, 28, 107, 140],
      [0.52, 56, 160, 168], [0.66, 127, 201, 189], [0.79, 214, 176, 110],
      [0.88, 242, 231, 208], [1.00, 6, 18, 42]
    ];
    var t = new Uint8Array(1024 * 3);
    for (var i = 0; i < 1024; i++) {
      var f = i / 1023, a = stops[0], b = stops[stops.length - 1];
      for (var s = 0; s < stops.length - 1; s++) {
        if (f >= stops[s][0] && f <= stops[s + 1][0]) { a = stops[s]; b = stops[s + 1]; break; }
      }
      var k = (b[0] - a[0]) ? (f - a[0]) / (b[0] - a[0]) : 0;
      t[i * 3] = a[1] + (b[1] - a[1]) * k;
      t[i * 3 + 1] = a[2] + (b[2] - a[2]) * k;
      t[i * 3 + 2] = a[3] + (b[3] - a[3]) * k;
    }
    return t;
  })();

  var scale = SPAN / W, rgb = [0, 0, 0];
  function shade(it, zr, zi) {
    if (it >= MAXIT) { rgb[0] = 0; rgb[1] = 0; rgb[2] = 0; return; }
    var log_zn = Math.log(zr * zr + zi * zi) / 2;
    var nu = Math.log(log_zn / Math.LN2) / Math.LN2;
    var f = (Math.log(1 + it + 1 - nu) / Math.log(1 + MAXIT)) * 3.0 % 1;
    var idx = (f * 1023) | 0;
    if (idx < 0) idx = 0; else if (idx > 1023) idx = 1023;
    rgb[0] = LUT[idx * 3]; rgb[1] = LUT[idx * 3 + 1]; rgb[2] = LUT[idx * 3 + 2];
  }

  function mk(w, h) { var c = document.createElement("canvas"); c.width = w; c.height = h; return c; }
  var fine = mk(W, H), coarse = mk(W, H);

  function paintInto(c, step) {
    var g = c.getContext("2d"), img = g.createImageData(W, H), d = img.data;
    for (var y = 0; y < H; y += step) {
      var ci = CY + (y - H / 2) * scale;
      for (var x = 0; x < W; x += step) {
        var cr = CX + (x - W / 2) * scale;
        var zr = 0, zi = 0, it = 0, zr2 = 0, zi2 = 0;
        while (it < MAXIT && zr2 + zi2 <= 16) {
          zi = 2 * zr * zi + ci; zr = zr2 - zi2 + cr;
          zr2 = zr * zr; zi2 = zi * zi; it++;
        }
        shade(it, zr, zi);
        var r = rgb[0], gg = rgb[1], b = rgb[2];
        for (var by = 0; by < step && y + by < H; by++) {
          for (var bx = 0; bx < step && x + bx < W; bx++) {
            var o = ((y + by) * W + (x + bx)) * 4;
            d[o] = r; d[o + 1] = gg; d[o + 2] = b; d[o + 3] = 255;
          }
        }
      }
    }
    g.putImageData(img, 0, 0);
  }
  paintInto(fine, 1);
  paintInto(coarse, 8);

  /* ---- the cluster: lucy is master-only, so only two hosts compute ----- */
  var HOSTS = [
    { id: "arthur", hue: "#c8863c", weight: 2.67, cell: "t-arthur", bar: "b-arthur", n: 0, free: 0 },
    { id: "aurora", hue: "#5f9e4a", weight: 6.67, cell: "t-aurora", bar: "b-aurora", n: 0, free: 0 }
  ];

  var cols = Math.ceil(W / TILE), rows = Math.ceil(H / TILE), tiles = [];
  for (var r = 0; r < rows; r++) {
    for (var c = 0; c < cols; c++) {
      tiles.push({ x: c * TILE, y: r * TILE, w: Math.min(TILE, W - c * TILE), h: Math.min(TILE, H - r * TILE) });
    }
  }
  tiles.sort(function (a, b) {
    return Math.hypot(a.x + a.w / 2 - W / 2, a.y + a.h / 2 - H / 2) -
           Math.hypot(b.x + b.w / 2 - W / 2, b.y + b.h / 2 - H / 2);
  });
  var TOTAL = tiles.length;

  /* replay the whole schedule, then read off the state at time T */
  function schedule() {
    var rnd = mulberry32(20260726);
    var next = 0, ev = [];
    HOSTS.forEach(function (h) { h.free = 0; });
    while (next < TOTAL) {
      var k = 0;
      for (var i = 1; i < HOSTS.length; i++) if (HOSTS[i].free < HOSTS[k].free) k = i;
      var h = HOSTS[k];
      var cost = (700 + rnd() * 400) / h.weight;
      var t = tiles[next++];
      ev.push({ t: t, host: h, start: h.free, end: h.free + cost });
      h.free += cost;
    }
    return ev;
  }
  var EV = schedule();
  var SPAN_MS = Math.max.apply(null, EV.map(function (e) { return e.end; }));

  var T = Math.min(FRAME, FRAMES - 3) / (FRAMES - 3) * SPAN_MS;
  var finished = FRAME >= FRAMES - 3;
  if (finished) T = SPAN_MS + 1;

  ctx.drawImage(coarse, 0, 0);
  var done = 0;
  HOSTS.forEach(function (h) { h.n = 0; });
  EV.forEach(function (e) {
    if (e.end <= T) {
      ctx.drawImage(fine, e.t.x, e.t.y, e.t.w, e.t.h, e.t.x, e.t.y, e.t.w, e.t.h);
      e.host.n++; done++;
      var age = T - e.end;
      if (age < 420 && !finished) {
        ctx.save();
        ctx.globalAlpha = 0.85 * (1 - age / 420);
        ctx.strokeStyle = e.host.hue; ctx.lineWidth = 1;
        ctx.strokeRect(e.t.x + 0.5, e.t.y + 0.5, e.t.w - 1, e.t.h - 1);
        ctx.restore();
      }
    } else if (e.start <= T) {
      ctx.save();
      ctx.globalAlpha = 0.34; ctx.fillStyle = "#000";
      ctx.fillRect(e.t.x, e.t.y, e.t.w, e.t.h);
      ctx.globalAlpha = 0.9; ctx.strokeStyle = e.host.hue; ctx.lineWidth = 1;
      ctx.strokeRect(e.t.x + 0.5, e.t.y + 0.5, e.t.w - 1, e.t.h - 1);
      ctx.restore();
    }
  });

  /* ---- the readouts move with it -------------------------------------- */
  var pct = Math.round(100 * done / TOTAL);
  var secs = T / 1000;
  var mpix = done * TILE * TILE / 1e6;
  var rate = secs > 0.2 ? mpix / secs : 0;
  var eta = rate > 0 ? (TOTAL - done) * TILE * TILE / 1e6 / rate : 0;

  function set(id, txt) { var e = document.getElementById(id); if (e) e.textContent = txt; }
  set("m-text", pct + "%");
  document.getElementById("m-fill").style.width = pct + "%";
  set("tb-render", "tess — mandel — render — 1.00e+03× — " + (finished ? "done" : pct + "%"));
  if (finished) {
    set("m-detail", TOTAL + "/" + TOTAL + " tiles · " + secs.toFixed(1) + " s · " + rate.toFixed(2) + " Mpix/s · done");
    set("sl-prog", "tiles " + TOTAL + "/" + TOTAL + " · " + rate.toFixed(2) + " Mpix/s · " + secs.toFixed(1) + " s");
    set("sl-state", "DONE");
  } else {
    set("m-detail", done + "/" + TOTAL + " tiles · elapsed " + secs.toFixed(1) + " s · eta " + eta.toFixed(1) + " s · " + rate.toFixed(2) + " Mpix/s");
    set("sl-prog", "tiles " + done + "/" + TOTAL + " · " + rate.toFixed(2) + " Mpix/s · " + secs.toFixed(1) + " s");
    set("sl-state", "RENDERING");
  }
  set("a-rate", rate.toFixed(2));
  HOSTS.forEach(function (h) {
    set(h.cell, h.n);
    var bar = document.getElementById(h.bar);
    if (bar) bar.style.width = Math.max(2, Math.round(34 * (h.n / Math.max(1, TOTAL)))) + "px";
  });
  document.documentElement.setAttribute("data-ready", "1");
})();
</script>
"""


def build_page():
    src = open(SRC, encoding="utf-8").read()

    style = re.search(r"<style>.*?</style>", src, re.S).group(0)
    stage = re.search(r'<div class="stage" id="stage">.*?\n    </div>', src, re.S).group(0)
    # the trailing </div> above closes .stage; drop the callout markers for a clean share
    stage = re.sub(r'\s*<div class="callout"[^>]*>\d+</div>', "", stage)

    page = (
        "<title>Tess — replay</title>\n"
        + style
        + """
<style>
  /* capture overrides: exactly one 1280x1024 desktop, nothing else */
  html, body { margin: 0; padding: 0; background: #000; overflow: hidden; }
  .stage { transform: none !important; }
</style>
"""
        + stage
        + DRIVER.replace("__DT__", str(DT)).replace("__FRAMES__", str(FRAMES))
    )
    open(PAGE, "w", encoding="utf-8").write(page)
    print("wrote %s (%.0f kB)" % (PAGE, len(page) / 1024))


def capture():
    os.makedirs(FRAMEDIR, exist_ok=True)
    for f in os.listdir(FRAMEDIR):
        if f.endswith(".png"):
            os.remove(os.path.join(FRAMEDIR, f))
    for i in range(FRAMES):
        out = os.path.join(FRAMEDIR, "f%03d.png" % i)
        cmd = [
            CHROME, "--headless=new", "--disable-gpu", "--hide-scrollbars",
            "--force-device-scale-factor=1", "--window-size=1280,1024",
            "--virtual-time-budget=8000", "--screenshot=" + out,
            "file://" + PAGE + "#f=%d" % i,
        ]
        subprocess.run(cmd, capture_output=True)
        if not os.path.exists(out):
            print("frame %d FAILED" % i)
            return False
        sys.stdout.write("\rcaptured %d/%d" % (i + 1, FRAMES))
        sys.stdout.flush()
    print()
    return True


def encode():
    seq = os.path.join(FRAMEDIR, "f%03d.png")
    apng = os.path.join(HERE, "img", "tess-replay.apng")
    mp4 = os.path.join(HERE, "img", "tess-replay.mp4")
    gif = os.path.join(HERE, "img", "tess-replay.gif")
    os.makedirs(os.path.dirname(apng), exist_ok=True)

    subprocess.run(["ffmpeg", "-y", "-framerate", str(FPS), "-i", seq,
                    "-plays", "0", "-f", "apng", apng], capture_output=True)
    subprocess.run(["ffmpeg", "-y", "-framerate", str(FPS), "-i", seq,
                    "-vf", "scale=1280:-2", "-c:v", "libx264", "-pix_fmt", "yuv420p",
                    "-crf", "20", "-movflags", "+faststart", mp4], capture_output=True)
    pal = os.path.join(FRAMEDIR, "pal.png")
    subprocess.run(["ffmpeg", "-y", "-i", seq, "-vf",
                    "scale=960:-1:flags=lanczos,palettegen=max_colors=192", pal],
                   capture_output=True)
    subprocess.run(["ffmpeg", "-y", "-framerate", str(FPS), "-i", seq, "-i", pal,
                    "-lavfi", "scale=960:-1:flags=lanczos[x];[x][1:v]paletteuse=dither=bayer:bayer_scale=3",
                    "-loop", "0", gif], capture_output=True)

    for p in (apng, mp4, gif):
        if os.path.exists(p):
            print("  %-24s %6.1f MB" % (os.path.basename(p), os.path.getsize(p) / 1e6))


if __name__ == "__main__":
    build_page()
    if "--page" in sys.argv:
        sys.exit(0)
    if capture():
        encode()

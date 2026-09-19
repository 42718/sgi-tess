# Six builds to the finished framework

Written 19 September 2026. The rule for this plan: **every build compiles with
MIPSpro on lucy, runs on the real cluster, and is validated by something you can
see or diff.** No build is "done" because the code looks right. Each one names
the check that closes it.

This supersedes the sequencing in DESIGN.md §9 milestones 1 and 2, which put a
Mac-hosted mock before any MPI. That order protects against slow iteration on
real hardware; it was judged not worth it here, because the machines are in the
room and a validated increment beats a simulated one.

Current state, build 0: `tess-node` and `tess-ui` exist and have never been
through MIPSpro. The portable half is C89 clean under `gmake gate`.

---

## Build 1 — It compiles, and the cluster paints a picture

**Do.** Get what exists through MIPSpro on lucy and aurora. Fix what it finds.
Run the headless path, then the GUI against a real master.

**Proves.** The frame protocol survives the wire between two machines of the
same endianness but different ABIs. `XCreateImage`/`XPutImage` works on V12 at
the depth lucy actually runs. The tile scheduler distributes and completes. The
epoch check does not reject everything.

**Delivers.** A distributed fractal viewer. Ugly, one window, click to zoom, but
real: aurora's four CPUs draw, lucy displays.

**Validated by.**

1. `mpirun ... lucy 1 tess-node -o a.ppm ... : aurora 4 tess-node -o a.ppm ...`
   writes a correct image.
2. **Determinism across rank counts.** The same parameters with `aurora 1`,
   `aurora 2` and `aurora 4` must produce **byte-identical** PPMs:
   `cmp a1.ppm a2.ppm && cmp a1.ppm a4.ppm`. This is the only real test of the
   `-OPT:roundoff=0:IEEE_arithmetic=1` discipline in DESIGN.md §8, and it is
   what stops seams appearing between tiles drawn by different machines.
3. The GUI shows tiles arriving centre-out, and a click re-renders.

**Risks.** MIPSpro will reject something; `XmNinputCallback` behaviour on a
`DrawingArea` needs `XmNresizePolicy` attention; `XtAppAddInput` on IRIX wants
`XtInputReadMask` cast exactly as written.

---

## Build 2 — The control window, generated from the parameter table

**Do.** Second top-level shell on the same app context. Settings pane built from
a `TessParamDesc` table rather than by hand, per DESIGN.md §5. Fields for centre
re/im, magnification, max iterations, escape. Colour section: palette, cycles,
rotate, interior colour. Elapsed/ETA. A log pane.

**Proves.** The descriptor table really can drive the pane, which is the claim
that makes a second module cheap. The GUI/master split survives two shells.
Recolour is local: changing palette, cycles or interior repaints with **zero**
bytes on the network.

**Delivers.** The interface in `design/ui-design.html`, minus the cluster half.

**Validated by.** Watching `netstat` or the master's tile counter while cycling
the palette: no traffic, instant repaint. Typing a centre and magnification and
getting exactly the frame a click-zoom to the same place produces.

---

## Build 3 — Progressive, cancellable, and honest about who drew what

**Do.** The ⅛-scale whole-image pass before full tiles. Cancel that abandons an
epoch in flight. Dirty-rect blitting instead of whole-window `XPutImage`.
Colour-by-owner toggle, tinting each tile by the rank that computed it.

**Proves.** Epochs work under pressure: fast repeated zooms never paint a stale
tile. The ⅛ pass removes the empty canvas for 1.6% of the work. Cancel returns
promptly rather than after the queue drains.

**Delivers.** A viewer that feels alive while you aim it, and a picture that
shows instantly whether a machine is contributing or idling.

**Validated by.** Clicking zoom six times rapidly: the final image must be
correct for the final position, with no fragments of earlier epochs. Colour-by-
owner on a `lucy 1, aurora 4` job shows four colours; on `aurora 1` it shows one.

---

## Build 4 — The GUI owns the cluster

**Do.** Flip the launch direction to the design's: `tess-ui` writes an `mpirun`
arguments file, forks and execs `mpirun -f`, and the master listens on
**loopback only** with a nonce the GUI passed it. Cluster panel: per-host CPUs
from `tess-probe` over `arshell`, rank spinner with `auto` and `0` legal,
detected-versus-configured, Test All, Rescan. Relaunch on change.

**Proves.** DESIGN.md §2's whole reason for the process split: changing the host
set relaunches the MPI job while the windows, the view and the log survive.
Aurora's shape is discovered, never stored, so 4 CPUs and 20 CPUs both work from
one profile.

**Delivers.** Cluster control from the GUI, and the end of hand-typed `mpirun`.

**Validated by.** Power aurora's extra bricks up or down, hit Rescan, watch the
panel change 4 → 20 and the rank count follow. Change the host set and confirm
the zoom position and log survive the relaunch. Confirm with `netstat` that the
master no longer accepts connections from anywhere but loopback.

**Note.** This is where the current public-port, no-nonce socket gets fixed. It
is a real security difference, not a cosmetic one.

---

## Build 5 — Telemetry: what the cluster is actually doing

**Do.** `MPI_SGI_stat_get()` in the master for per-transport byte counters, so
the panel reports **which interconnect is carrying traffic** rather than which
one was requested. Per-rank tiles/s, idle time, bytes. PCP read path via
`pmFetch` for `hinv.*`, `kernel.all.load`, `mem.freemem`, with the
`arshell`+`sysget` fallback already proven by `tess-probe`. Save as PPM and SGI
`.rgb` from the GUI.

**Proves.** The panel and PCP cannot disagree, because they read one source.
Transport is measured, not assumed — the lesson from the GM work.

**Delivers.** The performance monitoring half of the original goal: data flowing
back from the array nodes to lucy and being displayed.

**Validated by.** Run over TCP and confirm `BYTES_PT2PT_TCP` is nonzero and
`_GM` is zero. Save a `.rgb` and open it in `imgview`. Compare the panel's
memory and CPU figures against `tess-probe -v` on each host.

---

## Build 6 — Prove it is a framework, and cut the seam

**Do.** Add a second compute module — Julia sets are the cheapest honest choice,
orbit traps the prettiest — implemented **only** as a `TessModule` plus its
parameter table, with no edits to shell, scheduler, transport or panel. Then
introduce the link seam from DESIGN.md §6b: transport behind an interface, with
MPI as the only implementation for now.

**Proves.** The 85%-module-agnostic claim in §5, measured by the diff: if adding
a module touches the scheduler, the abstraction is wrong. The seam exists before
anything needs it, which is the only time it can be cut cleanly.

**Delivers.** Two modules, a module menu, and the place where the raw-GM
transport lands without disturbing anything above it.

**Validated by.** `git diff --stat` for the module commit: new files plus a
registration line, nothing else. Switching modules at runtime without a
relaunch.

---

## After the six

The framework is done and the GM work has somewhere to live:

- **Raw GM behind the seam.** Measured 50.9 MB/s against MPT-over-GM's 10.1, on
  the same wire, with our protocol. `doc/GM-MPT-INTEROP.md` has the numbers.
- **MPT over GM, correctly.** The host table plus translation on send and
  receive inside libgm, which is the honest fix for the identity problem the
  `GM_MPT_NODE_ID` shim only papered over. Same document, last two sections.

Either is a good winter project. Neither blocks anything above.

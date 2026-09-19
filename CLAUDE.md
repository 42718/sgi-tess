# Tess — distributed render framework for IRIX

**Status: builds 1-4 closed, build 5 in progress.** `tess-probe`, `tess-node` and `tess-ui`
all exist and render across lucy and aurora over TCP, byte-identical on both architectures;
see `doc/BUILD-PLAN.md` for what each build closed and what remains. `baseline/` holds the
2001-era originals this replaces; `tools/` holds small probes; `src/` holds the real tree.

**There is no git on lucy or aurora.** Version control happens on the Mac against the same
NFS tree the machines build from, so an edit is live on both the moment it is written and
there is nothing to pull.

**The repo lives on the NFS share at `/cluster/dev/sgi-tess`**, which is the same
directory reached through the share's mount point on whichever machine holds git, so every
machine builds natively from one checkout. It moved there on
18 September 2026; the old `~/dev/sgi-tess` is kept as `sgi-tess.moved-2026-09-18` until you
delete it.

Read these in order before doing anything:

| File | What it is |
|---|---|
| `doc/DEPLOY.md` | prerequisites, per-architecture build, running the UI, and the failure modes we hit |
| `doc/DESIGN.md` | **the engineering contract.** Decisions table in §0, open questions in §11. Change nothing here without saying why |
| `doc/PLATFORM-FACTS.md` | every platform claim, each citing the install medium it came from. Facts, not opinions |
| `doc/CHECKLIST.md` | the linear runbook. Steps 1–5 are the critical path; 6–10 are optimisations |
| `doc/BRINGUP.md` | the long-form version of the checklist, plus recorded hardware state |
| `doc/MPI-HELLO.md` | multi-host MPI bring-up: the smallest working job, and the six traps on the way |
| `design/ui-design.html` | the approved two-window mockup, published as an Artifact |

## Hard rules for this project

- **Target is real hardware.** IRIX 6.5.30, MIPSpro 7.4.4m, `-64 -mips4`. Not Linux, not gcc.
- **C89 only.** MIPSpro 7.4.4 is not a C99 compiler. No `//` comments, no declarations after
  statements, no `//`-style anything. `tools/pingpong.c` is the reference for the dialect.
- **Cite, don't recall.** Every platform claim in the docs names the file it came from. If a
  new claim can't be traced to an install image, a man page in `doc/mpt/man/`, or a terminal
  paste from the machines, it is a guess and must be labelled one.
- **One interconnect per MPI job.** MPT picks XPMEM → GSN → MYRINET → TCP for the whole job,
  not per host pair. This constraint drives the entire architecture; see DESIGN.md §2.
- **The GUI is a separate process from the MPI job.** IRIX MPT has no multi-host
  `MPI_Comm_spawn`, so the rank set is fixed at `mpirun` time.
- **The repo is private, and `doc/mpt/` is why.** Those headers carry SGI's
  "UNPUBLISHED PROPRIETARY INFORMATION" notice. Never advise making this repository public
  without excluding that directory first.
- **lucy contributes no compute by default** — it runs X, the GUI, the shading pass, the PCP
  collector and rank 0. See DESIGN.md §3b.

## The cluster

| Host | Machine | CPUs | Role |
|---|---|---|---|
| `lucy` | Octane2, VPro V12 | 2 × R14000 600 MHz | GUI + master + PCP collector, no compute |
| `arthur` | Onyx2, IR2/IE2 | 4 × R12000 400 MHz | 4 compute ranks |
| `aurora` | Origin 350 | 4 or 20 × R16000 | compute ranks = whatever is powered on |

Ethernet `172.28.4.8` / `.17` / `.16`. Aurora normally runs one brick (4 CPUs); five bricks
plus router plus L2 is a valid but power-hungry shape, and the software must not care which.

## Where bring-up actually stands

- MPT + Array Services: **installed and working on lucy and aurora.** `arshell` and
  multi-host `mpirun` both run, over GM and over TCP. arthur is not in the array yet.
- arthur's HIPPI: **parked.** Receive-side sync/LLRC failure, TX clean, firmware 4.0 valid.
  Next untried test is the spare board. State recorded in BRINGUP.md.
- lucy's HIPPI (`ess0`, Essential driver, not SGI's `hipXX`): configured, link never came up.
- GM to aurora: **2.0.8 builds and runs on both, and MPT now uses it.** `GM_MPT_NODE_ID=2` on
  both hosts reconciles the gmID check; the shim is on `myrinet-gm` branch `irix-2.0.8`. The
  earlier claim that this needed the mapper, and the mapper a switch, was wrong on both counts:
  the mapper maps a back-to-back pair when run on **both** ends, and `gm_get_node_id()` is a
  constant in GM-2 so no map would ever have satisfied MPT. **But MPI over GM measures
  10.1 MB/s against raw GM's 50.9**, so the gigabit-to-aurora decision in DESIGN.md stands and
  raw GM belongs behind the link seam. See `doc/GM-MPT-INTEROP.md`, last section.
- Gigabit lucy↔aurora: card is fitted (`tg1`), untested.

## Building

One tree, several machine types, all building at once into `build/$(uname -m)`:

```sh
gmake            # everything this machine can build
gmake info       # arch, build dir, compiler line, which sources exist
gmake probe      # tess-probe: inventory, no MPI, no X, builds anywhere
```

The per-architecture split exists so IP27, IP30, IP35 and the Mac do not overwrite each other's
objects on the shared filesystem. It is **not** licence to vary flags: DESIGN.md §8 requires
identical machine code across hosts so tiles agree at their boundaries. `scripts/tess-launch`
uses the split deliberately, giving each host group its own binary path via `mpirun`'s colon
form.

## Working conventions

- IRIX root shell is `csh` and does **not** honour `#` interactively — it globs what follows
  and aborts on an unmatched `?` or `*`. Strip glob characters from any command block written
  for pasting, or tell the user to type `sh` first.
- macOS `nm`/`readelf` cannot read IRIX MIPS objects. Parse big-endian MIPS ELF directly.
- **The SGI clocks run ahead of the Mac's, and the tree is shared over NFS.** A file written
  from the Mac arrives looking *older* than files the SGI made, so `make` skips it and reports
  success. Unpack archives with `tar xmf` (the `m` restamps with the local clock), and when a
  rebuild seems to do nothing, delete the derived files rather than trusting timestamps. This
  cost an hour on the GM work; see the `myrinet-gm` build notes.
- SGI `inst` images: `uncompress -c`, then `gzip -dc`, then `col -b`. `doc/instx.py` does it.

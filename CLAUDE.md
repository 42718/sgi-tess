# Tess — distributed render framework for IRIX

**Status: design complete, nothing built.** No source for `tess-ui` or `tess-node` exists yet.
`baseline/` holds the 2001-era originals this replaces; `tools/` holds small probes.

Read these in order before doing anything:

| File | What it is |
|---|---|
| `doc/DESIGN.md` | **the engineering contract.** Decisions table in §0, open questions in §11. Change nothing here without saying why |
| `doc/PLATFORM-FACTS.md` | every platform claim, each citing the install medium it came from. Facts, not opinions |
| `doc/CHECKLIST.md` | the linear runbook. Steps 1–5 are the critical path; 6–10 are optimisations |
| `doc/BRINGUP.md` | the long-form version of the checklist, plus recorded hardware state |
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

- MPT + Array Services: **not yet installed.** This is checklist step 2 and blocks everything.
- arthur's HIPPI: **parked.** Receive-side sync/LLRC failure, TX clean, firmware 4.0 valid.
  Next untried test is the spare board. State recorded in BRINGUP.md.
- lucy's HIPPI (`ess0`, Essential driver, not SGI's `hipXX`): configured, link never came up.
- GM to aurora: three known blockers, listed in `doc/GM-MPT-INTEROP.md`.
- Gigabit lucy↔aurora: card is fitted (`tg1`), untested.

## Working conventions

- IRIX root shell is `csh` and does **not** honour `#` interactively — it globs what follows
  and aborts on an unmatched `?` or `*`. Strip glob characters from any command block written
  for pasting, or tell the user to type `sh` first.
- macOS `nm`/`readelf` cannot read IRIX MIPS objects. Parse big-endian MIPS ELF directly.
- SGI `inst` images: `uncompress -c`, then `gzip -dc`, then `col -b`. `doc/instx.py` does it.

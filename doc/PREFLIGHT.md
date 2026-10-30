# Preflight: checking a machine before blaming the software

Someone outside this project tried to bring Tess up and could not get the UI to
reach the backend. That is the single most likely failure, it has at least five
distinct causes, and every one of them is already written down in
[DEPLOY.md](DEPLOY.md) sections 1 and 7 — as prose, which means it only helps
the people who read it first and in full.

This is those two sections made executable.

## The three pieces

One check library, three callers, for the reason `tess_inventory.h` gives about
inventory: asking the same question more than one way is how the answers drift.

| | What | Needs |
|---|---|---|
| `src/common/tess_check.c` | the checks and their remedies | libc |
| `tess-preflight` | prints them, on any host and over `arshell` | libc |
| `tess-setup` | the Motif wizard over the same checks | Motif, X |
| `tess-ui` | a RAG light over the cheap subset | already Motif |

**The text tool exists because the Motif one cannot do its job.** A wizard that
needs Motif and a working X server cannot tell you that your Motif or your X
server is the problem, and it cannot run over `arshell` on a compute host that
has no display at all. So the checks live below both, and the wizard is a
front end rather than the thing itself.

`tess-setup` and the `tess-ui` indicator are **not built yet**. This document
describes all three so the check list can be reviewed once; the two Motif
pieces follow after `tess-preflight` has been run on real hardware.

## Running it

```
cd /cluster/dev/sgi-tess
./build/`uname -m`/tess-preflight
```

On a compute host, where Motif, X, root and the master port are irrelevant:

```
arshell aurora /cluster/dev/sgi-tess/build/IP35/tess-preflight -compute -q
```

| Switch | Default | What it does |
|---|---|---|
| `-tree <path>` | `/cluster/dev/sgi-tess` | where the shared tree is, on this host |
| `-port <n>` | `7333` | the master's loopback port, to test it is free |
| `-compute` | off | skip the display-host checks |
| `-q` | off | one line per check, no remedies |
| `-1` | off | a single summary line, for a caller that wants a light |

Exit status is the worst result: **0** all passed, **1** warnings only,
**2** something failed. Unknown switches are an error, deliberately —
DEPLOY.md section 5 records that `tess-ui` ignores them silently and calls it a
trap, so this does not repeat it.

## The checks, in dependency order

The order is the order to fix things in. A failure early makes the checks after
it meaningless, which is why dependants report `SKIP` with the name of what
blocked them rather than a second, misleading failure: "mpirun across the
array" failing tells you nothing useful when `arrayd` is not running.

| # | id | Asks | On failure |
|---|---|---|---|
| 1 | `arch` | `uname -m` is IP27, IP30 or IP35 | WARN. Other IP types are probably R4400 or R5000, which `-mips4` binaries cannot run |
| 2 | `irix` | IRIX 6.5.x | WARN. The statistics calls are 6.5 APIs |
| 3 | `abi` | this binary is 64-bit | FAIL. The build is `-64 -mips4` and MPT's 64-bit libraries are what Tess links |
| 4 | `cc` | `cc` on PATH identifies as MIPSpro | FAIL if absent, WARN if it is not MIPSpro. gcc cannot produce `-64 -mips4` |
| 5 | `gmake` | GNU make, as `gmake` | FAIL. The Makefile uses `:=`, `ifeq` and `VPATH` |
| 6 | `mpirun` | `mpirun` on PATH | FAIL. `tess-ui` forks it directly, so **root's** PATH must carry it |
| 7 | `libmpi` | `/usr/lib64/libmpi.so` | FAIL. Distinguishes "no MPT" from "32-bit MPT only" |
| 8 | `mpiext` | `/usr/include/mpi_ext.h` | WARN only. Without it the build sets `TESS_HAVE_MPI_EXT=0` and loses transport counters; everything else works |
| 9 | `arrayd` | `ainfo machines` answers | FAIL. MPT launches through `arrayd`, not rsh |
| 10 | `tree` | the tree is at this path and has a Makefile | FAIL. It must be the **same path on every machine**, because mpirun is given one binary path per host group |
| 11 | `build` | `build/$(uname -m)` has this host's binaries | FAIL. One machine building is not deployment |
| 12 | `stale` | `tess-node` is newer than the newest source | FAIL. The binary predates the source, so this host runs code the tree does not describe |
| 13 | `clock` | the tree's clock agrees with this host's | WARN. Measured, not assumed: writes a file into the tree and compares the mtime the filesystem gives it against `time()` |
| 14 | `probe` | `tess-probe` runs, has `busy=`, and used `sysget` | FAIL if it does not run; WARN if stale or on the `MP_SAGET` fallback |
| 15 | `root` | `geteuid() == 0` | FAIL, display host only. **This is the reported failure**: `arshell` needs root, so without it every host shows "no answer" and the UI never reaches the backend |
| 16 | `motif` | Motif 2.1 under `/usr/Motif-2.1` | FAIL, display host only. Compute hosts need none |
| 17 | `display` | `DISPLAY` is set | FAIL, display host only. Reads the variable only; `tess-setup` proves the server answers by opening it |
| 18 | `port` | the master's port is free | FAIL, display host only. A master from a previous job holding it is why "master closed the connection" happens |

### Why `root` is the one to read first

The symptom reported from outside was that the UI never reached the backend.
DEPLOY.md section 4 says it in one line:

> **`arshell` needs root here**, which is why the UI does too. Run as an
> ordinary user it returns nothing and every host shows "no answer".

That is almost certainly what happened, and it is a check, not a paragraph,
now. The other four candidates are `arrayd` not configured with those hostnames
(check 9), the tree not at the same path on every host (10), stale or missing
binaries on one host (11, 12), and a leftover master holding the port (18).

### The two checks that are not simply "does this file exist"

**`stale`** compares the mtime of `build/$(uname -m)/tess-node` against the
newest `.c` or `.h` beneath `src/`. This is the trap that cost an evening:
the SGI clocks run ahead of the Mac's and the tree is shared, so a file written
over NFS arrives looking *older* than the binary built from the previous
version. `make` then reports success having done nothing.

**`clock`** measures that skew rather than inferring it. It writes
`.tess-clock-probe` into the tree, stats it, compares the mtime the filesystem
assigned against this host's `time()`, and removes it. This is the only check
with a side effect, and it is deliberate: detecting the skew requires touching
the filesystem whose clock is in question.

## What this does not do

**It writes no configuration.** It reports, and it prints what you would type.
`tess_cluster.h` makes the same promise about `arrayd.conf` and this keeps it:
a tool that edits Array Services configuration on a machine it has just met is
a worse idea than a tool that tells you which line is missing.

**It does not test the cluster, yet.** Every check above runs on one host.
The genuinely cluster-wide tests belong to `tess-setup`, below, because they
need a host list and they take time:

- `arshell <host>` answers for each configured host
- each host's `uname -m` has a matching `build/<arch>` in the tree
- `tess-preflight -compute` over `arshell` on each host, which is why the text
  tool takes `-q`
- the clocks of all hosts agree with each other
- `mpirun` a trivial job across the whole array (`tools/mpi-hello.c`)
- **the real thing**: launch `tess-node` exactly as DEPLOY.md section 6
  describes, connect to the master on the loopback port with the nonce, and
  tear it down. That is the UI-to-backend link, tested without the UI.

That last one is the test worth having. Everything before it is a prerequisite
for it.

## Still to build

**`tess-setup`**, a Motif wizard on the display host. One row per check with a
green, amber or red light, the detail beside it, and the remedy in a pane
below, selectable so it can be copied into a terminal. A **Recheck** button per
row and for the whole list, because the point of a wizard is the loop: fix,
recheck, watch it go green. Then the cluster tests above, run in order, each one
unlocking the next.

**The `tess-ui` indicator.** A single light in the existing cluster panel
showing the worst status from the cheap subset, so you can see at a glance
whether a failed render is your fault or the machine's, and a button that
launches `tess-setup`. The expensive checks — the probe, the clock file, the
cluster tests — do not belong on a timer, so the indicator runs the cheap ones
and says when it last ran the rest.

## What lucy actually reported, 29 October 2026

First run on lucy (IP30), as an ordinary user: **15 pass, 2 warn, 1 fail.**
The three assumptions this document previously flagged are now facts read off
the machine rather than guesses:

| Check | What lucy said |
|---|---|
| `cc` | `MIPSpro Compilers: Version 7.4.4m` |
| `gmake` | `GNU Make 3.82` |
| `mpirun` | `/usr/bin/mpirun` |
| `libmpi` | `/usr/lib64/libmpi.so` |
| `mpiext` | `/usr/include/mpi_ext.h` present, so transport counters are on |
| `arrayd` | `ainfo machines` answered, 12 lines |
| `motif` | `/usr/Motif-2.1`, headers and lib64 both present |
| `display` | `DISPLAY=:0.0` |
| `irix` | `IRIX64 6.5` |

So the Makefile's assumptions about where things live are correct on this
machine, and `cc -version` does contain the word MIPSpro.

**The clock skew is 51 seconds.** Measured, not inferred: lucy's clock and the
clock stamping files in the shared tree disagree by that much. This is the
first time that number has been written down, and it is the mechanism behind
the `gmake fresh` rule. Note how close it runs to the margin the `stale` check
reported in the same run, 83 seconds between the newest source and the binary
built from it: a rebuild that happened inside a minute of an edit would be
indistinguishable from a stale one.

**The one failure was `root`, and it was correct.** The run was as an ordinary
user, which is exactly the condition under which `arshell` returns nothing and
every host shows "no answer" — the reported symptom. The check does its job.

### A false positive this run caught

`probe` warned "fell back off sysget(SGT_RMINFO)" on a machine where nothing
had fallen back. The check searched for the `via` line in the wrong text: the
machine-readable first line of `tess-probe` stops at `hippi=` and carries no
`memwhy` field at all — see `tess_inventory_line()` — while the `via` line
exists only in the `-v` block. The check would have warned on every machine
forever, including a healthy one.

Fixed by running `tess-probe -v` and searching the whole block. Recorded here
because a check that cries wolf is worse than a missing check: it trains you to
ignore the thing you built to be believed.

## Still open for the hardware owner

Two of the three guesses are settled above. One is only half settled, and the
clock question that appeared is now settled too.

1. **`arrayd`, the negative case.** Lucy's Array Services was running, so the
   check passed. What `ainfo machines` does when `arrayd` is *down* is still
   assumed: the check treats a non-zero exit or empty output as failure. If it
   instead exits zero and prints an error, the check passes when it should
   fail — and that is the one direction that matters, because a passing check
   is the one nobody looks at. Settle it by stopping Array Services once
   (`/etc/init.d/array stop`) and running `tess-preflight`, then starting it
   again.
2. **The `clock` threshold: settled at 2 seconds.** Lucy's 51 seconds was
   fixed on 29 October: the NFS server serves the RFC-868 time service and the
   SGIs run `timeslave` against it. The full account, with measurements, is in
   [PLATFORM-FACTS.md](PLATFORM-FACTS.md#clock-synchronisation). On 30 October
   lucy read `PASS clock tree and local clock agree within 0 s`, so the
   threshold came down from 30, which was chosen before any of that was known.
   It is 2 rather than 0 because the mtime and `time()` are both whole seconds.
   The check compares **epochs**, so it is immune to the `TZ` confusion that
   made lucy look an hour out immediately afterwards: that was Europe leaving
   CEST on 25 October, not a clock fault, and `date -u` on lucy was correct
   throughout.

3. **arthur and aurora still need `timeslave`.** Only lucy has been done. Any
   machine that builds in the shared tree needs the same treatment, or the
   `stale` check is comparing mtimes written by a disciplined clock against
   binaries built by an undisciplined one.

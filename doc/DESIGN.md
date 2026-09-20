# Distributed render framework for IRIX — design

Status: **design only, nothing built.** Visual design: `design/ui-design.html`.
Verified platform facts: `doc/PLATFORM-FACTS.md`. Baseline sources: `baseline/`.

Target: IRIX 6.5.30, MIPSpro 7.4.4m, MPT 1.9 (MPI 4.4), IRIX IM (Motif) 2.1.20 + libSgm.

## 0. Decisions taken

| Decision | Choice | Why |
|---|---|---|
| Name | **Tess** | a *tessera* is one tile of a mosaic — what the scheduler hands out. Binaries `tess-ui` / `tess-node`, resource class `Tess`, profiles in `~/.tess/` |
| ABI | **`-64 -mips4`** | Myrinet/GM in MPT is 64-bit only — SGI's IRIX GM library ships `lib64` and nothing else. Keeps MPI-2 one-sided available too |
| Compute layout | **1 rank per CPU, no threads in v1**; per-host count is editable and **0 is legal** | reversed on evidence: `mandel3.c` failed to load both of lucy's CPUs with pthreads. MPT places processes for us; see §3a |
| GUI host compute | **0 compute ranks by default** — master only | lucy also runs X, the GUI, the shading pass and the PCP collector. Worth 3–10% of throughput to keep the UI responsive; see §3b |
| Host-set config | UI writes an `mpirun` arguments file (`-f`); never touches `arrayd.conf` | `-f` is MPT's own format; `arrayd.conf` is root-owned and one-time |
| Iteration buffer | `u16` + `u8` = 3 B/px, auto-promote past 65535 | same wire cost as RGB8, and palette changes stay instant |
| Process topology | GUI + non-computing master on the display host | tile data crosses the network exactly once |
| Telemetry | **Performance Co-Pilot, both directions** | read `pmcd` via PMAPI; ship a `tess` PMDA so render metrics join system metrics |
| Transport | MPI now, **raw link seam designed in** | GM bypass to aurora is free at `-64`; raw `hippibp` to arthur comes later |
| Blit | X (+MIT-SHM) now, **GL as a switchable backend** | works on every head and over remote X; V12/IR2 measured later |
| Save formats | PPM P6 + SGI `.rgb` | no library, no endian question; `.rgb` opens in `imgview` directly |

### The cluster

| Host | Machine | CPUs | RAM | Link to lucy | Role |
|---|---|---|---|---|---|
| `lucy` | Octane2, **VPro V12** | 2 × R14000 600 MHz | 2 GB | — | GUI + master (rank 0) + PCP collector; **no compute by default** |
| `arthur` | **Onyx2**, 2 × dual-CPU nodeboards, **IR2/IE2 pipe** | 4 × 400 MHz | ~7 GB | **HIPPI** → tcp | 4 compute ranks; also a visualization host |
| `aurora` | **Origin 350**, single system image over NUMAlink, 5 bricks + router + L2 | 20: 8 × 1 GHz + 12 × 800 MHz R16000 | 20 GB (4 GB/brick) | **Myrinet** → GM 1.6 | compute ranks = whatever is powered on |

Addressing: ethernet `192.0.2.8` lucy, `.17` arthur, `.16` aurora; HIPPI `198.51.100.0/24` with
the host octet kept identical (`198.51.100.8`, `198.51.100.17`). Myrinet needs `203.0.113.0/24` only if
the GM build provides an IP interface — GM bypass itself uses no IP.

### aurora has two shapes, and the software must not care

Running all five bricks with the router and the L2 controller is expensive in power, so the
normal state is **the main brick only: a quad-CPU box with 4 GB, same hostname, same
everything**. Both shapes are legitimate configurations of the same host.

Aurora is **4 CPUs per brick**, so its two shapes are 4 CPUs / 4 GB and 20 CPUs / 20 GB.
The cluster totals follow from that:

| aurora shape | aurora CPUs | lucy | arthur | aurora | compute CPUs | ranks |
|---|---|---|---|---|---|---|
| **solo — 1 brick** (everyday) | **4** | 0 | 4 | 4 | **8** | 9 (1 master + 8 compute) |
| full — 5 bricks | **20** | 0 | 4 | 20 | **24** | 25 (1 master + 24 compute) |

Share of the work in each shape, with lucy contributing no compute (the default — §3b):

| aurora shape | arthur | aurora |
|---|---|---|
| solo | ≈ 29% | ≈ 71% |
| full | ≈ 8% | ≈ 92% |

Enabling lucy's second CPU as a compute rank adds ≈10% in the solo shape and ≈3% in the full
one. The rank spinner allows it; the default declines it.

So: **the per-host rank count defaults to `auto` and is resolved at preflight from the
detected CPU count**, never stored as a number. One profile works in both shapes, and the
panel shows detected-versus-configured so a half-powered cluster is visible rather than
mysterious. A stored `20` would silently oversubscribe a 4-CPU aurora by 5×.

The everyday shape is also the *better* development target — not just cheaper. At 8 compute
CPUs the two workers contribute 29 / 71 percent, which is a far more useful test of the load
balancer than one host doing 92% of everything. Power up all five bricks for benchmarks and
big offline renders, which is exactly what "Render to File" is for.

Aurora dominating in the full shape is not a problem to hide either — it is the clearest
argument for colouring tiles by owner, because the picture tells you instantly whether the
small machines are contributing or merely adding latency.

### Consequences of aurora being one image

- **Aurora runs 20 ranks, and MPI between them never touches a wire.** One IRIX image means
  MPT uses its on-host shared-memory path for all 20, which is the fastest transport anywhere
  in this cluster — faster than the Myrinet link that carries tiles in and out. So many ranks
  on aurora is the cheap option, not the expensive one.
- **NUMA placement is MPT's job, not ours.** Five NUMAlinked bricks mean memory has a home,
  and `MPI_DSM_DISTRIBUTE` exists precisely to pin each rank to a CPU spread across nodes.
  With one rank per CPU and each rank touching only its own tile buffers, locality is correct
  by construction — no first-touch discipline for us to get wrong.
- **Mixed clocks need no special handling.** In the full shape, 8 CPUs at 1 GHz and 12 at
  800 MHz simply pull tiles at different rates from the master queue. The dynamic scheduler
  already copes with a 25:1 spread between hosts; a 1.25:1 spread inside one host is noise.
- **`MP_NUMNODES` reports the shape.** 5 when fully powered, 1 when running the main brick
  alone — so it is also a cheap way for the panel to say which shape it is looking at.

### How the shape is discovered

Nothing about the cluster's size is stored as a number, so it has to be found. Three
mechanisms, used at three different moments:

**1. Preflight, to size the job — Performance Co-Pilot first.** We are already linking
`libpcp`, so the cheapest discovery is a `pmFetch` of the inventory metrics per host:

```
hinv.ncpu          hinv.cpuclock       hinv.physmem
kernel.all.load    mem.freemem         network.interface.*
```

Remote, uniform, no agent of ours, and a context that will not open is simultaneously the
reachability answer. This is the primary path. It does depend on `pmcd` running and on the
`hinv.*` metrics existing on IRIX — both are in `doc/HARDWARE-CHECKS.md` because neither is
verified yet.

**2. Preflight fallback — a `tess-probe` run over `arshell`.** When `pmcd` is absent, run a
tiny probe binary on each host and read one machine-readable line back:

```sh
arshell aurora /work/tess/tess-probe
# tess-probe 1 host=aurora cpus=4 online=4 mhz=1000 nodes=1 memkb=4194304 #            freekb=3350000 irix=6.5.30 mpt=1.9 abi=64 gm=1 hippi=0
```

A purpose-built probe beats parsing `hinv` output, whose format varies between machines and
IRIX revisions. It needs only Array Services, which MPI requires anyway, so it adds no new
dependency — and it doubles as proof that `arshell` works before `mpirun` is attempted.

**3. At `MPI_Init`, to verify — every rank self-reports.** Each rank's hello message carries
its own hostname, CPU count and memory. This is authoritative, because it is the actual
process on the actual machine, but it arrives too late to size the job. Its value is catching
the mismatch: if the job launched 4 ranks on aurora and aurora reports 20 online CPUs, the
panel says **under-subscribed** rather than quietly wasting sixteen processors — which is
exactly the failure mode a powered-up aurora invites.

One implementation, three callers: a single `tess_inventory()` function is compiled into
`tess-probe`, into `tess-node`'s hello path, and into the PMDA. Inventory is gathered one way
or it drifts.

**Rescan** sits next to *Test All* in the panel, because aurora's shape changes between runs
whenever you power bricks up or down, and the panel should be re-askable without a restart.

### Interconnect reality

- **CORRECTION — MPT uses exactly one interconnect per job.** An earlier draft of this section
  assumed GM to aurora and TCP-over-HIPPI to arthur could run in the same job. They cannot.
  `MPI(1)` is explicit: *"There will only be one interconnect configured for the entire MPI
  job"*, chosen XPMEM → GSN → MYRINET → TCP/IP, and a forced choice requires **every** host in
  the job to have that device or the job is terminated. So with arthur on HIPPI and aurora on
  Myrinet, a three-host job falls to **TCP/IP throughout**, and only a two-host lucy+aurora job
  gets GM. The consequences are set out below and they change what the link seam is for.
- **Myrinet is GM 1.6, self-built, working on both lucy and aurora.** SGI's own IRIX Myrinet
  package is irrelevant here — it was restricted to IP27/IP35 and never covered the Octane2,
  which is presumably why the 1.6 build exists.
- **The open question is whether MPT 1.9's bypass binds GM 1.6.** MPT was built against SGI's
  GM 1.4-based package and the GM API moved between 1.4 and 1.6. `doc/BRINGUP.md` §5 tests it
  with `MPI_GM_VERBOSE`. Two prerequisites: a **64-bit** `libgm` (MPT's GM support lives only in
  the 64-bit MPI library — the reason for `-64`), and a mapped fabric.
- **If MPT will not bind it**, the raw GM transport behind the link seam (§6b) becomes the
  obvious next piece of work rather than a someday item: a known, self-built GM, used directly
  for tile payloads while MPI keeps control. That is precisely the case the seam was cut for.
- **Whether Myrinet carries IP depends on that build.** SGI's package installed a GM driver only
  — no `if_gm`, nothing to `ifconfig`. If the 1.6 build includes the IP-over-GM emulation, MPT's
  TCP fallback lands on the fabric; if not, the fallback is ethernet, since there is no slower
  Myrinet to fall back to.
- When GM does bind, MPT probes XPMEM → GSN → GM → TCP and selects it by itself — but only if
  *every* host in the job has it.

- **CORRECTION (17 September 2026) — MPT does bind this GM, and no switch was needed.**
  `GM_MPT_NODE_ID=2` on both hosts satisfies MPT's gmID check, and MPT reports *"Using the GM
  (Myrinet) OS bypass interconnect"* on a real two-host job. The earlier reasoning here, that
  global node ids required the mapper and the mapper required a crossbar, was wrong twice:
  `gm_mapper` maps a directly cabled pair when run on **both** ends, and `gm_get_node_id()` is a
  constant in GM-2, so no map would ever have produced what MPT wanted. What this does **not**
  change is the gigabit decision below: MPI over GM measures **10.1 MB/s** against raw GM's
  **50.9**, so it is slower than the gigabit estimate, not faster. It raises the value of the
  link seam (§6b) instead. Full evidence in `doc/GM-MPT-INTEROP.md`, last section.

### What one-interconnect-per-job means for this cluster

| job | MPT's interconnect | note |
|---|---|---|
| lucy + aurora | **GM bypass** | the fast pair, and aurora is 64–89% of the work |
| lucy + arthur | TCP/IP, over HIPPI if named so | HIPPI has no bypass in MPT 1.9 anyway |
| all three | **TCP/IP throughout** | Myrinet is absent on arthur, so GM is off the table |

### The resolution: gigabit ethernet to aurora

Two options are ruled out by physics and inventory rather than by software:

- **aurora cannot take HIPPI.** The boards are 5 V and its PCI-X slots are 3.3 V. Not negotiable.
- **A switchless HIPPI star does not fit.** HIPPI is point-to-point without a switch, so a
  three-machine fabric centred on lucy needs two boards in lucy plus one each — four boards,
  and there are three. HIPPI therefore stays what it is: a good lucy↔arthur link.

So the answer is a **gigabit ethernet card in lucy, point-to-point to aurora**. It keeps the
whole job on TCP/IP — which satisfies MPT's one-interconnect rule with all three hosts present —
while still using both fabrics, because under TCP each *pair* resolves to its own interface:

```
lucy <-> arthur    HIPPI      (already in place)
lucy <-> aurora    gigabit    (one card + a cable)
everything else    100BASE-TX (admin, ssh, arrayd)
```

What it costs and buys, for a 1920×1200 frame against ~6.8 s of estimated compute:

| aurora's link | transfer | share of frame | software needed |
|---|---|---|---|
| 100BASE-TX today | 0.40 s | 5.9% | — |
| **gigabit, conservative** | **0.15 s** | **2.2%** | **none** |
| gigabit + jumbo frames | 0.07 s | 1.1% | none |
| GM bypass | 0.02 s | 0.3% | shared lib + shim + mapper |

**Gigabit lands within about one percentage point of GM using a path that already works.** That
retires the GM work from the critical path entirely — no `libgm.so` rebuild, no
`gm_register_cmd_memory` shim, no mapper bring-up — and it reuses the `203.0.113.0/24` subnet that
was freed when Myrinet turned out to need no IP at all.

GM stays interesting for a two-host lucy+aurora job, where MPT *will* select it and where the
three fixes in `doc/GM-MPT-INTEROP.md` would pay off. That is now a benchmark curiosity rather
than a dependency — which is the right place for it.

The link seam in §6b keeps its justification independently: MPT still cannot use two fabrics at
once, so anything later that is genuinely transport-bound has somewhere to go. But nothing in v1
needs it.
- **HIPPI to arthur is plain TCP under MPT 1.9**, which removed HIPPI-800 OS bypass. But the
  **`hippibp` bypass driver is still in the OS** (HIPPI 4.0 installs `if_hip`, `hippibp`,
  `harp8`, `hipcntl`, `hiptest`, `ifhipconfig`), so a raw low-latency path to arthur remains
  reachable outside MPI — see §7.
- **There is no `MPI_TCP_INTERFACE` variable.** Which interface the TCP path uses comes from
  name resolution and the `HOSTNAME` field in `arrayd.conf`. Each compute node sits on exactly
  one fabric, so one canonical name each is unambiguous — `arthur.hippi.local` on HIPPI, and
  plain `aurora` on ethernet since GM bypass uses no IP. `arrayd.conf` is identical everywhere
  and names each machine once;
  **`/etc/hosts` differs per host** and resolves each name to the best address reachable from
  there. The canonical name states the policy, each machine implements it with the board it
  actually has. Details in `doc/BRINGUP.md` §3.
- **arthur and aurora share no fabric, and never need one.** All tile traffic is a star through
  the master, so those two ranks never exchange a message. Their `/etc/hosts` entries for each
  other point at ethernet purely so resolution always succeeds.
- Which transport is *actually* carrying traffic is reported from
  `MPI_SGI_stat_get()`'s per-transport byte counters — measured, not assumed from config.

### arthur is also a graphics machine

The IR2 pipe means arthur may be wanted for visualization while Tess is running, or wanted
*as* the display host. So the GUI host is a profile field, not a hardcoded assumption.

This is where the GUI/master split pays off a second time. To visualise on the IR2, **move the
GUI to arthur and leave the master on lucy**: compute traffic still rides Myrinet into lucy,
and the finished tile stream then rides HIPPI out to arthur — the exact pair of machines that
share a HIPPI link. Moving the *master* to arthur would be the mistake, because then aurora's
89% share crosses ethernet.

| GUI on | master on | tile path | |
|---|---|---|---|
| lucy | lucy | Myrinet in, then loopback | default |
| **arthur** | **lucy** | Myrinet in, **HIPPI** out to the display | visualise on the IR2 |
| arthur | arthur | ethernet for 89% of the work | avoid |

The cost of the middle row is that tiles cross a wire twice — 6.9 MB per 1920×1200 frame at
3 B/px, a fraction of a second over HIPPI. The IR2 is also the natural target for a later
visualization module, which is one more reason the module contract keeps `compute()` free of
any X or GL dependency.

## 1. Where we start from

`baseline/mandel3-motif-panes.c` is the current program: one window, an
`XmPanedWindow` with the fractal in an `XmDrawingArea` beside a stats `XmText`
pane, two pthreads splitting the image in half, greyscale, B1/B3 click zoom,
`XtAppAddTimeOut(50ms)` polling for progress. It works and it is the right
skeleton to keep — the framebuffer/XImage/expose path in particular. What goes:
the side panel, the single-window layout, the fixed two-thread split, and the
greyscale-only colouring.

Also inherited: `baseline/mandelmpi-ppm-tiler.c`, a headless MPI row tiler that
writes PPM. Its row-granularity dynamic scheduler is the seed of the real
scheduler. Note it has a C89 violation MIPSpro will reject in strict mode
(`int w;` declared after statements, around line 171) — worth fixing on sight,
because the same mistake in new code is the most common MIPSpro surprise for
anyone used to modern compilers.

## 2. The decision that shapes everything

MPT 1.9 cannot spawn ranks across hosts at runtime. `MPI_Comm_spawn` is
declared in `mpi.h`, but the MPT 1.9 release notes enable MPI-2 process
management for Linux partitioned systems only, and on IRIX it is single-host at
best. **The rank set is therefore fixed for the lifetime of an `mpirun`.**

If the GUI were rank 0, then every change to the host list — the thing the
control window exists to do — would kill the windows and lose the view, the
zoom history and the log. So:

```
                       lucy  (Octane2, 2 x R14k)
        ┌────────────────────────────────────┐
        │  tess-ui        (no libmpi)      │  Xt loop, two top-levels
        │  ┌────────┐   ┌───────────┐        │  iteration buffer lives here
        │  │ render │   │  control  │        │  shade() -> instant recolour
        │  └────────┘   └───────────┘        │
        └───────┬────────────────┬───────────┘
        fork/exec│                │ loopback TCP + nonce (XtAppAddInput)
                 v                v
        mpirun -f cluster.args   ┌──────────────────┐
                                 │ rank 0   master  │  tile queue, epochs,
                                 │ (no compute)     │  stats aggregation
                                 └──┬────┬───────┬──┘
                        MPI_BYTE,   │    │       │   native structs
                     ┌──────────────v┐ ┌─v────────┐ ┌─v─────────────┐
                     │ rank 1  lucy  │ │ranks 2-5 │ │ ranks 6-25    │
                     │ 1 cpu         │ │ arthur   │ │ aurora        │
                     │ (loopback)    │ │ 4 cpu    │ │ 20 cpu, 5 NUMA│
                     └───────────────┘ └──────────┘ └───────────────┘
                                        HIPPI/tcp     Myrinet/gm
                                        ~8% of work   ~89% of work
                     one rank per CPU — no threads anywhere
```

**Put the master on the same box as the GUI.** Rank 0 does no computing, so it costs
almost nothing, and keeping it on `lucy` next to the GUI makes that hop a loopback socket —
tile data then crosses the network exactly once. Putting the master on `aurora` instead
would relay every tile twice; running the GUI remotely and displaying on `lucy` would push
~9 MB per refresh across the wire and lose MIT-SHM entirely. `lucy` spends one of its two
CPUs on the X server, the GUI and a nearly-idle master, and offers the other as a single
compute thread.

The GUI binds a listening socket on port 0 and reads the real port back with
`getsockname()`, so nothing is hardcoded and two instances can coexist. Rank 0 connects
*outward* to it and must present a nonce passed on its command line, or the connection is
dropped and logged.

Two binaries, and the split is enforced by the linker rather than by good
intentions:

| binary | links | never links |
|---|---|---|
| `tess-ui` | `-lXm -lSgm -lXt -lXext -lX11 -limage -lm` | `libmpi` |
| `tess-node` | `-lmpi -lm` (no `-lpthread` in v1) | Motif, Xt, X11 |

Consequences worth stating plainly:

- Changing the cluster relaunches the job; the windows never blink.
- The GUI cannot deadlock on MPI, because it has no MPI.
- A dead rank kills the job, not the session. That matters, because MPT offers
  no fault tolerance at all: `MPI_Abort` takes down the whole job whatever
  communicator you pass, and `MPI_ERRORS_RETURN` does not make a dead rank
  survivable. Prevention (preflight) plus fast relaunch is the only honest
  design.

## 3. Scheduling

- **Tiles, not rows.** 64×64 default, dynamic hand-out from a master queue,
  credit-based prefetch of two tiles per worker so a worker is never idle
  waiting for a reply.
- **Centre-out order**, so the interesting part of the frame resolves first.
- **Two passes, not four.** A ⅛-scale whole-image pass costs 1.6% of the work
  and removes the empty canvas entirely; then full-resolution tiles. Adding ¼
  and ½ levels triples the bookkeeping and buys nothing a human can see.
- **Epoch counter on every message.** Cancel and re-zoom bump the epoch; tiles
  from an abandoned render are recognised and dropped. This is what makes
  cancel instant and non-blocking — no `MPI_Cancel`, no collectives on the
  interactive path.
- **Pause drains.** Stop handing out tiles, let in-flight work land, keep the
  image. Resume continues the same epoch.
- `MPI_Iprobe` once per tile row inside `compute()` gives sub-tile abort latency
  on deep-zoom tiles that take seconds.

## 3a. Per-node parallelism — and why mandel3 only used one CPU

**MPI does not manage threads.** MPT gives you *processes* — ranks — and places them.
Threads inside a rank would be entirely our own code, our own bugs, and on IRIX that turns
out to be a real trap. Which is what `baseline/mandel3-motif-panes.c` already demonstrated.

Two independent things are wrong in that file, and it is worth separating them because only
one is IRIX's fault.

**1. Nothing ever raises the concurrency level.** `mandel3.c` calls `pthread_create` twice
and assumes the kernel spreads the threads across CPUs. IRIX pthreads are an M:N
implementation — POSIX threads are multiplexed onto a smaller number of kernel execution
vehicles, and the process does not get more of them just because it created more threads.
The documented knob is `pthread_setconcurrency()`, and there is no call to it anywhere in the
baseline (nor any `sysmp`, `runon` or affinity call). So both workers can be time-slicing a
single CPU while `top` shows the process at 100% and everything *looks* fine.

**2. The work is split into static halves.** Even with two CPUs genuinely running, the split
is `rows 0..h/2` and `h/2..h`. Mandelbrot cost per row varies enormously — interior rows cost
`max_iter` per pixel, exterior rows escape in a few iterations. So one thread finishes early
and the other runs on alone. A *correctly* threaded mandel3 would still only show roughly
1.3× on two CPUs, not 2×. This is the same reason the new design uses a dynamic tile queue
rather than dividing the image up front, and it is why the fix for (1) alone would have been
disappointing.

### So: one rank per CPU, and no threads in v1

Four ways to fill a multiprocessor from this program, honestly ranked for our case:

| approach | who places it on CPUs | verdict |
|---|---|---|
| **many MPI ranks** | MPT + `MPI_DSM_*` | **chosen.** Separate processes, guaranteed separate scheduling, on-host shared-memory transport, NUMA placement already solved by `MPI_DSM_DISTRIBUTE` |
| OpenMP via MIPSpro `-mp` | SGI's runtime (`MP_SET_NUMTHREADS`) | good second option. One directive on the tile loop, no thread code of ours. Worth measuring on aurora later |
| pthreads + `pthread_setconcurrency` | us, by hand | the thing that already bit you. Also drags in MPT's thread-safety question |
| `sproc(2)` | us, IRIX-native | 1:1 with CPUs and what SGI's own code used, but cannot be mixed with pthreads and is a dead end |

Consequences of choosing ranks, all of them simplifying:

- **The "did it use every CPU?" question becomes trivially checkable.** 26 ranks means 26
  processes; `osview` shows 26 CPUs busy or it does not. No hidden multiplexing.
- `MPI_Init_thread` and MPT's thread-safety level leave the critical path entirely. `tess-node`
  does not even link `-lpthread` in v1.
- `MPI_STATS` becomes usable, since the not-thread-safe warning no longer applies.
- Memory cost is negligible: a rank's working set is one tile (64×64×3 B = 12 KB) plus MPI
  buffers, on machines with 2–20 GB.
- On aurora all 20 ranks talk over shared memory, which is faster than the Myrinet link
  carrying tiles in and out — so the rank count costs nothing in communication.

The per-host rank count stays editable in the cluster panel, so OpenMP-per-rank remains a
measurable experiment rather than a rewrite: it would change `compute_tile()` and nothing else.

## 3b. Lucy's budget — why the GUI host contributes no compute

Lucy has two R14000s and five jobs competing for them:

| job | cost | character |
|---|---|---|
| X server | idle between blits, spikes on each one | latency-sensitive |
| GUI: shade + blit | ~1–2% of a CPU steady state; 23–46 ms shade + ~61 ms blit per full-frame recolour | bursty, interactive |
| master (rank 0) | ~265 tile messages/s of bookkeeping, no computing | must not busy-wait |
| PCP collector | one `pmFetch` across three hosts every 2 s | negligible |
| a compute rank | **pegs an entire CPU at 100% for the whole render** | throughput |

The first four together are a small fraction of one CPU. The fifth consumes one outright.
So with a compute rank on lucy, everything interactive shares the remaining CPU with the X
server; without one, it has both.

**The trade is 3–10% of throughput against interactive latency, and latency wins.** The
number that matters is not average load but whether a CPU is available *the moment* you drag
a rubber-band zoom box or rotate the palette. A pegged CPU and a nice-level fight are exactly
how a 1990s workstation UI comes to feel like treacle, and no amount of throughput compensates
for a render window that stutters while you are trying to aim it.

So: **rank count 0 on the GUI host by default.** The mechanics:

- `0` is a legal value in the per-host rank spinner, distinct from unticking the host — an
  unticked host leaves the array entirely, a host with 0 ranks stays in the job as master
  and/or telemetry source but computes nothing.
- The node table shows the role explicitly (`master only` rather than a bare `0`), so the
  state is legible rather than looking like a mistake.
- The `mpirun` line becomes `lucy 1, arthur 4, aurora 4` — one rank on lucy, which is rank 0,
  the master.
- `MPI_NAP=2` matters more here than anywhere: an idle master that spin-waits would defeat
  the entire point of this decision.
- If the GUI is moved to arthur (§0, visualising on the IR2), the same default applies to
  *that* host instead — it follows the GUI, not the hostname.

Turning it back on is one click, and worth doing when you leave a long render unattended:
there is no interactive latency to protect if nobody is watching. That is also the honest
argument for making it a toggle rather than a hardcoded rule.

## 4. What crosses the wire

Two protocols, deliberately different:

| link | encoding | why |
|---|---|---|
| GUI ↔ rank 0 | tagged, length-prefixed, **big-endian** frames | survives a little-endian dev build; versioned |
| rank 0 ↔ workers | native fixed-layout C structs over `MPI_BYTE` | MPT has no heterogeneous clusters, so native layout is safe and free |

Tile payload is **`uint16` iteration count + `uint8` smooth fraction = 3 bytes
per pixel** — the same wire cost as RGB8 and strictly more useful, because the
GUI can then recolour without the cluster. Palette, cycles, rotation and
smoothing become instant and local. At 778×716 that is 1.6 MB; at 2048² it is
12 MB. This single choice is why the colour section of the control window has no
Apply button.

The `uint8` fraction resolves 1/255 of an iteration, finer than any 256-entry palette
can express, so it is not the limiting factor. If `maxiter` ever exceeds 65535 the buffer
**promotes automatically to `uint32` + `uint8`** (5 B/px) with a one-shot note in the log
— a render that deep on this hardware is a multi-hour job anyway, so the extra 2 bytes
are free by comparison.

## 5. The module contract

```c
/* tess.h — a module is five functions and a parameter table */
typedef struct TessModule {
    const char  *name;
    TessParamDesc *params;                 /* drives the settings pane */
    int          npar;
    size_t (*state_size)(void);          /* bytes of kept result per pixel */
    void   (*plan)   (TessJob *, TessTileList *);
    void   (*compute)(const TessJob *, const TessTile *, void *out);   /* worker */
    void   (*shade)  (const void *in, int n, const TessPalette *, unsigned *rgb);
} TessModule;
```

`compute()` never touches X. `shade()` runs only in the GUI. `params` is a
descriptor table, so the settings pane is **generated**, not hand-built — that
is the piece that makes a second module cheap, and it is worth building properly
even though it looks like over-engineering for one application. Roughly 85% of
the code is module-agnostic: shell, scheduler, transport, cluster panel,
preflight, imaging, save pipeline.

## 6. Cluster configuration

Three layers, with a clean ownership split. The one rule that matters: **the UI never
writes `arrayd.conf`.** That file is root-owned and one-time, and an application that
rewrites it will eventually break the cluster silently. The UI *validates* it — `ainfo
arrays`, `ainfo dfltarray`, `ascheck`, `arshell <host> /bin/true` — and reproduces the
setup checklist under Help.

**Layer 1 — Array Services, once, by hand or `arrayconfig`:**

```sh
arrayconfig -d -i -m -a sgicluster lucy arthur aurora
```

In `arrayd.auth`, **comment out `AUTHENTICATION NOREMOTE`** — only the last
authentication entry read takes effect, so leaving it in place silently defeats a
`SIMPLE` entry added below it. Then `arrayd -c -f`, `/etc/init.d/array restart`,
`ascheck`. Note `arrayd` performs the `ruserok()` check, which is why `~/.rhosts` is
needed even though `rsh` is not used — and that `mpirun localhost 2 a.out` treats
`localhost` as a remote host.

**Layer 2 — the UI's own profile**, `~/.tess/hosts/<name>.hosts`, plain key=value, one
block per host, holding address, role, ranks, threads, colour, plus the environment
overrides. This is what "Save Profile…" writes.

**Layer 3 — the `mpirun` arguments file**, regenerated on every launch and shown in the
launch row:

```
-v -a sgicluster -d /work/tess
lucy 1, arthur 1, aurora 1
./tess-node -gui-host lucy -gui-port 5123 -nonce 9f3c1a77c204e18b
```

Prefer the **comma form** over colon-separated entries: one executable, one entry
object, ranks assigned in host order, rank 0 on the first host — the fewest moving parts
for `mpirun`'s entry-object parser. The colon form is still there for the day two hosts
need different binaries.

Traps designed around, each verified in `mpirun(1)`:

- **No colons in any argument** — `:` separates entries. Parameters travel via
  the arguments file and the socket, never as colon-bearing argv.
- **Remote working directory defaults to `$HOME`, not `$PWD`** — the single
  most common multi-host failure. Always emit `-d`.
- **stdin must be redirected** for a backgrounded job; **stdio cannot carry
  binary**, so tiles need the socket and `mpirun`'s stdout is reserved for the
  log pane.
- `mpirun` creates the equivalent of a fresh login session per job, so `.cshrc`
  can override an environment you thought you had exported.
- The child gets `dup2(open("/dev/null"), 0)` and `setsid()` before `execvp` —
  backgrounded MPI jobs otherwise die intermittently on `SIGTTIN`.
- No numeric hostnames, and the binary is invoked as `./tess-node` because `.` is not
  assumed to be on `$PATH`.

Environment the supervisor sets for the job: `MPI_NAP=2`, `MPI_BUFS_PER_HOST=128`,
`MPI_MSGS_PER_HOST=4096`, `MPI_CHECK_ARGS=1`. Never `MPI_STATS` or `mpirun -stats`,
because that code is not thread-safe and the worker is threaded.

Preflight, before `mpirun` is allowed to run: non-blocking `connect()` to
`sgi-arrayd` 5434/tcp with a `select()` timeout on every configured host. This
is the only approach that neither blocks the GUI nor hangs the way `mpirun`
does on an unreachable node.

## 6a. Telemetry via Performance Co-Pilot

PCP replaces most of the bespoke statistics layer, and it does it with a supported remote
API instead of `arshell` round trips.

**Reading.** The GUI links `libpcp` and opens one PMAPI context per host —
`pmNewContext(PM_CONTEXT_HOST, "aurora")` — then fetches a fixed metric list on the slow
timer. CPU, memory, load, per-interface bytes and errors all arrive uniformly across hosts,
with no agent of ours to write and no parsing of command output. A context that fails to
open *is* the reachability answer for the stats path, and it is cheap.

`pcp_eoe` is part of the base IRIX Foundation set — not on the Applications CDs — so `pmcd`
is probably already running. The visualisation tools (`pmchart`, `pmview`) came with the
licensed PCP product, so Tess's own panel stays the primary UI and PCP tools are a bonus.
Exact IRIX metric names must be confirmed with `pminfo` on the box; they differ from the
Linux PCP names people quote.

**Writing.** Tess ships a `tess` PMDA so render metrics live in the same namespace as system
metrics, which means `pmchart` can plot render rate against CPU and link load on one chart,
`pmlogger` can archive a whole render session, and `pmie` could alarm on a stalled job.

Lifetime is decoupled deliberately: the master writes a fixed-layout **mmap'd stats file**
and the PMDA maps it read-only. So `pmcd` does not care when Tess starts or exits, and a
stale file reports "no values" rather than lying.

```
tess.job.state          tess.rank.tiles          (instance domain: rank)
tess.job.epoch          tess.rank.mpix_per_sec
tess.job.tiles_total    tess.rank.tile_ms_median
tess.job.tiles_done     tess.host.share          (instance domain: host)
tess.job.mpix_per_sec   tess.link.bytes          (instance domain: transport)
tess.job.eta_sec        tess.queue.depth
```

Because both the panel and PCP read the same source, they cannot disagree — which is the
whole point of doing it this way rather than bolting on a second set of counters.

Fallback: on a host with no `pmcd`, the panel falls back to the `arshell` + `sysget` path
and marks the row as degraded rather than empty.

## 6b. The link seam

The goal is that Tess can use the low-latency links, without betting correctness on them.

```c
typedef struct TessLink {
    const char *name;                                  /* "mpi" | "gm" | "hippibp" */
    int  (*probe)  (const TessPeer *);                 /* is this usable to that peer? */
    int  (*setup)  (TessPeer *, const void *addr, size_t);
    int  (*send)   (TessPeer *, const void *, size_t);
    int  (*recv)   (TessPeer *, void *, size_t, int *);
    void (*teardown)(TessPeer *);
} TessLink;
```

Rules that keep this safe:

- **Control always travels on MPI** — work assignment, epochs, cancel, telemetry. Only bulk
  tile payload is eligible for a raw link.
- **MPI carries the handshake.** Link addresses (GM board/port ids, HIPPI ifield/ULA) are
  exchanged over MPI at epoch start, then the raw link is used for data. No out-of-band
  configuration, nothing to keep in sync by hand.
- **Any raw-link failure degrades to MPI** and logs it. A fast path that cannot fail closed
  is not worth having.
- v1 ships `link_mpi` only. GM to aurora needs no work at all, because MPT selects GM itself.
  `link_hippibp` is the later piece, and the HIPPI Administration Guide on the media plus
  `/usr/include/sys/hippi.h` are the references for it.

## 6c. Blit backends

One pixel packer, three ways to get it on screen, chosen at runtime with `-blit`:

| backend | call | when |
|---|---|---|
| `shm` | `XShmPutImage` | default on a local display, if MIT-SHM is present |
| `x` | `XPutImage` | remote display, or no MIT-SHM |
| `gl` | `glDrawPixels` in a `GLwMDrawingArea` | measured later on lucy's V12 and arthur's IR2 |

The pixel format lives in exactly one place, because GL wants a specific component order and
X wants whatever the visual's masks say. That single packer is built from the visual masks at
runtime — never from a hardcoded `0x00RRGGBB` — which is also the bug the little-endian macOS
build structurally cannot catch.

## 7. Environment to set explicitly

`MPI_BUFS_PER_HOST` (biggest win for many small messages), `MPI_MSGS_PER_HOST`,
`MPI_REQUEST_MAX`, `MPI_TYPE_MAX`. And `MPI_NAP`, which stops an idle rank from
pegging a CPU — it matters because rank 0 and one worker share `lucy` with the
X server and the GUI. For job layout diagnostics use `mpirun -v`, not an environment variable.

`MPI_STATS` only prints at `MPI_Finalize` to stderr; the live counters in the
cluster panel come from `MPI_SGI_stat_get()` (50 counters, `mpi_ext.h`), which
also reveals **which interconnect is actually carrying traffic** — nonzero
`BYTES_PT2PT_{TCP,GM,GSN,SHMEM}`. Note it is not thread-safe, so it is called
only from the master's single MPI thread.

## 8. Build environment

Two targets, one source tree, C89 throughout.

```sh
gmake        # on an SGI: MIPSpro into build/`uname -m`. Authoritative.
make         # on the Mac: clang -std=c89 -pedantic. The portability gate.
```

**CORRECTION (18 September 2026) — no rsync, no ssh.** This section originally proposed
`make sgi` pushing the tree to an SGI over ssh. Neither lucy nor aurora runs sshd, and the tree
now lives on the NFS share at `/cluster/dev/sgi-tess`, so every machine builds natively from one
checkout instead. Output goes to `build/$(uname -m)` — `IP27`, `IP30`, `IP35`, `arm64` — so
three machine types can build at once without overwriting each other's objects, and
`scripts/tess-launch` hands each host group its own binary path through `mpirun`'s colon form.

The split is about the shared filesystem, **not** about per-machine code: the flags below stay
identical everywhere, for the reason given in the first bullet — identical machine code is the
only guarantee of identical pixels across hosts.

The authoritative line:

```sh
# GUI: Motif, no MPI, no pthread
cc -64 -mips4 -O2 -I/usr/Motif-2.1/include \
   -o tess-ui <ui sources> \
   -L/usr/Motif-2.1/lib64 -Wl,-rpath,/usr/Motif-2.1/lib64 \
   -lXm -lSgm -lXt -lXext -lX11 -limage -lm

# NODE: MPI + threads, no Motif. -lmpi last among ordinary libs, -lpthread after it.
cc -64 -mips4 -O3 -OPT:roundoff=0:IEEE_arithmetic=1 \
   -o tess-node <compute sources> -lmpi -lpthread -lm
```

Flag notes that matter for a fractal:

- **Write the ISA explicitly.** `-64` already defaults to `-mips4`, but say it anyway —
  `-n32` defaults to `-mips3`, so an ABI change would silently change the ISA. R14000 and
  R16000 both implement MIPS IV, so one binary covers `lucy`, `arthur` and `aurora`, and
  identical machine code is the only real guarantee of identical pixels across them.
- `-O3` silently sets `-OPT:roundoff=2`, and `-TARG:madd` is on by default for `-mips4`
  (single rounding on multiply-add). Pinning `-OPT:roundoff=0:IEEE_arithmetic=1` keeps
  the useful optimisations — LNO, unrolling, software pipelining, prefetch — while
  forbidding exactly the reassociation and reciprocal transforms that would shift the
  escape-time boundary and produce **visible seams between tiles drawn by different
  machines**.
- Never `-Ofast` (implies `-O3 -IPA` and `roundoff=3`, and can raise the ISA), never
  per-host `-TARG` tuning, never `-c99` (coverage in 7.4.4m is undocumented), and
  **never `-ansi`** — it tightens the feature-test macros and hides POSIX/SGI
  prototypes. Stay in the default `-xansi`, which is C89 plus the SGI/POSIX extensions.
- Add `-Wl,-rpath,/usr/Motif-2.1/lib64` to the GUI link so it finds `libXm.so.2` at
  runtime. `motif_eoe` installs both `lib32` and `lib64` variants, so `-64` is covered.
- Verify with `elfdump -Dh tess-node | head -30` — expect ELF 64-bit MSB, MIPS, MIPS-IV,
  dynamically linked. (The existing `baseline/mandel3` reports 32-bit MSB N32 MIPS-IV,
  which is what the old n32 build produced — the new binaries should differ.)

**Never mix Motif 1.2 and 2.1 objects or libraries in one binary** — SGI documents this as
causing unpredictable crashes. Every 2.1-only widget therefore goes through a factory
function in one file, so that if `motif21_dev` turns out to be missing on a machine, a
single `-DRD_MOTIF12` selects the 1.2 bodies instead of touching call sites.

The X resource class is the **framework**, not the module (`Rdfw`, not `Mandel`) —
otherwise the second application needs its own app-defaults file.

The macOS build is a **portability gate, not a substitute**: macOS arm64 is
LP64 little-endian, IRIX n32 is ILP32 big-endian, so the Mac build differs on
both axes and will hide exactly the bug classes that matter — pointer/long size
and byte order. It is still worth having, because it catches everything else in
seconds instead of a round trip, and it lets the UI be iterated without an SGI.
Mac prerequisites, all confirmed available with arm64 bottles: `openmotif`
2.3.8, `open-mpi`, and XQuartz 2.8.6 (cask). Motif 2.3.8 is a looser gate than
IRIX's 2.1.20, and `libSgm` has no Mac equivalent — so Sgm widgets sit behind a
thin wrapper with a plain-Motif fallback.

Rejected, with reasons: a real cross-compiler is now genuinely possible (the
IRIX GCC port was revived out-of-tree at 9.2, and clang/LLD needs only a small
patch, since Homebrew's LLVM already carries the MIPS backend) but it needs a
sysroot copied off the SGI and it still is not MIPSpro, which is what the
binaries must ultimately be built with. MAME boots IRIX 6.5 on the Indy only,
caps at 6.5.22, and runs slower than the hardware in the room — so
emulation-as-CI is pointless here. Details in the research notes.

## 9. Milestones

The sequencing rule that matters: **the GUI must be fully usable against a mock before
any MPI exists.**

0. **Bring-up spike — half a day.** Install `mpi`, `sma`, `mpt`, `arraysvcs` from the
   August 2006 CD, then get the *existing* `baseline/mandelmpi-ppm-tiler.c` running under
   `mpirun` across lucy, arthur and aurora. Do the `.rhosts` / `AUTHENTICATION SIMPLE` / `ascheck` /
   `jlimit files` dance once and **write down the exact working command line**. Fix the
   tiler's C89 slip while you are in there. Deliverable: `doc/CLUSTER-SETUP.md` and a
   command line that works.
1. **Wire format + mock, tested on the Mac.** Frame header, encode/decode helpers,
   message table, parameter-file reader/writer — unit-tested against **golden hex
   vectors on the little-endian Mac**, which is where byte-order bugs are cheap to find.
   Plus a ~200-line fake master that computes tiles locally and speaks the real protocol
   over a socket.
2. **The whole GUI against the mock.** Both shells on one app context, the socket input
   source, the model, colouring, the dirty-rect blitter, overlays, zoom/pan/rubber band,
   the status line, the generated settings pane, the cluster half fed with synthetic
   stats, the log. **Hard rule: do not start milestone 3 until this feels good.** There
   is no MPI risk anywhere in this picture.
3. **Real worker binary, launched by hand.** Master scheduler with epochs, credits and
   centre-out order; worker receive loop; the `sizeof`/magic handshake. Launched manually
   with `mpirun -v -d … lucy 1, arthur 1, aurora 1 ./tess-node -gui-host lucy -nonce …` while
   the GUI listens. **First real distributed render — a frame drawn by three SGIs.**
4. **Job supervisor.** fork/exec `mpirun`, `/dev/null` on fd 0, `setsid`, log capture,
   SIGCHLD self-pipe, soft cancel/pause, the args-file writer, and a launch-failure
   dialog that lists the five known causes. One-click render.
5. **Cluster half for real, on PCP.** PMAPI contexts per host, the metric list, and the
   `tess` PMDA writing through an mmap'd stats file — plus the `arshell`+`sysget` fallback for
   a host without `pmcd`. Preflight children — an arrayd TCP probe plus one `arshell`
   round trip running `uname -srm; hinv -c processor -t; hinv -c memory; uptime; test -x
   ./tess-node && echo BINOK; versions -b | grep -i mpi` — then the stats layer, rank
   telemetry, node table, tile map, host editor, profiles. Every column has a verified
   source; a host that is up but has `arrayd` down reads as unreachable, correctly.
6. **Module boundary.** Refactor the Mandelbrot behind the module struct and switch the
   top half to the *generated* pane. **Falsifiable check: the generated pane must
   reproduce the hand-built one exactly.**
7. **Robustness.** Watchdog, tile reissue, a partial-result state, backpressure, MIT-SHM and
   the `gl` blit backend measured against it on lucy's V12,
   `MPI_SGI_stat_get` counters and the interconnect row, save-with-template and the `.rgb`
   writer. Acceptance test: **pull `arthur` off the network mid-render and finish the frame on
   the other two** — fault tolerance built entirely outside MPI.
8. **Raw HIPPI link (optional).** `link_hippibp` behind the seam from §6b, handshake over
   MPI, tiles over bypass, automatic fallback. Measured against TCP-over-HIPPI before it is
   allowed to become the default.
9. **A second module.** A 40-line checkerboard permanently in the tree to keep the seam
   honest, then something real that is not a fractal. A new application should cost a
   descriptor table plus five callbacks.

## 10. Framework boundary — where this honestly stops

The shell covers **embarrassingly parallel rectangular output**: fractals, ray tracing,
image filters, per-pixel Monte Carlo, distance fields. An N-body or PDE solver is
bulk-synchronous with global communication every step and cannot use a tile queue.

So the seam is cut in advance: the master entry point takes the scheduler as a
parameter, and the tile scheduler is only the first one. Such a module would still reuse
both windows, the cluster half, the protocol, the log and every dialog — replacing only
the scheduler and the status semantics ("step 412 of 10000" instead of "72/192 tiles").
Even the tile map survives, degenerating to N cells coloured by owner and state. That
reasoning belongs in a comment so nobody hard-codes tile geometry into the map.

Generalising stops there. Anything needing all-to-all communication per step is a
different framework wearing this one's clothes.

## 11. Open decisions

See the end of `design/ui-design.html` for the full list with recommendations.
The blocking ones: n32 vs 64-bit (n32 first, but Myrinet/GM is 64-bit only in MPT), the
real machine inventory, a project name, and ranks vs threads per host.

**One rank per host, threads = CPUs** — decided (see §0). MPI is called only from each
rank's main thread, so MPT's lack of default thread safety is respected regardless of the
granted level. Two things this still depends on:

- The thread-level probe in `doc/HARDWARE-CHECKS.md`. If `MPI_Init_thread` will not grant
  at least `FUNNELED`, the thread pool is illegal and the layout falls back to ranks.
- Thread concurrency is set with `pthread_setconcurrency()`, not an environment variable,
  and 20 threads in one address space on `aurora` needs its memory locality checked. If it
  disappoints, the fallback is several pinned ranks with `MPI_DSM_CPULIST` — which is why
  the per-host ranks×threads control ships in v1 rather than v2.

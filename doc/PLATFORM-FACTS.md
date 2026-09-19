# Platform facts — verified from the install media in ~/Downloads

Everything here was read out of Rutger's own IRIX distribution media, not from
memory or the web. Provenance is given per fact so it can be re-checked.
Extractor: `doc/instx.py` (SGI `inst` image → `uncompress` → `gzip -d` (pack) → `col -b`).

## MPT — SGI Message Passing Toolkit

| Fact | Value | Source |
|---|---|---|
| Last MPT for IRIX | **MPT 1.9** | `dist/mpt` descriptor, *IRIX 6.5 Applications August 2006* |
| MPI component | **MPI 4.4 (MPT 1.9)** | `dist/mpi` descriptor |
| SHMEM component | SMA 4.2 (MPT 1.9) | relnotes ch2 |
| MPI standard level | **MPI 1.2** + selected MPI-2 | `mpi.h`: `MPI_VERSION 1`, `MPI_SUBVERSION 2`; `MPI(1)` |
| Array Services | **3.7** on the same CD (base 6.5.30 carries 3.5); MPT needs ≥ 3.4 | `dist/arraysvcs` descriptor; relnotes ch2 §2.3 |
| Disk footprint | ~21.2 MB + 1.5 MB install overhead | relnotes ch2 §2.2 |
| Other prereqs | `eoe.sw.base` (6.5+); `insightbase.sw.eoe` for the manuals | relnotes ch2 §2.3 |

Subsystems to install: `mpt`, `mpi` (`mpi.sw`, `mpi.sw32`/`sw64`, `mpi.hdr`, `mpi.man`,
`mpi.books`), `sma`, plus `arraysvcs`. The `mpi` subsystem requires `sma`.

### Hard constraints that shape the architecture

1. **No heterogeneous clusters.** *"This release of MPT does not support MPI jobs
   across heterogeneous clusters."* (relnotes ch1). In context this is IRIX-vs-Linux;
   an all-IRIX MIPS cluster is the supported case. All ranks must use the same ABI
   and the same MPT version.
2. **`MPI_Comm_spawn` cannot grow a job across hosts on IRIX.** MPI-2 process-manager
   functionality is *"enabled for Linux partitioned systems in this release"* (relnotes
   ch4 §4.4.1); secondary sources describe it as single-host-only on IRIX. Either way the
   prototypes exist in `mpi.h` and the multi-host capability does not. ⇒ **The rank set is
   fixed at `mpirun` time.** Changing the host set means relaunching the MPI job, which is
   the single strongest argument for keeping the GUI in a separate, long-lived process.
3. **MPI is not thread-safe by default.** *"By default MPI is not threadsafe...
   The `MPI_Init_thread` call can be used to request thread safety... `MPI_Init_thread`
   is available on IRIX only."* (`MPI(1)`). The granted level must be read back from
   `provided` at runtime; never assume `MPI_THREAD_MULTIPLE`.
4. **`MPI_STATS` is not thread-safe** — do not enable it in a threaded worker
   (`MPI(1)`, `MPI_Thread(3)`).
5. **`-lmpi` goes last — except that `-lpthread` goes after it.** MPT initialises during
   library load, not at `MPI_Init`; *"To initialize MPI first, include the -lmpi command
   last"* (relnotes ch4 §4.1.3). SGI's own threaded example is `... -lmpi -lpthread`, i.e.
   `-lmpi` last among the ordinary libraries, with the thread library after it. The
   two-binary split makes this moot: the GUI links no MPI, the rank binary links no Motif.
6. **Multi-host needs Array Services + `.rhosts`.** *"MPI requires the presence of an
   Array Services daemon (arrayd) on each host"*, and *"users must set up their .rhosts
   files to enable remote logins. Note that MPI does not use rsh"* (`MPI(1)`).
7. **`AUTHENTICATION SIMPLE`** is required for multi-host jobs; the default changed to
   `NOREMOTE` in Array Services 3.3.1 (relnotes ch6 §6.2).
8. **HIPPI-800 OS bypass was removed in MPT 1.9** (relnotes ch4 §4.3.1, §4.4.7).
   Supported fast interconnects are NUMAlink, GSN and Myrinet/GM. GM 1.4 for IRIX
   exists (relnotes ch6 §6.4) — matches the Myrinet gear already in the collection.
9. **File descriptors**: MPI jobs open many; raise the `jlimit` "files" limit
   (relnotes ch6 §6.5). ~1024 fds ≈ 199 ranks/host.
10. **n32 is not feature-complete in MPT.** The 32-bit MPI library lacks MPI-2 one-sided
   calls, and **Myrinet/GM support is 64-bit only**. If Myrinet is ever to carry MPI
   traffic, the build must be `-64`. Keep the ABI a Makefile variable.
11. **`MPI_STATS` ≠ `MPI_SGI_stat_get()`.** The environment variable prints per-rank
   totals to stderr during `MPI_Finalize` (`MPI(1)`); the live 50-counter API is
   `MPI_SGI_stat_get()` in `mpi_ext.h` (`MPI_SGI_stat(3)`), and it is not thread-safe.
   Only the API can feed a live panel.
12. **`MPI_VERBOSE` is not the job-layout switch.** It appears in `MPI(1)` only inside a
   Linux-only note about `MPI_COREDUMP_DEBUGGER`. Use `mpirun -v` for layout diagnostics,
   and the per-subsystem `MPI_*_VERBOSE` variables for detail.
13. **`mpirun` argument traps** (`mpirun(1)`): `:` separates entries so no argument may
   contain one; the remote working directory defaults to `$HOME` rather than `$PWD` unless
   `-d` is given; a backgrounded job needs stdin redirected; and each job gets a fresh
   login session, so shell startup files can override an exported environment.

### One interconnect per job — the constraint I initially missed

From `MPI(1)`, "Default Interconnect Selection":

> *"MPI uses the first interconnect it can detect and configure correctly. **There will only be
> one interconnect configured for the entire MPI job**, with the exception of XPMEM. If XPMEM is
> found on some hosts, but not on others, one additional interconnect is selected."*

Search order on IRIX: **XPMEM → GSN → MYRINET → TCP/IP**. And forcing one is all-or-nothing:

> *"For a mandatory interconnect to be used, **all of the hosts on the mpirun command line must
> be connected via the device**, and the interconnect must be configured properly. If this is not
> the case, an error message is printed to stdout and the job is terminated."*

So `MPI_USE_GM=1` on a job containing a host without Myrinet does not degrade — it **kills the
job**. And a mixed-fabric cluster cannot have MPT use both fabrics at once:

| job | interconnect MPT will choose |
|---|---|
| lucy + aurora (both Myrinet) | **GM bypass** |
| lucy + arthur (HIPPI only) | TCP/IP — over HIPPI if the names resolve that way |
| lucy + arthur + aurora | **TCP/IP for everything**, because Myrinet is not on arthur |

This is the single most consequential fact for the Tess interconnect design, and it is why the
link seam in `doc/DESIGN.md` §6b matters more than it first appeared: **MPT structurally cannot
use HIPPI and Myrinet in the same job.** Only a transport of our own can.

### mpirun(1) — authoritative syntax

```
mpirun [global_options] entry_object [: entry_object ...]
entry_object := host_list local_options program program_arguments
host_list    := host | host,host,host
```

Verified examples straight from the man page:

```sh
mpirun host_a -np 10 a.out                        # 10 ranks on one host
mpirun host_a, host_b, host_c 10 fred arg1 arg2   # 10 ranks on EACH of three hosts
mpirun -array test host_a 6 a.out : host_b 26 b.out
mpirun -d /tmp/mydir host_a 6 a.out : host_b 26 b.out
mpirun -f my_arguments                            # all args from a file
```

Global options that matter to us: `-a[rray] name`, `-d[ir] path`, `-f[ile] file`,
`-np`, `-v`, `-p prefix`, `-cpr` (checkpoint/restart — single host, single executable
only, so **not** usable as our pause/resume), `-up` (universe size).

**`-f argfile` is the clean answer to "how do I configure which systems to use":**
the control window writes a plain-text mpirun arguments file (whitespace and newlines
are ignored, so it can be formatted for humans) and the launcher runs `mpirun -f`.
That file becomes the saveable "cluster profile", and it can be shown verbatim in the
UI for transparency.

### Environment variables available (MPT 1.9, from `MPI(1)`)

Resource limits (raise these for a tile scheduler with many small messages):
`MPI_BUFS_PER_PROC`, `MPI_BUFS_PER_HOST`, `MPI_MSGS_PER_PROC`, `MPI_MSGS_PER_HOST`,
`MPI_MSGS_MAX`, `MPI_REQUEST_MAX`, `MPI_TYPE_MAX`, `MPI_TYPE_DEPTH`, `MPI_COMM_MAX`,
`MPI_GROUP_MAX`, `MPI_BUFFER_MAX`.

Interconnect selection: `MPI_USE_TCP`, `MPI_USE_GM`, `MPI_USE_GSN`, `MPI_USE_XPMEM`,
`MPI_GM_ON`, `MPI_GM_DEVS`, `MPI_GM_VERBOSE`, `MPI_GSN_ON`, `MPI_GSN_DEVS`,
`MPI_XPMEM_ON`, `MPI_XPMEM_THRESHOLD`, `MPI_BYPASS_OFF`.

NUMA placement (Origin 200 / Onyx2): `MPI_DSM_CPULIST`, `MPI_DSM_CPULIST_TYPE`,
`MPI_DSM_DISTRIBUTE`, `MPI_DSM_MUSTRUN`, `MPI_DSM_PLACEMENT`, `MPI_DSM_PPM`,
`MPI_DSM_TOPOLOGY`, `MPI_DSM_VERBOSE`, `MPI_DSM_VERIFY`, `MPI_DSM_OFF`.

Diagnostics / behaviour: `MPI_STATS` (see `MPI_SGI_stat(3)`, and note fact 11 above),
`MPI_CHECK_ARGS`, `MPI_NAP` (how ranks wait for events — relevant when a rank shares a
CPU with the GUI), `MPI_UNBUFFERED_STDIO` (needed to stream rank output into the log
pane without lag), `MPI_COREDUMP*`, `MPI_SIGTRAP*`, `MPI_SLAVE_DEBUG_ATTACH`,
`MPI_ARRAY`, `MPI_DIR`, `MPI_UNIVERSE`, `MPI_UNIVERSE_SIZE`.

Extracted reference copies live in `doc/mpt/man/` (40 man pages incl. `MPI.txt`,
`mpirun.txt`, `MPI_Thread.txt`, `MPI_SGI_stat.txt`), `doc/mpt/relnotes/` (all 9
chapters) and `doc/mpt/include/mpi.h` (the real MPI 4.4 header).

## Motif / IRIS IM — this was the big surprise

| Subsystem | What it is | Source |
|---|---|---|
| `motif_dev` | "IRIX IM Development Software, 6.5 (based on **OSF/Motif 1.2.4**)" | descriptor, Overlays 2/3 |
| `motif21_dev` | "IRIX IM 2.1 Development Software, 6.5.22 (based on **OSF/Motif 2.1.20**)" | descriptor, Overlays 3/3 |
| `motif_eoe` | "IRIX IM Execution Software (**Motif 1.2 and 2.1 Combined**) for 6.5.22" | descriptor, Overlays 2/3 |

**The Motif 2.1 runtime is part of the base OS on 6.5.22+**, not an add-on:
`motif_eoe` installs `/usr/lib32/libXm.so.1` *and* `/usr/lib32/libXm.so.2`, plus
`libSgm.so.1`, `libSgm.so.2`, `libMrm.so.2`, `libUil.so.2`, `libXpm.so.2`
(and the same set under `lib64`). Only the 2.1 *headers* need the `motif21_dev`
overlay, which is on the media.

Header/lib layout:

```
Motif 1.2 headers : /usr/include/Xm/...
Motif 1.2 libs    : /usr/Motif-1.2/lib32/libXm.so   -> /usr/lib32/libXm.so.1
Motif 2.1 headers : /usr/Motif-2.1/include/{Xm,Sgm,Mrm,uil,X11/Xirisw}
Motif 2.1 libs    : /usr/Motif-2.1/lib32/libXm.so   -> /usr/lib32/libXm.so.2
```

⇒ **Target Motif 2.1.** The runtime is guaranteed on any 6.5.22+ box, and it unlocks
`XmComboBox`, `XmSpinBox`, `XmNotebook`, `XmScrollFrame` and friends, which Motif 1.2
lacks. Build host needs `motif21_dev` installed (verify: `ls /usr/Motif-2.1/include/Xm/Xm.h`).

### libSgm — SGI's own widget set (ships for both 1.2 and 2.1)

`/usr/Motif-2.1/include/Sgm/` contains, among others:

- **`ThumbWheel.h`** — the SGI thumbwheel. The period-correct control for continuous
  values: zoom, iteration count, palette rotation.
- **`Dial.h`**, **`Osc.h`** — a dial and an oscilloscope-style trace. `Osc` is a ready-made
  live throughput history per node.
- **`ColorC.h`** (colour chooser), **`ColorHexagon.h`**, **`ColorChooserSwatch.h`**,
  **`OglColorHexagon.h`**, `GLColorSlider*` — palette editing, for free.
- **`VisualDrawingA.h`** — a drawing area that lets you *request a visual*. This is the
  clean fix for a machine whose default visual is 8-bit PseudoColor: ask for 24-bit
  TrueColor for the render canvas instead of fighting the default.
- **`GlxDraw.h`, `GlxMDraw.h`** — OpenGL drawing-area widgets, if GL blitting ever beats
  `XPutImage`.
- `Finder.h`, `Graph.h`, `Grid.h`, `SgList.h`, `Column.h`, `SpringBox.h`, `RubberBoard.h`,
  `HPanedW.h`, `ZoomBar.h`, `IconG.h`, `DropPocket.h`, `DynaMenu.h`, `ZbText.h`, `Arc.h`.

Link with `-lSgm`. These are SGI-only — the macOS iteration build must stub them, which
argues for confining `Sgm` usage behind a thin wrapper.

## Compile / link lines

```sh
# IRIX 6.5.30, MIPSpro 7.4.4, Motif 2.1, MPT 1.9 — note -lmpi LAST
cc -n32 -mips4 -O3 -c89 \
   -I/usr/Motif-2.1/include \
   -o gizmo gizmo.c \
   -L/usr/Motif-2.1/lib32 \
   -lXm -lSgm -lXt -lX11 -lm -lpthread -lmpi
```

`-mips4` covers R12000 (arthur), R14000 (lucy) and R16000 (aurora), so one `-64 -mips4`
binary runs on all three. Confirm each box's CPU with `hinv -c processor -t`.

## Still to verify on the hardware

```sh
versions -b | egrep -i 'mpt|mpi|arraysvcs|motif'   # what is actually installed
ls /usr/Motif-2.1/include/Xm/Xm.h                  # are the 2.1 headers there
ainfo arrays ; ainfo dfltarray ; ascheck            # Array Services config sanity
ps -ef | grep arrayd                                # is the daemon running
mpirun -np 2 hostname                              # single-host MPI works
xdpyinfo | grep -A3 'depth of root'                # visual depth on each screen
hinv -c processor ; hinv -m                        # CPU model/count, memory
```

## Array Services configuration — what actually has to be true

`arrayd.conf` entry types: `ARRAY`, `COMMAND`, `LOCAL`, `AUTHENTICATION`
(plus `ROOTEXECUTION`). The minimum for a working cluster is one `ARRAY` block with
`MACHINE` lines and `DESTINATION ARRAY <name>` under `LOCAL`.

- Authentication has exactly three settings: `NOREMOTE` (the installed default — blocks
  every remote request), `NONE`, or `SIMPLE` with 64-bit per-host keys. Multi-host MPI
  needs `SIMPLE`.
- `sgi-arrayd 5434/tcp` must be in `/etc/services` on every node, identically.
- Array Services also wants a shared `arraysvcs` account present on all nodes.
- `arrayconfig(1M)` generates and distributes `arrayd.conf` in one shot.
- Validate with `arrayd -c -f <file>` (syntax), `ascheck` (cross-node semantics),
  `arrayd -n -v` (foreground diagnostics), and probe with `ainfo` / `array` / `arshell`.
- IRIX 6.5.30 also offers Secure Array Services (`sarraysvcs`, daemon `sarrayd`, SSL/SSH).
  It is mutually exclusive with the standard flavour and has no `arshell`. Use standard.
- Must match across nodes: user name/uid, binary path, ABI, MPT version, arrayd port,
  array name and machine list, and the auth keys.

## IRIX statistics APIs — the recipes that actually work on 6.5

| Want | Call |
|---|---|
| CPU count | `sysconf(_SC_NPROC_CONF / _SC_NPROC_ONLN)` or `sysmp(MP_NPROCS / MP_NAPROCS)` |
| CPU model, MHz, cache | `getinvent(3)` — in libc, no `-linvent`. For `INV_PROCESSOR`/`INV_CPUBOARD`, `inv_controller` is the clock in MHz |
| Memory total/free | `sysget(SGT_RMINFO)` × `getpagesize()` — prefer this over `sysmp(MP_SAGET, MPSA_RMINFO)`; never `MP_KERNADDR`. Pass a real cookie as the fifth argument, see below |
| Swap | `swapctl(SC_GETFREESWAP)` |
| Load average | `sysget(SGT_KSYM, …)` ÷ 1024.0 — unprivileged, this is what `uptime` does. The symbol name goes in the *cookie*, not the buffer, see below |
| Per-CPU %busy | two samples of `struct sysinfo` `cpu[]` ticks; per-CPU form `sysmp(MP_SAGET1, MPSA_SINFO, …, cpuid)` |
| Process RSS/VSZ | `ioctl(PIOCPSINFO)` on `/proc/pinfo/<pid>` — `getrusage` `ru_maxrss` is useless here; `syssgi(SGI_PROCSZ)` was removed in 6.5 |
| Link speed, MTU, if counters | `ioctl(SIOCGIFDATA)` → `struct if_data` (`ifi_baudrate`, `ifi_mtu`, `ifi_type`) — what `ifconfig` uses |
| Uptime | utmp `BOOT_TIME` record |
| Local high-res timing | `clock_gettime(CLOCK_SGI_CYCLE)` |
| Cross-rank timing | `MPI_Wtime()`, timed at one rank so `MPI_WTIME_IS_GLOBAL` never matters |

`sysget(2)` is the supported, unprivileged, node-aware replacement for the `sysmp`
`MP_SAGET` family and should be the default choice on 6.5.

**Correction (19 September 2026, from `/usr/include/sys/sysget.h` on lucy).** Every
`sysget` call in this project returned −1 with `errno` 14, EFAULT, for memory and for the
load average alike. The header says why. The prototype is

```c
extern int sysget(int, char *, int, int, sgt_cookie_t *);
```

The fifth argument is a **structure the kernel dereferences**, not a spare pointer and not
a string. Passing `(void *)0` for memory, or `(void *)"avenrun"` for the load average, is
an unreadable address in both cases, which is exactly what EFAULT reports. The header
supplies the macros to fill it in:

```c
sgt_cookie_t ck;

SGT_COOKIE_INIT(&ck);                    /* sc_status=SC_BEGIN, all cells */
sysget(SGT_RMINFO, (char *)&rmi, sizeof rmi, SGT_READ | SGT_SUM, &ck);

SGT_COOKIE_INIT(&ck);
SGT_COOKIE_SET_KSYM(&ck, KSYM_AVENRUN);  /* name goes in the cookie */
sysget(SGT_KSYM, (char *)avenrun, sizeof avenrun, SGT_READ, &ck);
```

`SGT_COOKIE_INIT` sets the cookie to mean every cell; `SGT_SUM` asks the kernel to add
those cells together, which is what a whole-machine total means on a NUMA box like aurora.
`KSYM_AVENRUN` is the header's own spelling of `"avenrun"`.

With this, `sysget` answers for memory, for the load average and for `SGT_SINFO` CPU ticks,
all three unprivileged. `src/common/tess_inventory.c` uses it and keeps
`sysmp(MP_SAGET, …)` only as a fallback; `-v` names whichever answered. That the
unprivileged path works again matters here: the probe happens to run as root today, and
`MP_SAGET` would have read zero the moment anything ran as an ordinary user.

The earlier note in this file blamed the EFAULT on a node or CPU selector and recorded
`MP_SAGET` as "what answers today". That was a guess dressed as a finding, and the header
that settled it was two commands away the whole time. Read the header.

## arshell needs root here, so tess-ui does too (19 September 2026, measured)

`tess-ui` discovers the cluster by running `tess-probe` over `arshell`. Run as an
ordinary user the GUI got nothing back and every host showed "no answer"; run as root the
same code discovered both machines immediately.

So on this cluster the GUI is a root process, which follows from the Array Services
configuration rather than from anything Tess does. Worth knowing before build 4 is used on
arthur, and worth revisiting if the array is ever reconfigured with per-user
authentication, because a root GUI is not something to keep by choice.

## MPI_NAP does not stop a parked rank spinning (19 September 2026, measured)

`MPI(1)` presents `MPI_NAP` as the answer to an idle rank pegging a CPU: undefined
means spin, defined with no value means yield, a positive integer means sleep that many
milliseconds between checks. DESIGN.md §7 relies on it for exactly that.

It does not cover a rank blocked in `MPI_Probe` under MPT 1.9. Measured on aurora with
four workers parked between frames and the GUI idle:

```
arshell aurora printenv MPI_NAP        ->  2
TIME column, four workers, 30 s apart  ->  0:15 ... 0:45   (each)
```

Thirty seconds of CPU in thirty seconds of wall time, per worker, with the setting
confirmed present in the ranks' own environment. So a "parked" rank costs a whole CPU.

The fix is in the application, not the environment: poll with `MPI_Iprobe`, spin briefly
so tile hand-out stays immediate during a frame, then sleep a couple of milliseconds
while genuinely idle. `src/node/tess_node.c:wait_for_master()`. Idle CPU on aurora then
falls to zero between frames.

Worth remembering when any rank has to wait on a human rather than on work.

## Motif conventions worth honouring

- `appName*sgiMode: TRUE` — one resource line switches on the sculpted IRIX
  Interactive Desktop look.
- Use SGI *scheme* symbolic colour/font names, never literals. Schemes deliberately
  exclude content/rendering areas — correct for a fractal canvas.
- Canonical menu order is File, Selected, Edit, View, Tools, Options, Help — keep the
  order even when the menus are renamed.
- The two-window shape has a name in SGI's guidelines: a *main primary window* plus a
  *support window*, with different prescribed behaviour for each.
- A status line at the bottom of the render window is the documented status area;
  `XmMainWindow` has a slot for it.
- Default keyboard focus policy is *implicit* (follows pointer). Install accelerators
  per window; never warp the pointer.
- Feedback thresholds are numeric: busy pointer at 3 s, progress indicator at 5 s.
- `XmPanedWindow` is vertical-only in Motif 1.2 (`XmNorientation` arrived in 2.0) — a
  non-issue once targeting 2.1, but it explains the baseline's layout.

## Myrinet / GM — why 64-bit is forced (verified from the media)

`6.5.13_myrinet_1.0.2.tar` describes itself as **"IRIX Myrinet Driver 1.0.2"** and its
`.idb` shows it installs **only 64-bit libraries**:

```
usr/myricom/lib64/libgm.so
usr/myricom/lib64/libgm.a
usr/myricom/{bin,sbin,include,doc,info,var}
etc/init.d/myrinet
etc/config/myrinet_mapper{off,.options}
```

There is no `lib32`, which corroborates "Myrinet/GM support is 64-bit only" in MPT. A build that
wants GM bypass must be `-64`, and the self-built GM 1.6 needs a 64-bit `libgm` for MPT's
64-bit `libmpi` to load.

**Superseded in practice.** SGI's package is restricted to `mach(CPUBOARD=IP27 IP35)` in all 49
of its install conditionals — Onyx2 and Origin 350 but *not* IP30, the Octane2 — so it never
covered lucy. Rutger has since built **GM 1.6, working on both the Octane2 and the Origin 350**,
which is what the cluster actually runs. Keep the 1.0.2 facts above only as the explanation for
why MPT's expectations may not match.

**The live question is whether MPT 1.9 binds GM 1.6.** MPT's release notes (ch6 §6.4) point at
*"A version of GM 1.4 for IRIX ... 6.5.13myrinet1.0.1"*, so its bypass was built against a
GM 1.4-based library and the GM API moved between 1.4 and 1.6. Test it directly:

```sh
ls -l /usr/myricom/lib64/libgm.so ; /usr/myricom/bin/gm_board_info
versions -av | grep -i myrinet
setenv MPI_USE_GM 1 ; setenv MPI_GM_VERBOSE 1 ; mpirun lucy 1, aurora 1 ./hello
```

If MPT will not bind GM 1.6, SGI's 1.0.2 is not a fallback — it cannot install on the Octane2.
The real options are a raw GM transport of our own behind the link seam (`doc/DESIGN.md` §6b),
IP over GM if the 1.6 build provides that driver, or ethernet for that pair. Note also that
MPT's GM support exists only in the **64-bit** MPI library, so a 64-bit `libgm` is a
prerequisite either way.

## Which interface does MPI actually use?

There is no `MPI_TCP_INTERFACE`-style variable. For the TCP path, the interface is chosen
by **name resolution / the `HOSTNAME` field in `arrayd.conf`**. So to make TCP traffic ride
a HIPPI or Myrinet IP interface rather than ethernet, point each machine's `arrayd.conf`
`HOSTNAME` at its address *on that fabric*:

```
array sgicluster
  machine arthur
    hostname "arthur-hippi"     # the HIPPI address, not the ethernet one
```

GM bypass does not use IP at all, so it is unaffected by this — but the fallback path is,
and so is every Array Services round trip.

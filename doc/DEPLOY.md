# Deploying Tess

How to get Tess onto a set of SGI machines and render something. Written against the
cluster it was built on: `lucy`, an Octane2 (IP30), and `aurora`, an Origin 350 (IP35).
Nothing here is specific to those two beyond the hostnames.

Everything below was run on the machines rather than reasoned about. Where a step has a
failure mode we actually hit, it says so.

## 1. What you need

**On every machine that will compute or display:**

| | |
|---|---|
| IRIX | 6.5.30 (6.5.x should be fine; the statistics calls are 6.5 APIs) |
| Compiler | MIPSpro 7.4.4m. `cc`, not gcc: the build is `-64 -mips4` |
| MPI | SGI MPT 1.9 (MPI 4.4). Needs `libmpi` and `mpirun` on `PATH` |
| Make | GNU make, as `gmake`. The Makefile uses `:=`, `ifeq` and `VPATH` |
| Array Services | `arrayd` running, with every host in the same array |

**On the display host only, additionally:**

| | |
|---|---|
| Motif | 2.1 under `/usr/Motif-2.1` (headers in `include`, libraries in `lib64`) |
| X11 | A running server, and `DISPLAY` set |

**Shared between them:**

An NFS share, mounted at the same path on every SGI. This tree lives at
`/cluster/dev/sgi-tess`, so `/cluster` is the mount point throughout this guide;
substitute your own. One checkout, every machine building natively in it. That is not a
convenience, it is the design: lucy and aurora have no ssh, and a cross compiler is not
MIPSpro.

[doc/NFS-SERVER.md](NFS-SERVER.md) covers setting the share up, including the UID agreement
the SGIs need.

Check the pieces are there before going further:

```
which cc gmake mpirun arshell
ls /usr/Motif-2.1/lib64/libXm.so
ar t /usr/lib64/libmpi.so > /dev/null && echo mpt ok
```

## 2. Getting the source

**There is no git on IRIX.** Use any machine that has git and can reach the share: the
NFS server itself, a workstation with it mounted, whatever is convenient. Clone straight
into the shared directory and the SGIs see the result immediately.

From that machine, with the share mounted at `<share>`:

```
git clone git@github.com:42718/sgi-tess.git <share>/dev/sgi-tess
```

so that the SGIs, which mount the same share at `/cluster`, find it at
`/cluster/dev/sgi-tess`. From then on a commit on that machine is already live on every
SGI. There is nothing to pull, push or copy, and no deployment step.

If you would rather not share a working tree at all, `tar` it across to each machine and
skip to the next section. The build does not care, you just lose the single checkout.

## 3. Building

Run this **on each machine**, in the shared tree:

```
cd /cluster/dev/sgi-tess
gmake fresh
```

Output goes to `build/$(uname -m)`, so `build/IP30` and `build/IP35` coexist in one tree
and neither overwrites the other's objects. `gmake fresh` cleans **only this machine's
architecture** and rebuilds it, which is why building on lucy leaves aurora's binaries
untouched and stale until aurora builds too. That caught us: an `IP35` binary run on lucy
works, because both are `-64 -mips4`, and silently reports whatever the last aurora build
produced.

| Target | Builds |
|---|---|
| `gmake` | everything this machine can build |
| `gmake probe` | `tess-probe`, the inventory reporter. Needs nothing but libc |
| `gmake node` | `tess-node`, the MPI master and workers. No Motif |
| `gmake ui` | `tess-ui`, the Motif interface. No MPI |
| `gmake tiler` | `tess-tiler`, the milestone 0 backend from `baseline/` |
| `gmake info` | what this machine looks like to the build |
| `gmake fresh` | clean this architecture, then rebuild it |
| `gmake clean` / `distclean` | this architecture / every architecture |

**Always use `gmake fresh` after editing from the other side of the NFS mount.** The SGI
clocks run ahead of the Mac's, so a file written over NFS arrives looking *older* than the
binary built from the previous version. `make` then reports success having done nothing,
and you debug code that was never compiled. `fresh` exists because this cost us an evening.

## 4. Verifying before you start the UI

Two checks, in this order. They take a minute and they localise every failure that follows.

**Each machine can describe itself:**

```
./build/IP30/tess-probe -v
```

```
host          lucy
cpus          2 configured, 2 online
clock         600 MHz
memory        2560 MB total, 1912 MB free
  via        sysget(SGT_RMINFO) ok
load          0.20
cpu busy      4.8 %
```

The `via` line is the one to read. `sysget(SGT_RMINFO) ok` is the documented, unprivileged
path. If it says `sysmp(MP_SAGET,MPSA_RMINFO) ok` instead, the `sysget` call failed and
fell back, and the numbers will read zero the moment the probe runs as an ordinary user.
`load` should match `uptime`. `cpu busy` should be small on an idle machine and rise under
load; pin a CPU with `dd if=/dev/zero of=/dev/null bs=1024k &` if you want to see it move.

**Array Services can reach the other machines:**

```
arshell aurora /cluster/dev/sgi-tess/build/IP35/tess-probe
```

This is exactly what the UI runs to discover the cluster, including the architecture in the
path. **`arshell` needs root here**, which is why the UI does too. Run as an ordinary user
it returns nothing and every host shows "no answer".

## 5. Running the UI

On the display host, as root:

```
cd /cluster/dev/sgi-tess
./build/IP30/tess-ui
```

The defaults match a standard layout, so that bare form is the same as spelling out
`-hosts lucy,aurora -tree /cluster/dev/sgi-tess -port 7333`.

| Switch | Default | What it does |
|---|---|---|
| `-hosts a,b,c` | `lucy,aurora` | the cluster. **The first host is the display host** |
| `-tree <path>` | `/cluster/dev/sgi-tess` | where the binaries live, on every machine |
| `-port <n>` | `7333` | loopback port the master listens on |
| `-savedir <path>` | `.` | where saved images go |
| `-w <px>` / `-h <px>` | fitted to the screen | fixed frame size, for reproducible output |
| `-attach` | off | attach to a job already running instead of launching one |
| `-host <name>` | `localhost` | attach to a master somewhere else |

Unrecognised switches are currently ignored without a word, so check your spelling.

Then: **Rescan** to discover the hosts, adjust rank counts and tick boxes, **Start** to
launch the job, **Render** to draw a frame.

The host order matters. The first host runs the GUI, the master rank and the shading pass,
so its first rank computes nothing: two ranks on lucy means one computing CPU there.
Compute hosts have nothing else to do and offer every CPU they have. Unticking the display
host does not remove it, it drops it to master only, because the master is what the GUI
connects to.

## 6. What Start actually runs

Useful when reproducing a failure by hand. The UI forks this directly, it does not go
through a shell:

```
mpirun -d /cluster/dev/sgi-tess \
    lucy 2 /cluster/dev/sgi-tess/build/IP30/tess-node -listen 7333 -nonce <16 hex> : \
    aurora 4 /cluster/dev/sgi-tess/build/IP35/tess-node -listen 7333 -nonce <16 hex>
```

One colon-separated group per host, each naming that host's own architecture binary, which
is how one job spans IP30 and IP35. The nonce is generated per launch and the master
rejects a client that greets it with the wrong one.

## 7. When it does not work

| Symptom | Cause |
|---|---|
| "master closed the connection" | A master from a previous job still holds port 7333, so the new job cannot bind and the GUI attaches to the old one, which rejects the new nonce. Start now sweeps first; if it persists, `killall tess-node` on every host |
| Every host shows "no answer" | `arshell` is not running as root, or `arrayd` is not configured with these hosts |
| A host shows no CPU percentage | Its `tess-probe` is stale and predates the `busy=` field. `gmake fresh` on that machine |
| Edits appear to do nothing | NFS clock skew. `gmake fresh` |
| `tess-probe` reports 0 MB memory | Running as an ordinary user against the `MP_SAGET` fallback. See the `via` line, and `doc/PLATFORM-FACTS.md` |
| Ranks survive Stop | `SIGTERM` does not reliably end them; `SIGINT` does. Stop escalates and then sweeps with `arshell killall` |

`doc/PLATFORM-FACTS.md` records every platform claim with the evidence behind it, and is
the right place to look when something behaves differently on your hardware.

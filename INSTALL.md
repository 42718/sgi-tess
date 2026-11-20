# Installing Tess from source

The short path from nothing to a rendered frame. [doc/DEPLOY.md](doc/DEPLOY.md) has the
reasons behind each step and what to do when one fails.

Tess 1.0-beta is built from source on each machine. A `tardist` for `inst` is planned for
1.0. Every command below runs **as root**, in IRIX's root shell, `csh`. They paste
cleanly into it.

## 1. What each machine needs

**Every machine:** a 64-bit MIPS CPU, R10000 or later, running IRIX 6.5. Tested on IP27
(Onyx2, R12000), IP30 (Octane2, R14000) and IP35 (Origin 350, R16000), all on 6.5.30.

| Product | What for | Tested with |
|---|---|---|
| `c_dev`, `compiler_dev`, `compiler_eoe` | MIPSpro `cc` | 7.4.4m |
| `mpt`, `mpi` | MPI, `mpirun` | MPT 1.9, MPI 4.4 |
| `arraysvcs` | `arrayd` and `arshell`, which `mpirun` launches through | Array Services 3.7 |
| GNU make | the Makefile, as `gmake` | Nekoware |

**The machine you run the interface on, also:**

| Product | What for | Tested with |
|---|---|---|
| `motif21_dev`, `motif_eoe` | Motif 2.1, under `/usr/Motif-2.1` | IRIX IM 2.1 for 6.5.30 |
| `x_eoe` | X11 | 6.5.30 |

The display needs a 24-bit TrueColor visual. It does not have to be the desktop's: arthur's
InfiniteReality runs its desktop at 8 bits and still offers one, and the interface finds it.

To see what a machine has:

```
versions -b | egrep -i 'arraysvcs|mpt|motif|c_dev|compiler'
which cc gmake mpirun arshell
```

## 2. A shared filesystem and agreeing clocks

All machines mount one NFS share at **the same path**. The source tree lives there, every
machine builds into its own directory inside it, and `mpirun` starts each machine's binary
from the same path. Here the share is `/cluster` and the tree `/cluster/dev/sgi-tess`;
substitute your own. [doc/NFS-SERVER.md](doc/NFS-SERVER.md) covers setting one up.

The clocks must agree with the file server's, or `make` decides edited files are older than
the binaries and skips them without a word. It is the file server that matters, because it
stamps every file in the tree. NTP does the job if the file server follows the same time
source as the SGIs. Without NTP, IRIX's `timeslave` does it against the file server itself,
with nothing installed on the SGIs. On each machine:

```
echo '-H <file-server> -P /var/adm/timetrim' > /etc/config/timeslave.options
chkconfig -f timeslave on
/usr/etc/timeslave -H <file-server> -P /var/adm/timetrim
```

The file server needs the RFC 868 time service. On a Debian server that is one line for
inetd; `doc/PLATFORM-FACTS.md`, "Clock synchronisation", has it and the reasons.

## 3. Array Services

`mpirun` starts ranks on other machines through `arrayd`, so every machine must be in the
same array, with the same `/usr/lib/array/arrayd.conf`, and must resolve every other
machine's name to its address in `/etc/hosts`. Section 5 has `tess-setup`, which checks
all of this and fixes most of it; [doc/CHECKLIST.md](doc/CHECKLIST.md), step 4, has the
configuration by hand. A first array, on a machine with none, is one command:

```
arrayconfig -m -a <name> lucy arthur aurora
```

It is working when this answers from every machine:

```
array uptime
```

Then check that a shell started by Array Services prints nothing, on every machine. Any
output from `/.cshrc` or `/.login`, such as an unguarded `stty`, breaks every launch:

```
csh -c true
csh -l -c true
```

## 4. Get the source

There is no git on IRIX. Clone on any machine that has git and mounts the share, straight
into the shared directory:

```
git clone git@github.com:42718/sgi-tess.git <share>/dev/sgi-tess
```

`main` is the latest. Releases are tagged, starting with `v1.0-beta`.

## 5. Configure, then check each machine

Copy the example settings to the top of the tree and set the hosts, the array and the
path of the tree:

```
cd /cluster/dev/sgi-tess ; cp sgi-tess.config.example sgi-tess.config
```

Every machine reads this one file over the share. Or skip the copy, and the first
`tess-setup -apply` writes one from what Array Services already knows.

Then run `tess-setup` on each machine, as root. With no switches it only checks: it
says what is right, what it would change, and what needs doing by hand, and changes
nothing. Start there:

```
sh /cluster/dev/sgi-tess/scripts/tess-setup
```

When its list looks right, let it make the changes. It asks before each one, and keeps
a dated copy of any system file it edits, such as `/etc/hosts.tess-20261120-165015`:

```
sh /cluster/dev/sgi-tess/scripts/tess-setup -apply
```

It checks that every host is in `/etc/hosts` with its plain name first, that
`arrayd.conf` lists every host, that Array Services is on and the other machines answer
through it without printing anything, that shells print nothing, that `timeslave`
or NTP keeps the clock with the share's, and that this machine has a current build. It never
edits `arrayd.auth`; when authentication is the problem it says what to change.

## 6. Build, once per machine type

Each machine builds only its own architecture, into `build/<arch>`, so the IP27, IP30 and
IP35 builds sit side by side in one tree. Build on one machine of each type. From one
machine, through Array Services:

```
cd /cluster/dev/sgi-tess ; gmake fresh
arshell aurora "cd /cluster/dev/sgi-tess ; gmake fresh"
arshell arthur "cd /cluster/dev/sgi-tess ; gmake fresh"
```

`gmake fresh` cleans this machine's build and rebuilds it. Use it rather than plain
`gmake` after editing from another machine. It builds:

| Program | What it is |
|---|---|
| `tess-ui` | the Motif interface. Only where Motif is installed |
| `tess-node` | the MPI master and workers |
| `tess-probe` | reports a machine's CPUs, memory and load to the interface |
| `tess-preflight` | checks a machine and says what to fix |
| `tess-tiler` | the 2001 renderer, kept as a known-good MPI test |

Check the build is the release you meant:

```
./build/`uname -m`/tess-node -version
```

```
tess-node 1.0-beta (IP27, built Nov 20 2026)
```

## 7. Check, then run

On every machine, `tess-preflight` runs the checks the steps above set up, and prints the
fix for anything that fails. On the machines that only compute, skip the display checks:

```
./build/`uname -m`/tess-preflight
arshell aurora /cluster/dev/sgi-tess/build/IP35/tess-preflight -compute -q
```

Then, on the machine with the display, as root, start the interface with the machines to
use:

```
./build/`uname -m`/tess-ui -hosts lucy,arthur,aurora
```

It runs the same checks in a window first, and closes it after a minute if all pass. In
the **Cluster** pane, **Rescan** finds the machines, **Start** launches the job, and
**Render** draws a frame. The machine the interface runs on is always first in the list:
it holds the master, which schedules and shades rather than computing.

`tess-ui -help` lists the switches. [doc/DEPLOY.md](doc/DEPLOY.md), section 7, lists the
failures we have hit and their causes.

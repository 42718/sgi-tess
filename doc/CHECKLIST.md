# Bring-up checklist — the linear version

One page to keep open at the machines. Details for every step are in `BRINGUP.md`;
this is just the order, the one command that matters, and what "good" looks like.

**Ordering principle: make MPI work over boring ethernet first.** Every fabric is an
optimisation on top of that, and if you add fabrics first you cannot tell which layer
broke. Steps 1–5 are the critical path; 6 onward are improvements, in payoff order.

Aurora on **one brick** throughout — 4 CPUs is plenty to prove all of this, and the
router plus L2 controller cost real power.

> **Pasting these blocks into IRIX:** the root shell is `csh`, which does **not** honour `#`
> comments interactively — it tries to glob what follows, and an unmatched `?` or `*` aborts the
> whole command with `No match.` before it runs. Type `sh` first and paste into that, or paste
> commands one at a time. Comments here have been stripped of glob characters, but `#` itself
> still isn't a comment to csh.

---

## Critical path — this is what unblocks Tess

### 1 · Inventory  ·  all three hosts
```sh
uname -aR ; hinv -c processor -t ; hinv -c memory
versions -b | egrep -i 'mpt|mpi|arraysvc|motif|pcp'
```
☐ All three on 6.5.30, same MIPSpro. Write the output down.

### 2 · Install MPT + Array Services  ·  all three
From *IRIX 6.5 Applications August 2006*: `inst -f /CDROM/dist`, then
`install mpt mpi sma arraysvcs`.
```sh
versions -av | egrep -i 'mpi|mpt|arraysvc'    # MPI 4.4 (MPT 1.9), Array Services 3.7
ls /usr/lib64/libmpi.so /usr/include/mpi.h /usr/include/mpio.h
cc -64 -mips4 -O2 -o mpi-hello mpi-hello.c -lmpi   # tools/mpi-hello.c
mpirun -d $PWD -np 2 ./mpi-hello              # prints rank 0 and rank 1, both local
```
☐ Single-host `mpirun` works. **Stop here if not** — nothing later can work.

**Do not test with `mpirun -np 2 hostname`.** It is the documented smoke test and it does not
work here: `hostname` exits before MPT's startup handshake, and MPT reports that as
`MPI: could not run executable`. Use a real MPI binary. See `doc/MPI-HELLO.md`.

### 3 · Hosts file, ethernet only  ·  all three
Identical `/etc/hosts` everywhere, ethernet addresses only for now:
`172.28.4.8 lucy`, `172.28.4.17 arthur`, `172.28.4.16 aurora`. Plus `~/.rhosts`
listing all three, mode 600.
```sh
grep sgi-arrayd /etc/services                 # sgi-arrayd 5434/tcp, identical everywhere
```
☐ Every host can `ping` the other two by name, forward and reverse.

### 4 · Array Services  ·  all three
`arrayconfig -d -i -m -a tess lucy arthur aurora`, or hand-write `arrayd.conf` with one
`ARRAY` block. **In `arrayd.auth`, comment out `AUTHENTICATION NOREMOTE`** — only the last
authentication entry read takes effect.
```sh
arrayd -c -f /usr/lib/array/arrayd.conf ; /etc/init.d/array restart ; ascheck
ainfo arrays ; array uptime ; arshell arthur hostname ; arshell aurora hostname
```
☐ `ascheck` clean, `array uptime` answers from all three, `arshell` works.
If `arshell` is missing you have Secure Array Services — different daemon, no `arshell`.
Under `AUTHENTICATION SIMPLE`, try `ascheck -F` before believing a reported failure.
`IDENT unknown` in `ainfo machines` is the `asmachid` suggestion, not an error, and does
**not** block multi-host MPI.
Before step 5, confirm `csh -c true` and `csh -l -c true` are **silent** on every host —
any output from a login file breaks rank launch. See `doc/MPI-HELLO.md`.

### 5 · Three-host mpirun over ethernet  ·  **the milestone**
```sh
setenv MPI_USE_TCP 1                                             # every host — MPI-HELLO.md trap 6
cd /work/tess
cc -64 -mips4 -O2 -o mandelmpi mandelmpi-ppm-tiler.c -lmpi -lm   # fix line 171 first
mpirun -v -a tess -d /work/tess lucy 2, arthur 4, aurora 4 ./mandelmpi \
       -w 1920 -h 1200 -max 2000 -o /work/tess/t.ppm
```
`-d` is not optional — the remote working directory defaults to `$HOME`.
☐ `imgview t.ppm` shows a Mandelbrot.
☐ `osview` on aurora shows **all four CPUs busy**, not one.
☐ **Write the working command line down.** It becomes the first `~/.tess/cluster.args`.

At this point Tess development can start. Everything below is optimisation.

---

## Improvements, in payoff order

### 6 · pingpong, ethernet baseline
```sh
cc -64 -mips4 -O2 -o pingpong pingpong.c -lmpi -lm
mpirun -v -a tess -d /work/tess lucy 1, aurora 1 ./pingpong
mpirun -v -np 2 ./pingpong                       # on-host shmem, for comparison
```
☐ Record the **12288-byte row** — one 64×64 tile. Predicted need: only ~2.9 MB/s
sustained from a 20-CPU aurora, so ethernet at 11 MB/s should already have 4× headroom.

### 7 · Gigabit lucy↔aurora
```sh
hinv -c network ; ifconfig -a                    # is the card seen, and by which driver
```
Driver support for that specific card is the real question. Then `10.42.2.8` / `10.42.2.16`,
direct cable (crossover if the PHYs don't negotiate), then chase `mtu 9000`.
☐ `netstat -i` counters move on the gigabit interface, not `ef0`.
☐ pingpong again — expect 25–60 MB/s; the PCI bus limits it as much as the wire.

### 8 · HIPPI lucy↔arthur  ·  Essential driver, interface `ess0`
Not SGI's HIPPI package — that drives `hipXX`. Yours is Essential's `essN`.
☐ Install **release 2.0.3** from `HIPPI-archive-full/essential/HIPPI_Octane_6.5_IP27_IP30.tar`
(fixes an Octane boot panic; covers IP27 and IP30, so both ends).
☐ `/etc/config/netif.options`: `if2name=ess0`, `if2addr=lucy-hip`
☐ `/etc/config/ifconfig-2.options`: `netmask 0xffffff00 up`
☐ `/etc/init.d/Essnic.pre`: **uncomment `essarp -X ess0`** — no switch means no HARP server
☐ `/usr/Essential/hippi/scripts/esssetup.sh ess0`, then reboot
☐ `essarp -h` shows UP RUNNING with a ULA; `ping -s 60000 arthur-hip` works
☐ `netstat -i` counters move on `ess0`. (`arp -a` will *not* show it — the driver keeps its
own tables. Use `essarp -h`.)
☐ `blast`/`sink` for a fabric-level number before MPI is involved.
Remember MPT 1.9 has **no** HIPPI bypass; this is plain TCP over `ess0`.

### 9 · Performance Co-Pilot  ·  independent of everything above
```sh
versions | grep -i pcp ; chkconfig | grep pmcd ; /etc/init.d/pcp start
pminfo -f hinv                                   # does hinv.ncpu exist
pmval -h aurora kernel.all.cpu.idle              # a REMOTE fetch from lucy
```
☐ Remote fetch works. ☐ Note the **exact** metric names — IRIX differs from Linux.
☐ Run `pminfo -f hinv.ncpu` on aurora in **both** power shapes and confirm it tracks
4 versus 20. That decides whether PCP or `tess-probe` is Tess's discovery path.

### 10 · GM lucy↔aurora  ·  do it for the satisfaction
Three known blockers, in order — full evidence in `GM-MPT-INTEROP.md`:

☐ **a.** `drivers/irix/make-os.in:317-318` — add `-rpath /usr/myricom/lib64` and
`-version-info` so libtool emits a real `libgm.so` instead of a convenience archive.
The March build produced only `libgm.la` + `libgm.a`, and **MPT dlopens
`/usr/myricom/lib64/libgm.so`** — a static archive cannot be dlopened.
☐ **b.** Build `tools/gm_cmd_shim.c` *into* libgm — `gm_register_cmd_memory` is the one
symbol of MPT's 23 that GM 1.6.4 lacks. `dlsym` searches only the library MPT opened, so a
separate `.so` will not do. (SGI's libgm is not a substitute: GM enforces a
library↔driver build-ID match, and you already hit that error.)
☐ **c.** Run the mapper. `gm_board_info` currently says *"Node ID not set, mapper not yet
run?"* and *"No routes found"* — until it runs, nothing crosses the fabric at all.

Then:
```sh
setenv MPI_USE_GM 1 ; setenv MPI_GM_VERBOSE 1
mpirun -v lucy 1, aurora 1 /usr/bin/hostname     # TWO hosts only
```
Match the outcome against the four `libmpi` error strings in `GM-MPT-INTEROP.md` —
they localise any failure exactly.

**Then unset `MPI_USE_GM`.** Leave selection to MPT: it picks GM automatically for a
two-host job and falls back to TCP when arthur joins. Setting it permanently makes a
three-host job *terminate* rather than fall back.

Card note: the PCI-X M3F-PCIXD-2 does not work with this driver. The M3F-PCI64D /
LANai 9.3 does.

---

## Predictions worth falsifying

Written down now so the measurements mean something. If any is badly wrong, the design
assumptions behind it need revisiting.

| measurement | prediction |
|---|---|
| aurora → lucy sustained rate, 20 CPUs | ~2.9 MB/s — 26% of 100BASE-TX |
| tiles/s arriving at lucy | ~265, of 12 KB each |
| shade loop on lucy (iteration → RGB, LUT) | **20–100 Mpix/s.** If it returns 5, something is calling `log()` per pixel |
| full-frame recolour, 2.3 Mpix | 20–120 ms shading + ~60 ms `XShmPutImage` |
| pixel-identical output across R12000/R14000/R16000 | identical, with `-O3 -OPT:roundoff=0:IEEE_arithmetic=1` |
| adding arthur to a 20-CPU aurora job | roughly break-even; the gigabit rate decides the sign |

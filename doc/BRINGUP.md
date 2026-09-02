# Bring-up guide — proving the foundation

Goal: get `mpirun` running a real job across lucy, arthur and aurora, over the links you want
it on. Nothing here builds Tess — the deliverables are a working command line, a written-down
configuration, and four measurements that settle open design decisions. Budget half a day, most
of it waiting on `inst`.

Two files from this repo need to reach the SGI (`/work/tess`, or wherever your shared path is):

```sh
rsync -a baseline/mandelmpi-ppm-tiler.c tools/pingpong.c lucy:/work/tess/
```

`mandelmpi-ppm-tiler.c` is the MPI tiler that already exists — step 7 uses it as the real test
rather than writing anything new. `pingpong.c` is the link benchmark for step 8.

**For the tickable one-page version, see `doc/CHECKLIST.md`** — this file is the reference
behind it, organised by subsystem rather than as a sequence.

> **Pasting these blocks into IRIX:** the root shell is `csh`, which does **not** honour `#`
> comments interactively — it tries to glob what follows, and an unmatched `?` or `*` aborts the
> whole command with `No match.` before it runs. Type `sh` first and paste into that, or paste
> commands one at a time. Comments here have been stripped of glob characters, but `#` itself
> still isn't a comment to csh.

Order matters. Each step has a check; if the check fails, stop there rather than continuing,
because every later failure looks like the same symptom (`MPI: could not run executable`).

One ordering note that overrides the section numbering below: **get three-host `mpirun` working
over plain ethernet before bringing up either fabric.** The fabrics are optimisations, and
adding them first means a failure could be in any of three layers. Ethernet-only is the control
experiment, and it is also the point at which Tess development can begin.

Two things found in the media that change what to expect, both detailed in place:

- **MPT uses one interconnect per job**, so a three-host job runs on TCP/IP throughout — but
  under TCP each *pair* still resolves to its own interface. That is what lets HIPPI and gigabit
  be used at once, and it makes the naming in step 3 the mechanism rather than a nicety.
- **The aurora link is gigabit, not Myrinet.** aurora cannot take a HIPPI board (5 V cards,
  3.3 V slots) and a switchless HIPPI star needs a fourth board. Gigabit gets within about a
  percentage point of GM with no software work at all — see step 5. GM is demoted to a
  two-host benchmark project.

**Do all of this with aurora in single-brick mode.** Aurora is 4 CPUs per brick, so the main
brick alone is a quad-CPU box with 4 GB — same hostname, same configuration — and nothing in
this guide benefits from spinning up all five bricks, the router and the L2 controller. That
gives a **10-CPU cluster** (lucy 2 + arthur 4 + aurora 4), which is plenty to prove the
foundation and a better balance for testing than one host doing 89% of the work. Power up the
remaining four bricks for benchmarks, when aurora becomes 20 CPUs and the cluster 26.

---

## 1 · Take stock

On each of lucy, arthur, aurora:

```sh
uname -aR                                   # expect IRIX64 ... 6.5.30
hinv -c processor -t ; hinv -c memory
versions -b | egrep -i 'mpt|mpi|arraysvc|motif|pcp'
versions -b | egrep -i 'hippi|myrinet'
```

Write the output down. This is also the moment to confirm what arthur actually is and what
shape aurora is in: `hinv -c processor` reports 4 CPUs on one brick and 20 with all five
powered, and `sysmp(MP_NUMNODES)` says 1 versus 5. Both are fine — Tess resolves rank counts
from the detected CPU count rather than storing a number, so one configuration covers both
shapes. See DESIGN §"How the shape is discovered" for the three mechanisms; step 9 below
checks whether PCP can serve as the primary one.

**Check:** all three report 6.5.30 and the same MIPSpro version.

---

## 2 · Install MPT and Array Services

From *IRIX 6.5 Applications August 2006*. Mount the CD or serve the tar over NFS, then on
**each** host:

```sh
su -
inst -f /CDROM/dist
  install mpt mpi sma arraysvcs
  go
  quit
```

`mpi` requires `sma`, and MPT wants Array Services ≥ 3.4 — the CD carries 3.7, which is
newer than the 3.5 in base 6.5.30, so let it upgrade. Expect ~21 MB.

```sh
versions -av | egrep -i 'mpi|mpt|arraysvc'   # MPI 4.4 (MPT 1.9), Array Services 3.7
ls /usr/lib64/libmpi.so /usr/include/mpi.h /usr/include/mpio.h
which mpirun ainfo array arshell ascheck
```

**Check:** `mpirun -np 2 hostname` prints the local hostname twice. If it complains that
Array Services is not running, go to step 4 first — but it usually works out of the box for
a single host.

---

## 3 · Name the interfaces before configuring anything

This is the step people skip, and it is the one that decides which wire your tiles ride.
MPT has no "use this interface" variable: **the TCP path follows name resolution**, so an
interface gets used only if a hostname resolves to its address.

### Every link in the plan carries IP — that is the point

The whole job runs on TCP/IP (step 5 explains why), so each pair simply needs a name that
resolves to the right interface:

| link | driver | how MPI uses it |
|---|---|---|
| **HIPPI** lucy↔arthur | `if_hip` + `harp8` (+ unused `hippibp`) | as TCP — MPT 1.9 has no HIPPI bypass, so IP is the only path anyway |
| **gigabit** lucy↔aurora | whatever driver claims the card | as TCP |
| **100BASE-TX** everything else | `ef0` etc. | admin, ssh, `arrayd`, and the arthur↔aurora pair |

Myrinet is deliberately absent from that table. GM bypass uses no IP at all — SGI's package
installs a GM driver with no network interface, and while GM 1.6.4 does contain an IRIX
`ifnet` driver (`myri0`), its own author flags it unfinished. Since gigabit reaches within about
a percentage point of GM's contribution to a frame, the Myrinet fabric is simply not in the
addressing plan. See step 5.

### The names

Each compute node gets exactly **one canonical name**, and the fabric is implied by it:

```
arthur.hippi.local        arthur, reached over HIPPI  (10.42.1.x)
aurora                    aurora, reached over ethernet — GM needs no address
lucy.local                lucy — see below
```

The technique that makes this work with a single, identical `arrayd.conf`:

> **`arrayd.conf` is the same on every host and names each machine once.
> `/etc/hosts` differs per host, and resolves each canonical name to the best address
> reachable *from that host*.**

So the canonical name is a policy statement — "reach arthur over HIPPI" — and each machine
implements it with whatever interface it actually has.

Keeping the **host octet identical on every fabric** makes `netstat` and `ping` output
unambiguous — the subnet tells you which wire, the last octet tells you which machine:

```
             admin (100BASE-TX)   HIPPI          gigabit
lucy         172.28.4.8           10.42.1.8      10.42.2.8
arthur       172.28.4.17          10.42.1.17     —
aurora       172.28.4.16          —              10.42.2.16
```

Each pair gets the best wire it has: **HIPPI for lucy↔arthur, gigabit for lucy↔aurora**, and
100BASE-TX for administration and for the arthur↔aurora pair that never exchanges tiles anyway.
All of it is TCP/IP, which is what lets a three-host job use both fabrics at once — see step 5.

`/etc/hosts` on **lucy**:

```
10.42.1.17      arthur.hippi.local    arthur     # out over hip0
10.42.2.16      aurora.gige.local     aurora     # out over the gigabit card
172.28.4.8      lucy.local            lucy
```

`/etc/hosts` on **arthur** (HIPPI to lucy, no Myrinet):

```
10.42.1.8       lucy.local            lucy       # lucy's HIPPI address
172.28.4.16     aurora                           # ethernet — they share no fabric
10.42.1.17      arthur.hippi.local    arthur
```

`/etc/hosts` on **aurora** (gigabit to lucy, no HIPPI board — see step 5):

```
10.42.2.8       lucy.local            lucy       # lucy's gigabit address
172.28.4.17     arthur.hippi.local    arthur     # ethernet — they share no fabric
10.42.2.16      aurora.gige.local     aurora
```

**arthur and aurora share no fabric with each other**, and in Tess's design they never need
one: all tile traffic is a star through the master, so those two ranks never exchange a
message. Their entries for each other exist purely so resolution always succeeds.

Two rules that save an evening:

- **A separate subnet per fabric.** Same-subnet addresses on different interfaces leave IRIX's
  routing choice ambiguous, and you get ethernet without being told.
- **Keep every name resolvable and reverse-resolvable on every host.** `arrayd` performs a
  `ruserok()` check on the peer address, so if lucy connects to arthur from `10.42.1.8`,
  arthur must map that address back to a name that appears in `~/.rhosts`. Simplest: list all
  three canonical names *and* the short names in `.rhosts` on every host.

**Check, from each host:** `ping arthur.hippi.local`, `ping aurora`, `ping lucy.local` all
succeed. On lucy↔arthur, `netstat -i` should show the counters climbing on `hip0` rather than
the ethernet interface. On lucy↔aurora they will be on ethernet, and that is expected.

---

## 4 · Bring up IP on HIPPI (Essential PCI HIPPI)

**Your HIPPI is not SGI's.** `ifconfig -a` on lucy shows **`ess0`**, which is Essential
Communications' driver for the SGI PCI HIPPI card (EDR-3437). SGI's own HIPPI 4.0 package —
`hps`, `if_hip`, `hipcntl`, `ifhip.conf`, `hippi.imap`, interfaces named `hipXX` — drives the
XIO HIPPI-Serial board and **does not apply here.** Ignore it; everything below is Essential's
scheme, taken from the vendor readme and the shipped `/etc/init.d` scripts.

### Use release 2.0.3

You have two copies. Take the newer one:

```
Downloads/HIPPI-archive-full/essential/HIPPI_Octane_6.5_IP27_IP30.tar     <- RELEASE 2.0.3
Downloads/hippi-essential-2.0.2/                                          <- RELEASE 1.0.6
```

2.0.3's first release note is Octane-specific and decisive:

> *"XIO BUS ERROR Multiple Errors" was reported at boot time. **The Octane would panic and fail
> to complete its boot.** This error is fixed in this release.*

The configuration procedure is byte-identical between the two releases — only the bug fixes
differ. The package covers **IP27 and IP30**, so the same driver serves arthur (Onyx2) and lucy
(Octane2); both ends of the link run the same stack.

```sh
su -
cd /usr/dist && tar xvf HIPPI_Octane_6.5_IP27_IP30.tar
inst -f .        # install   /  go  /  quit
reboot
```

### Configure it

Cards are `essN` — `ess0` is the first. Four files, then a reboot.

**1 · `/etc/hosts`** — the HIPPI addresses from step 3, on every host.

**2 · `/etc/config/netif.options`** — add the interface, numbering after whatever exists:

```
if2name=ess0
if2addr=lucy-hip
```

**3 · `/etc/config/ifconfig-2.options`** — the number must match `if2name` above:

```
netmask 0xffffff00 up
```

This is also where any `mtu` argument goes, if you want to try a larger frame.

**4 · `/etc/init.d/Essnic.pre`** — runs *before* `ifconfig`. It starts `essd` and `essdmpd`,
and it is where the crucial switchless setting lives. The shipped file says:

> *"By default the HIPPI adapter will enable HARP (HIPPI ARP). HARP … requires the configuration
> of a HARP server on the HIPPI subnet. To disable HARP, uncomment the following line."*

**You have no switch and therefore no HARP server, so uncomment it:**

```sh
/usr/Essential/hippi/bin/essarp -X ess0
```

Leave the shipped `essarp -Z ess0` (disables IP broadcast emulation) in place. The logical
address is auto-discovered by default — `essarp -l` is only needed if you want to pin it.

**5 · `/etc/init.d/Essnic.post`** — runs *after* `ifconfig`, and for a direct-connected pair of
Essential cards you probably need nothing here. Release note 5: *"Direct connected systems
process each other's ARP properly."* Only hosts that cannot do HARP need a static entry:

```sh
/usr/Essential/hippi/bin/essarp -s arthur-hip <ULA> <logical-address> ess0
```

Get the local ULA with `essarp -h`, which prints something like
`ess0: (10.42.1.8) ULA 00:b1:46:00:14:C6 Logical Address 0xe`.

**6 · Load the tuning parameters into the card's EEPROM**, which the installer does not do:

```sh
/usr/Essential/hippi/scripts/esssetup.sh ess0
```

The script carries a parameter set per machine type — `IP30` for the Octane, `IP27` for the
Origin/Onyx2 — and picks by platform.

**7 · Reboot.**

### Verify

```sh
essarp -h                     # interface state, ULA, logical address
essstat                       # counters
ping -s 60000 arthur-hip      # large-packet path check
netstat -i                    # confirm the counters move on ess0, not ef0
```

Two behaviours that will confuse you otherwise, both from the release notes:

- **`arp -a` will not show HIPPI entries.** *"Driver does not use system ARP tables anymore. All
  ARP information is maintained in the driver."* Use `essarp -h`.
- **`ping` genuinely tests the wire.** *"The driver does not loopback IP packets through the
  host's local loopback interface. All IP packets are passed down to the NIC, regardless of
  destination address."*

### Measure it before believing it

Essential ships its own throughput tools, which test the link with no MPI involved — worth
running before `pingpong`, because they isolate the fabric from everything else:

```sh
/usr/Essential/hippi/bin/sink       # on one end
/usr/Essential/hippi/bin/blast      # on the other
```

Source for both is in `/usr/Essential/hippi/C_API/`, alongside `essioctl.h`, if you ever want a
raw-HIPPI transport for Tess. There are also PostScript manuals in `/usr/Essential/hippi/docs/`
(`network.ps`, `char_interface.ps`).

Remember the constraint from step 5: MPT 1.9 has **no HIPPI bypass**, so MPI uses this link as
ordinary TCP over `ess0`. That is fine — it is the bandwidth you want, and the latency
difference does not matter at 3 bytes per pixel.

---

### Arthur's HIPPI: where debugging stopped (27 Jul)

Parked, not solved. The state, so the next attempt starts from evidence:

**Works:** board present in `/hw/hippi/0`, device nodes created, `hipcntl shutdown` and
`versions` both respond — so the XIO host interface is healthy. Firmware versions all agree
at **4.0** (driver, `hipcntl`, adapter src EEPROM, adapter dst EEPROM), so no reflash is
indicated. Front-panel **`FW` LED is lit** — firmware is alive.

**Fails:** `hipcntl hippi0 startup` →
`hps0: bad error code (…010002) from HIPPI-Serial destination side!` then
`trouble bringing up HIPPI: I/O error`. `status` → `HIPPI board is down`.
Runtime warnings: `dst 4650 asleep at cmd id=2`, `Could not enable HIPPI-LE on board, error = 2`.

**Decoded** from `sys/hippi.h`: the trailing digits of those error codes are the destination
error bit vector — `…008` = `HIP_DSTERR_SYNC`, `…002` = `HIP_DSTERR_LLRC`. Both are
receive-side sync failures.

**Front panel:** `LK` dark, `TD` dark, `RD` green, `FW` green, **TX Err Code 00**,
**RX Err Code 11**. Transmit side clean; the whole fault is a two-bit receive error code.
(The code table is in the IRIS HIPPI Administration Guide, which is on the media only in
DynaText form — not extractable here. It would be readable in InSight on IRIX.)

**Inconclusive:** the electrical-loopback test. `hipcntl loopback` only takes effect *"at next
`hipcntl startup`"*, and startup fails — so loopback mode never engaged and that test proves
nothing either way. A bare optical TX→RX loop also failed, but a zero-length loop without a
5–10 dB attenuator can saturate the receiver and produce exactly this signature.

**Still untried, in order of cost:** clean both ferrules and both ports; reseat the board;
full power cycle (done 27 Jul — retest on next boot *before* touching anything else);
**swap in the spare board**, which is the one decisive test; try a different XIO slot.

**Open question:** wavelength and fibre-type compatibility between arthur's SGI HIPPI-Serial
and lucy's Essential card — short-wave multimode versus long-wave single-mode. `esshippi on`
has explicit `short|long` options, so the Essential side at least expects to be told.

**Worth remembering:** this link is worth roughly **2% of a frame**. Arthur joins over `ef0`
in the meantime and nothing on the critical path depends on it.

## 5 · The aurora link — gigabit, not Myrinet

**The constraint that decides this step.** `MPI(1)`: *"there will only be one interconnect
configured for the entire MPI job"*, chosen XPMEM → GSN → MYRINET → TCP/IP, and forcing one
requires **every** host in the job to have that device or the job is terminated. With arthur on
HIPPI and aurora on Myrinet, a three-host job therefore runs on **TCP/IP throughout** — GM is
unreachable for it, and `MPI_USE_GM=1` on three hosts will kill the job rather than fall back.

That sounds like a loss until you notice that "one interconnect" means one *protocol*, not one
wire: under TCP each **pair** resolves to its own interface. So gigabit to aurora and HIPPI to
arthur coexist happily in one job.

### Why gigabit rather than more HIPPI or GM

Two alternatives are ruled out before any software:

- **aurora cannot take a HIPPI board** — they are 5 V, its PCI-X slots are 3.3 V.
- **A switchless HIPPI star needs four boards** (two in lucy, one each in arthur and aurora) and
  there are three. HIPPI stays a lucy↔arthur point-to-point link.

And GM, though it would be fastest, needs three fixes first (`doc/GM-MPT-INTEROP.md`) and cannot
serve a three-host job at all. The numbers, for a 1920×1200 frame against ~6.8 s of estimated
compute:

| aurora's link | transfer | share of frame | software needed |
|---|---|---|---|
| 100BASE-TX today | 0.40 s | 5.9% | — |
| **gigabit** | **0.15 s** | **2.2%** | **none** |
| gigabit + jumbo | 0.07 s | 1.1% | none |
| GM bypass | 0.02 s | 0.3% | shared lib + shim + mapper |

Gigabit gets within about a point of GM on a path that already works.

### Bringing it up

```sh
# on lucy, with the card fitted
hinv -c network                      # is the card seen, and by which driver
ifconfig -a                          # what did IRIX name the interface
```

The real question is IRIX driver support for that specific card — worth settling before
scheduling the rest of the work. Then give it the `10.42.2.8` address from step 3, and on aurora
confirm its own gigabit interface (an Origin 350's base I/O usually has one — `hinv -c network`)
and give it `10.42.2.16`.

**No switch needed.** A direct cable between the two is fine; use a crossover cable if the PHYs
do not auto-negotiate. If you do have a gigabit switch, use it and skip the cabling question.

```sh
ping aurora.gige.local               # from lucy
netstat -i                           # confirm the counters move on the gigabit interface
```

**Then chase jumbo frames**, because on IRIX they matter more than they do elsewhere — the table
above shows 2.2% versus 1.1% for the same hardware:

```sh
ifconfig <if> mtu 9000               # both ends must agree, and any switch must pass it
ping -s 8000 aurora.gige.local       # verify large frames actually traverse
```

Measure it with `tools/pingpong.c` (step 8) rather than trusting the link rate. A PCI gigabit
card in an Octane2's cardcage is limited by the PCI bus as much as by the wire, so anything from
25 to 60 MB/s is plausible and only measurement will say.

### Myrinet, demoted

GM is no longer on the critical path. It remains worth doing eventually because a **two-host
lucy+aurora job** is exactly the case where MPT *will* select GM, and the three blockers are
understood and small — a `-rpath` fix so the build emits `libgm.so`, `tools/gm_cmd_shim.c` for
the one missing symbol, and running the mapper so the fabric has node IDs and routes at all.
`doc/GM-MPT-INTEROP.md` has the evidence and the diagnostic table. Treat it as a benchmark
project rather than a dependency.

Note also that the PCI-X Myrinet card (M3F-PCIXD-2) does not work with this driver
(`lanai 0x0 not supported`); the working one is the M3F-PCI64D / LANai 9.3.

---

## 6 · Configure Array Services

`arrayd` must run on every host, and this is where the fabric hostnames earn their keep:
**point each machine's entry at the address you want MPI traffic to use.**

`/usr/lib/array/arrayd.conf`, identical on all three — one canonical name per machine, with
the per-host `/etc/hosts` from step 3 doing the fabric selection:

```
array tess
    machine lucy
        hostname   lucy.local
    machine arthur
        hostname   arthur.hippi.local
    machine aurora
        hostname   aurora.gige.local

local
    destination array tess
```

Every machine is named on its best wire, and because the whole job runs on TCP/IP those names
are exactly what selects the interface per pair — HIPPI to arthur, gigabit to aurora.

That is all it takes. One array, one hostname per machine, no static routes — because the
name→address decision is made locally on each host, where the knowledge about which board
exists actually lives.

**Add a second array for benchmarking**, though. To compare fabrics you need to run the same
job over ethernet, and the cleanest way is a parallel array whose names resolve to the ethernet
addresses everywhere:

```
array tess-eth
    machine lucy
        hostname   lucy.eth.local
    machine arthur
        hostname   arthur.eth.local
    machine aurora
        hostname   aurora
```

with `lucy.eth.local` → `172.28.4.8` and `arthur.eth.local` → `172.28.4.17` in every
`/etc/hosts`. Then `mpirun -a tess` versus `mpirun -a tess-eth` is a controlled A/B over the
identical job — useful for lucy↔arthur, where it isolates HIPPI's contribution. It tells you
nothing about lucy↔aurora, since GM is selected by MPT rather than by name resolution; there,
`MPI_BYPASS_OFF=1` forces TCP for the comparison instead.

This is `-array`'s legitimate use — choosing a measurement, not working around a limitation.

Then authentication — the installed default blocks everything remote:

```sh
vi /usr/lib/array/arrayd.auth
    # comment out AUTHENTICATION NOREMOTE
    # only the LAST authentication entry read takes effect
    AUTHENTICATION SIMPLE
```

`SIMPLE` needs byte-identical 64-bit keys on every host. On a private lab segment
`AUTHENTICATION NONE` is defensible and much less fiddly — your call.

Also required, because `arrayd` performs the `ruserok()` check even though `rsh` is not used:

```sh
echo "lucy"   >> ~/.rhosts     # on each host, listing the others
echo "arthur" >> ~/.rhosts
echo "aurora" >> ~/.rhosts
chmod 600 ~/.rhosts
grep sgi-arrayd /etc/services   # expect: sgi-arrayd 5434/tcp — identical everywhere
```

Then validate and start:

```sh
arrayd -c -f /usr/lib/array/arrayd.conf     # syntax
/etc/init.d/array restart
ascheck                                     # cross-node semantics — run after EVERY change
```

**Check, from lucy:**

```sh
ainfo arrays ; ainfo dfltarray ; ainfo -b machines
array uptime                                # all three hosts answer
arshell arthur hostname ; arshell aurora hostname
```

If `arshell` does not exist you have Secure Array Services (`sarrayd`) installed instead —
it is mutually exclusive with the standard flavour and has no `arshell`.

---

## 7 · The real test: run something you already have

Build the existing baseline tiler and run it across all three. This is the moment the
foundation is proven.

```sh
cd /work/tess
cc -64 -mips4 -O2 -o mandelmpi mandelmpi-ppm-tiler.c -lmpi -lm
```

It has three things a strict C89 compiler objects to. These are verified, not guessed — the
same sources run through `clang -std=c89 -pedantic -Wall -Wextra` against the genuine MPT
`mpi.h` from your media report exactly:

```
mandelmpi-ppm-tiler.c:171: mixing declarations and code is a C99 extension
mandelmpi-ppm-tiler.c:161: unused variable 'y'
mandelmpi-ppm-tiler.c:131: unused variable 'provided'
```

Move the `int w;` at line 171 to the top of its block and the file is clean. (The unused
`provided` at 131 suggests someone once started wiring up `MPI_Init_thread` — with the
rank-per-CPU decision it is no longer wanted.)

Set the environment before the first real run, so what you measure is representative:

```sh
setenv MPI_NAP             2      # stop an idle rank pegging a CPU next to the GUI
setenv MPI_BUFS_PER_HOST   128    # a tile scheduler sends many small messages
setenv MPI_MSGS_PER_HOST   4096
setenv MPI_CHECK_ARGS      1      # cheap argument validation while bringing up
setenv MPI_DSM_DISTRIBUTE  1      # pin ranks across aurora's bricks and arthur's nodeboards
```

`MPI_DSM_DISTRIBUTE` is the one that matters on the NUMA machines: it is how MPT spreads ranks
over the available CPUs, and it is the reason Tess uses one rank per CPU instead of threads.
Leave `MPI_STATS` unset for now — step 8 uses it deliberately.

```sh
# single host first
mpirun -v -np 4 ./mandelmpi -w 640 -h 480 -o /work/tess/t.ppm

# then two hosts
mpirun -v -d /work/tess lucy 1, arthur 4 ./mandelmpi -w 1280 -h 1024 -o /work/tess/t.ppm

# then everything — 10 ranks with aurora on one brick
mpirun -v -a tess -d /work/tess \
       lucy 2, arthur 4, aurora 4 ./mandelmpi \
       -w 1920 -h 1200 -max 2000 -o /work/tess/t.ppm

# and once, with all five bricks powered, the full 26
mpirun -v -a tess -d /work/tess \
       lucy 2, arthur 4, aurora 20 ./mandelmpi \
       -w 1920 -h 1200 -max 2000 -o /work/tess/t.ppm
```

`-d` is not optional: the remote working directory defaults to `$HOME`, not `$PWD`, and
omitting it is the single largest cause of `MPI: could not run executable`.

**Check:** `imgview /work/tess/t.ppm` shows a Mandelbrot, and while it runs, `osview` on
aurora shows **every CPU busy, not one** — 4 in single-brick mode, 20 with everything powered.
That observation is the whole point of the rank-per-CPU decision; see DESIGN §3a for why the
threaded `mandel3.c` never managed it on two.

Then write down the working command line. That is the deliverable of this whole guide.

---

## 7a · If you want to visualise on arthur

The obvious plan — move everything to arthur and accept ethernet — is not the best one
available, because Tess splits the GUI from the master. Those are two processes talking over
a socket, and they do not have to live on the same machine.

So there are three arrangements, and the middle one is the good one:

| GUI on | master on | tile path | verdict |
|---|---|---|---|
| lucy | lucy | aurora→lucy over Myrinet, then loopback | default. One wire crossing |
| **arthur** | **lucy** | aurora→lucy over **Myrinet**, lucy→arthur over **HIPPI** | **use this to visualise on the IR2** |
| arthur | arthur | aurora→arthur over **ethernet** | avoid — puts 89% of the traffic on the slowest link |

The middle row keeps every heavy hop on a fabric. Compute traffic still rides Myrinet into
lucy, and the finished tile stream then rides HIPPI out to arthur — which is exactly the pair
of machines that share a HIPPI link. Nothing needs reconfiguring: the master stays where it
is, and only the GUI moves.

```sh
# on arthur, with the master left running on lucy
tess-ui -master lucy.local:5123
```

The cost is that tiles cross the wire twice instead of once. At 3 bytes per pixel that is
6.9 MB for a 1920×1200 frame — a fraction of a second over HIPPI, and it only happens for
tiles the display needs, not per rank. Compare the bottom row, where aurora's 89% share would
be crossing 100BASE-TX.

The row to genuinely avoid is running the master on arthur, because then the Myrinet link is
not in the path at all.

---

## 8 · Measure the links — this decides real design work

Two separate questions: *which* interface carried the traffic, and *how good* each fabric
actually is. Answer both, because one open decision in the design (whether to write a raw
`hippibp` transport at all) turns entirely on the second.

### Which interface

```sh
netstat -i                       # on lucy, before and immediately after a run
                                 # which interface's counters moved
setenv MPI_STATS 1
mpirun -v -a tess -d /work/tess lucy 2, arthur 4, aurora 4 ./mandelmpi \
       -w 1920 -h 1200 -o /work/tess/t.ppm
                                 # per-rank byte counts to stderr at MPI_Finalize
unsetenv MPI_STATS
```

`MPI_STATS` prints only at exit, to stderr — fine here. Tess's live panel will use
`MPI_SGI_stat_get()` instead, which reports the same counters split by transport
(`BYTES_PT2PT_TCP`, `_GM`, `_SHMEM`) while the job runs.

### How good each fabric is

`tools/pingpong.c` measures round-trip latency and bandwidth at five sizes, one of which is
**12288 bytes — exactly one 64×64 tile at 3 bytes/pixel**, which is the message Tess will
actually send.

```sh
cd /work/tess
cc -64 -mips4 -O2 -o pingpong pingpong.c -lmpi -lm

mpirun -v -a tess     -d /work/tess lucy 1, arthur 1 ./pingpong   # HIPPI
mpirun -v -a tess-eth -d /work/tess lucy 1, arthur 1 ./pingpong   # same pair over 100BASE-TX

mpirun -v -a tess     -d /work/tess lucy 1, aurora 1 ./pingpong   # gigabit
mpirun -v -a tess-eth -d /work/tess lucy 1, aurora 1 ./pingpong   # same pair over 100BASE-TX

mpirun -v -np 2 ./pingpong                                        # on-host shared memory

```

Record the 12288-byte row from each. What the numbers mean for the design:

| result at 12 KB | conclusion |
|---|---|
| HIPPI within ~2× of Myrinet/GM | leave `link_mpi` alone; the raw HIPPI path is not worth writing |
| HIPPI much closer to ethernet than to GM | the bypass work in DESIGN §6b has a real payoff |
| gigabit no better than 100BASE-TX | the card, its driver, or the MTU — check `netstat -i` counters and whether jumbo frames survive |
| three-host job slower than the lucy+aurora pair | expected: three hosts forces TCP job-wide, so aurora loses GM. Compare `lucy 1, aurora 4` against `lucy 2, arthur 4, aurora 4` and decide whether arthur earns its place |
| on-host shmem hugely fastest | expected, and the reason aurora's ranks cost nothing to add |

Also worth knowing: at 10 CPUs a full 1920×1200 frame is about 6.9 MB of tile data. If a
fabric moves 40 MB/s, transport is ~0.17 s against tens of seconds of compute — which is the
honest context for how much any of this matters.

---

## 9 · Performance Co-Pilot

```sh
versions | grep -i pcp                       # pcp_eoe is in base IRIX; probably already there
chkconfig | grep pmcd ; /etc/init.d/pcp start
pminfo | wc -l
pminfo -f hinv                               # hinv.ncpu, hinv.cpuclock, hinv.physmem
pminfo -f mem ; pminfo -f kernel.all.load
pmval -h aurora kernel.all.cpu.idle          # a REMOTE fetch from lucy — the whole read path
ls /usr/include/pcp/pmapi.h /usr/lib64/libpcp.so
ls /usr/pcp/pmdas                            # example agents to model the tess PMDA on
```

**Check:** `pmval -h <remote>` works from lucy. Note the exact metric names you see — the IRIX
names differ from the Linux ones people quote, and Tess's metric list has to match reality.

**And the one that settles a design decision:** run `pminfo -f hinv.ncpu` on aurora in **both**
power shapes and confirm it reports 4 with one brick and 20 with five. If it tracks correctly,
PCP is Tess's primary discovery mechanism and the node table's static columns come from it. If
it does not — stale, absent, or wrong — then the fallback becomes primary: a small
`tess-probe` binary run over `arshell`, printing one machine-readable line:

```sh
arshell aurora /work/tess/tess-probe
# tess-probe 1 host=aurora cpus=4 online=4 mhz=1000 nodes=1 memkb=4194304 \
#            freekb=3350000 irix=6.5.30 mpt=1.9 abi=64 gm=1 hippi=0
```

A purpose-built probe beats parsing `hinv`, whose output format varies between machines and
IRIX revisions, and it needs only Array Services — which MPI requires anyway. Either way rank
counts are never stored as numbers, so one configuration covers both of aurora's shapes.

---

## 10 · Housekeeping that bites later

```sh
limit descriptors                            # MPI jobs open many fds
# raise the jlimit "files" limit if you hit it — see jlimit(1)
```

And on the Mac, so the remote build loop works: macOS OpenSSH 10.2 has **no DSA support at
all**, so if an IRIX sshd only offers a DSA host key you must generate an RSA one. The
legacy-algorithm `~/.ssh/config` stanza is at the end of `doc/HARDWARE-CHECKS.md`.

---

## When it fails: `MPI: could not run executable`

Almost every first-time multi-host failure produces this one message. The documented causes, in
the order worth checking:

| # | Cause | Check |
|---|---|---|
| 1 | **No `-d`** — the remote working directory defaults to `$HOME`, not `$PWD` | add `-d /work/tess`; this is the most common by a wide margin |
| 2 | Binary not present, not executable, or a different ABI on the remote host | `arshell arthur 'ls -l /work/tess/mandelmpi; file /work/tess/mandelmpi'` |
| 3 | `arrayd` not running, or `AUTHENTICATION NOREMOTE` still in effect | `ascheck`; `array uptime`; check `arrayd.auth` — only the **last** authentication entry read takes effect |
| 4 | `~/.rhosts` missing, wrong permissions, or the peer's address reverse-resolves to a name that is not in it | `chmod 600 ~/.rhosts`; make sure the *fabric* name resolves back |
| 5 | Name resolves to an address the remote host cannot reach — a fabric address on a machine without that board | `ping` every canonical name **from every host** (step 3) |
| 6 | Mismatched MPT version or ABI between hosts | `versions -av \| grep -i mpi` on each |

Two more that produce different symptoms: a job that starts and hangs usually means `arrayd`
can reach the host but the ranks cannot connect back — suspect resolution asymmetry. A job that
dies immediately when backgrounded is stdin: redirect it (`< /dev/null`).

`mpirun -v` prints the layout it decided on, which is usually enough to see which of the above
it is.

---

## What "done" looks like

1. `ascheck` clean on all three hosts.
2. `array uptime` answers from all three.
3. A 1920×1200 Mandelbrot rendered by 10 ranks across three machines — and once, by 26 with
   all of aurora powered.
4. `osview` on aurora showing every powered CPU busy during that run. This is the one that
   confirms the rank-per-CPU decision; DESIGN §3a explains why the threaded `mandel3.c` never
   managed it on two.
5. `netstat -i` proving which fabric carried it, not an assumption that it did.
6. The exact working `mpirun` line written down — it becomes the first
   `~/.tess/cluster.args`.
7. A `pingpong` table for HIPPI, Myrinet, ethernet and on-host shmem, with the 12 KB row
   highlighted. This decides whether the raw-link work in DESIGN §6b happens at all.
8. An answer on whether `pminfo -f hinv.ncpu` tracks aurora's two shapes — which decides
   whether PCP or `tess-probe` is Tess's primary discovery path.
9. **An answer on whether MPT binds GM 1.6.** If it does, the interconnect story in DESIGN §0
   stands as written. If it does not, the raw GM transport behind the link seam moves from
   "later, maybe" to the obvious next piece of work — you have a GM you built yourself and
   Tess only needs it for tile payloads.

At that point the foundation is real and Tess's own milestones (DESIGN §9) can start —
beginning with the build loop and two empty windows, not with MPI.

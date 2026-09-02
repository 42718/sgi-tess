# Hardware checks — one 20-minute session before any widget code

Every item here settles a decision that is otherwise a guess. Run as your normal
user, not root. Record the output; several answers change the design.

> **Pasting these blocks into IRIX:** the root shell is `csh`, which does **not** honour `#`
> comments interactively — it tries to glob what follows, and an unmatched `?` or `*` aborts the
> whole command with `No match.` before it runs. Type `sh` first and paste into that, or paste
> commands one at a time. Comments here have been stripped of glob characters, but `#` itself
> still isn't a comment to csh.

Do these on **each** box (lucy, arthur, aurora) unless noted.

## Compiler and toolchain

```sh
cc -version                                  # expect MIPSpro Compilers: Version 7.4.4m
cc -help 2>&1 | grep -i diagnos              # is -brief_diagnostics in the C driver
cc -64 -mips4 -c t.c                         # baseline (the chosen ABI)
cc -64 -mips4 -r14000 -c t.c                # r14000 is absent from the documented -r list
cc -64 -mips4 -O3 -OPT:swp=off -c t.c       # confirm the spelling is -OPT:swp, not -SWP:
cc -64 -mips4 -E -dM -x c /dev/null | sort  # the predefine list the Mac header gate must fake
man make | grep -iE '\-P|parallel|include'   # does IRIX make support parallelism at all
which smake pmake gmake rsync
echo '#include <stdint.h>' > t.c; cc -c t.c  # expected to fail in C89 mode
```

The `-E -dM` predefine dump is the most valuable line here: it is exactly what the
Mac-side syntax gate has to reproduce.

## Motif and the widget assumptions

```sh
versions | grep -i motif                     # motif_eoe / motif_dev / motif21_eoe / motif21_dev
ls -ld /usr/include/Xm /usr/lib64/libXm.so   # which version is the default symlink
ls -l /usr/Motif-2.1/include/Xm/SpinBox.h /usr/Motif-2.1/include/Xm/ComboBox.h \
      /usr/Motif-2.1/lib64/libXm.so
ls /usr/Motif-2.1/include/Sgm/ThumbWheel.h /usr/lib64/libSgm.so
ls /usr/lib/X11/schemes/                     # which schemes are installed
```

Then a small test program that prints `XmVERSION`, `XmREVISION`, `XmVersionString`
and answers four questions:

1. Does `XmNchildType XmFRAME_TITLE_CHILD` render the title on the frame shadow?
2. Does `Tab` reach a childless `XmDrawingArea`? If not, canvas keys must come from
   `XtAddEventHandler(KeyPressMask)` rather than translations.
3. Does a second top-level via `XtVaCreatePopupShell(topLevelShellWidgetClass)` get
   its own 4Dwm frame and independent resize, with **no** `WM_TRANSIENT_FOR`?
   Check with `xprop`. Also check what iconifying the render window does to it.
4. Does `XmFontListInitFontContext` / `XmFontListGetNextFont` yield a usable
   `XFontStruct` for `XTextWidth`?

## X display

```sh
xdpyinfo | grep -i -A3 'default visual'      # expect 24-bit TrueColor (V12 on lucy, IR2 on arthur)
xdpyinfo | grep -iE 'MIT-SHM|SHAPE|GLX'      # gates the XShmPutImage fast path
xdpyinfo | grep -i 'image byte order'        # expect MSBFirst locally
xlsfonts | grep -i 'helvetica-bold-r-normal--1[47]'
xev                                          # do Button4/Button5 (wheel) arrive
systune | grep -i pcmouse
```

Record the actual `red_mask` / `green_mask` / `blue_mask` from a one-line program.
**The pixel packer is built from those masks, never from a hardcoded `0x00RRGGBB`** —
that is the bug the macOS build cannot catch, because macOS is little-endian.

## MPT and Array Services

```sh
versions -av | grep -iE 'mpi|mpt|arraysvc'   # MPT version; standard vs Secure Array Services
relnotes mpt
ls -l /etc/init.d/array /etc/init.d/sarray   # sarrayd means arshell does NOT exist
ls /usr/lib64/libmpi*     # confirm there is no libmpi_mt on IRIX
ls /usr/include/mpi.h /usr/include/mpio.h
man MPI | grep -A20 MPI_NAP                  # confirm MPI_NAP exists in your MPT
man MPI | grep -i -e hippi -e bypass         # if present, you have a pre-1.9 MPT
nm -o /usr/lib64/libmpi.so | grep -i stat    # MPI_SGI_stat_get / _print present
chkconfig | grep array
grep sgi-arrayd /etc/services                # expect sgi-arrayd 5434/tcp
ainfo arrays; ainfo dfltarray; ainfo -b machines
ascheck                                      # run after EVERY arrayd.conf change
array uptime; arshell arthur hostname; arshell aurora hostname
limit descriptors                            # MPI jobs need a large fd limit
hinv -c processor -t; hinv -c memory; uname -aR
```

## Runtime probes worth writing as five-line programs

- **Thread level.** `MPI_Init_thread(…, MPI_THREAD_MULTIPLE, &provided)`, print
  `provided`; repeat with `MPI_THREAD_FUNNELED`. **This decides whether a worker
  thread pool is legal at all, or whether threads-per-rank must fall back to 1.**
- **Error handler.** `MPI_Errhandler_set(MPI_COMM_WORLD, MPI_ERRORS_RETURN)` then send
  to an out-of-range rank: does it return a code, or abort the job? Secondary sources
  say the handler has no effect on MPT; your box settles it.
- **Clock.** `MPI_Attr_get(MPI_COMM_WORLD, MPI_WTIME_IS_GLOBAL, …)` and `MPI_Wtick()`.
  Expect true within one Origin and **false** across hosts — so never subtract rank A's
  timestamp from rank B's.
- **Spawn.** `MPI_Comm_spawn` under `mpirun -up 8 -np 1 ./spawntest`, then again with two
  hosts named. The release notes and the man pages disagree about IRIX; capture the actual
  error. (The design assumes it does not work across hosts either way.)
- **Memory.** `sysmp(MP_SAGET, MPSA_RMINFO, &r, sizeof r)`, print
  `physmem/freemem/availrmem × getpagesize()`, compare the total against `hinv -c memory`.
- **`sysget` existence.** `ls -l /usr/include/sys/sysget.h; nm -o /usr/lib64/libc.so | grep -i sysget`
  — `sysget(2)` came from a source tree, not a running box, so confirm before relying on it.
  (If PCP's `hinv.*` and `mem.*` metrics work, most of this layer is unnecessary anyway.)
- **Load.** Print raw `sysget(SGT_KSYM, KSYM_AVENRUN)` divided by both 1024.0 and 1000.0 and
  compare with `uptime` in the same second.
- **Link speed.** `grep -n 'SIOCGIFDATA\|ifdatareq\|if_getbaud' /usr/include/net/if.h /usr/include/net/soioctl.h`,
  then compare against the `speed …` line from `ifconfig -a`, and check the HIPPI and Myrinet
  interfaces as well as ethernet.
- **Process RSS.** `ls /proc/pinfo | head`, then `sprintf(path,"/proc/pinfo/%05d",getpid())` and
  `ioctl(fd, PIOCPSINFO, &pi)` → `pr_rssize`, `pr_pagesize`, `pr_sonproc`.
- **NUMA.** `sysmp(MP_NUMNODES)` — on aurora expect **5 with all bricks powered, 1 when
  running the main brick alone**; 2 on arthur (two nodeboards); 1 on lucy. This is also the
  cheapest way for the panel to report which shape aurora is in.
- **Clocks.** `clock_getres(CLOCK_SGI_CYCLE, &r)` and `CLOCK_SGI_FAST`, built with plain `cc`.
- **libimage.** `ls /usr/lib64/libimage.* /usr/include/gl/image.h` before designing the `.rgb`
  writer. Run `file` and `elfdump -Dh` on any nekoware library to confirm it is 64-bit and not an
  n32 or gcc-built o32 leftover.

## Measurements, not just presence checks

- Time 1000 send/receive pairs of 12 288 bytes (one 64×64 tile at 3 B/px) lucy↔arthur over
  **HIPPI** and lucy↔aurora over **GM**, and again over ethernet for comparison — `tools/pingpong.c`
  does exactly this. It is the number that decides whether the raw-link work in DESIGN §6b is
  worth doing at all. For the GM pair, force the comparison with `MPI_BYPASS_OFF=1` rather than
  a second array, since GM is chosen by MPT and not by name resolution.
- Time an 8.79 MiB `XPutImage` against `XShmPutImage` against `glDrawPixels` on lucy's V12.
- Time the colouring pass over ~730 000 pixels on lucy. If it exceeds ~100 ms, the recolour
  has to be banded into smaller work-proc chunks to stay interactive.
- **The one that decides whether tile seams are possible:** compile once, then run the
  identical binary on lucy (R14000), arthur (R12000) and aurora (R16000) over the same tile
  and compare the pixel buffers byte for byte. Aurora's own mixed 1 GHz / 800 MHz CPUs are the
  same test at a smaller scale — same core, same code, so they should agree exactly.

## The ssh side, from the Mac

macOS ships OpenSSH 10.2, which has **no DSA support compiled in at all**. If your IRIX
sshd only has a DSA host key, generate an RSA one. Then in `~/.ssh/config`:

```
Host lucy arthur aurora
  ControlMaster auto
  ControlPath ~/.ssh/cm-%r@%h:%p
  ControlPersist 10m
  HostKeyAlgorithms +ssh-rsa
  PubkeyAcceptedAlgorithms +ssh-rsa
  KexAlgorithms +diffie-hellman-group14-sha1,diffie-hellman-group1-sha1
  Ciphers +aes128-cbc,3des-cbc
  MACs +hmac-sha1
```

`ControlPersist` is what turns the remote build from ~10 s of handshake into a couple of
seconds. Check whether `rsync` exists on the SGI (`/usr/nekoware/bin/rsync`); macOS now
ships `openrsync`, which speaks protocol 29 and should interoperate with an old IRIX rsync.

## Performance Co-Pilot

`pcp_eoe` is part of the base IRIX Foundation set, so it is probably already installed —
but the metric *names* on IRIX differ from the Linux PCP names, and the design depends on
knowing the real ones.

```sh
versions | grep -i pcp                       # pcp_eoe (base) and/or the licensed pcp
chkconfig | grep -i pmcd ; ps -ef | grep pmcd
pminfo | wc -l                               # how many metrics does this pmcd offer
pminfo -f kernel.all.cpu                     # exact CPU metric names + values
pminfo -f mem                | head -40      # memory metric names (mem.freemem mem.util.)
pminfo -f kernel.all.load network.interface  # load and per-interface counters
pminfo -f hinv                               # hinv.ncpu, hinv.cpuclock, hinv.physmem
                                             # ^ THIS decides whether PCP can size the job
pmval -h aurora kernel.all.cpu.idle          # does a REMOTE fetch work from lucy
ls /usr/lib/libpcp.so /usr/include/pcp/pmapi.h   # can we link against it
ls /usr/pcp/pmdas/                           # example PMDAs to model tess's on
```

Two things to note from the output: whether `pmval -h <remote>` works from lucy (that is
the whole read path), and whether `hinv.*` metrics exist. If `hinv.ncpu` is there, PCP is the
primary discovery mechanism and `tess-probe` over `arshell` is only a fallback. If it is not,
they swap places — so run this check on aurora in **both** power shapes and confirm
`hinv.ncpu` tracks 4 versus 20.

## HIPPI

MPT 1.9 has no MPI-level HIPPI bypass, but the driver is still in the OS. Confirm what is
actually installed and configured before planning a raw path.

```sh
versions | grep -i hippi
ls -l /usr/etc/hipcntl /usr/etc/hiptest /usr/etc/ifhipconfig /usr/etc/hippi.imap
ls -l /var/sysgen/master.d/hippibp /var/sysgen/master.d/if_hip /var/sysgen/master.d/harp8
ls -l /usr/include/sys/hippi.h
hipcntl -a                                   # link state on each end
ifconfig -a | grep -i hip                    # is IP configured over HIPPI, and what MTU
netstat -i | grep -i hip
hiptest                                      # SGI's own loopback/link test
```

Then measure it honestly before building anything on it: time a TCP round trip and a bulk
transfer over the HIPPI interface versus ethernet. If TCP-over-HIPPI already gives you
tens of microseconds and most of the bandwidth, the raw bypass work may not be worth it.

## Myrinet / GM

```sh
versions -av | grep -i myrinet
ls -l /usr/myricom/lib64/libgm.so /usr/myricom/include/gm.h
/usr/myricom/bin/gm_board_info                # board, firmware and mapper state
ls /usr/myricom/doc                           # the GM API docs for link_gm later
setenv MPI_USE_GM 1 ; setenv MPI_GM_VERBOSE 1
mpirun -v lucy 1, aurora 1 ./hello            # does MPT actually bind GM
```

If MPT will not bind the installed GM 1.6, SGI's 1.0.2 is not an option — it is restricted to
IP27/IP35 and will not install on the Octane2. The choices are a raw GM transport of our own
(DESIGN §6b), IP over GM if the 1.6 build includes that driver, or ethernet for that pair.
Note MPT's GM support lives only in the 64-bit MPI library, so check `file` on `libgm.so`.

## aurora — check it in both shapes

```sh
hinv -c processor                            # 4 in single-brick mode, 20 fully powered
sysmp(MP_NUMNODES) via a 5-line program      # 1 vs 5 — tells you the shape
hinv -c memory                               # 4 GB vs ~20 GB
dplace -v /bin/true                          # is dplace available for thread placement
pminfo -f hinv.cpuclock                      # confirm the 8x1GHz / 12x800MHz split
```

The mixed clocks matter: they are the reason worker threads pull from a rank-local queue
rather than being handed equal slices.

## OpenGL blit path

```sh
glxinfo | grep -iE 'direct rendering|OpenGL renderer|OpenGL version'
ls /usr/include/X11/GLw/GLwMDrawA.h /usr/lib64/libGLw.*
xdpyinfo | grep -i glx
```

On lucy that should report the V12; on arthur, the IR2. Worth timing `glDrawPixels`
against `XShmPutImage` for an 8.8 MB frame on both before deciding the default.

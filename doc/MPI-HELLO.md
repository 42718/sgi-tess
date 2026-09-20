# mpi-hello — proving a multi-host MPI job, and the six traps on the way

Verified on lucy (Octane2, IP30) and aurora (Origin 350, IP35), IRIX 6.5.30,
MPT 1.9 / MPI 4.4, **12 September 2026**. Everything here was observed on the
machines; where something is still unexplained it says so.

This is checklist step 5. It took most of a day, and almost none of that was MPI's
fault — five of the six traps below are shell, naming and array-services problems
that report themselves as a single unhelpful MPI message.

---

## The program

`tools/mpi-hello.c`. Every rank prints its number and its host.

```c
MPI_Init(&argc, &argv);
MPI_Comm_rank(MPI_COMM_WORLD, &rank);
MPI_Comm_size(MPI_COMM_WORLD, &size);
MPI_Get_processor_name(name, &len);
printf("rank %d of %d on %s\n", rank, size, name);
fflush(stdout);
MPI_Finalize();
```

### Build it

```sh
cc -64 -mips4 -O2 -o mpi-hello mpi-hello.c -lmpi
```

`-64` is not a preference. Per `PLATFORM-FACTS.md` §10 the n32 MPI library lacks the
MPI-2 one-sided calls **and has no Myrinet/GM support at all**, so an accidental
`-n32` link produces a binary that runs and silently cannot use the fast fabric.
`-mips4` covers R12000, R14000 and R16000, so one binary serves all three hosts.

Build it on each host, or build once and copy — but it must sit at the **same
absolute path everywhere**. MPT does no file staging.

### Run it

```sh
setenv MPI_USE_TCP 1                                        # on every host, see trap 6

mpirun -d /usr/people/you lucy 2 ./mpi-hello             # local only
mpirun -d /usr/people/you aurora 2 ./mpi-hello           # remote only
mpirun -d /usr/people/you lucy 1, aurora 1 ./mpi-hello   # both — the milestone
```

```
rank 1 of 2 on aurora
rank 0 of 2 on lucy
```

Line order varies. Two hosts' stdout is not synchronised.

**`-d` is not optional.** Without it the remote working directory is `$HOME`, which for
root is `/`. You can watch MPT send it: with arrayd logging on, the request reads
`cmd='cd /usr/people/you; exec ./mpi-hello'`, and without `-d` it reads `cd /`.

Climb the ladder in that order. Local, then remote, then both. Each step fails
differently and the difference is the diagnosis.

---

## Trap 1 · `mpirun -np 2 hostname` is not a valid smoke test

**This one is in SGI's documentation and in an earlier version of our own
`CHECKLIST.md`, and it is wrong on this cluster.**

```
$ mpirun -np 2 hostname
aurora
MPI: could not run executable
MPI: No details available, no log files found (all_signal.c:203)
```

`hostname` prints and exits in microseconds. MPT launches ranks and then waits for
them to register back; a non-MPI binary never does, so MPT concludes the executable
could not be run. Nothing crashed, so there is no log to show you — which is what the
second line is admitting.

The arrayd log proves the launch itself was fine:

```
REQUEST REMEXT(19) from root@lucy exec by user root, cmd='cd /; exec /usr/bsd/hostname'
No active local processes in ASH 0x61f6ffff00000023. Checking status of error log file.
Error log file empty
About to send signal 15 to 0 process(es) in ASH 0x61f6ffff00000023
```

Accepted, executed, gone before MPT looked. **Use a real MPI binary for every test.**
That is why `tools/mpi-hello.c` exists.

`MPI: could not run executable` should therefore be read as *"your program exited
before I could talk to it, or never started"* — not as "exec failed".

---

## Trap 2 · Any output from the remote shell kills the launch

The single most expensive trap of the day.

MPT launches ranks with the Array Services remote-exec call (`asremexec`), which runs
them through a **non-interactive csh**. csh reads `.cshrc` for non-interactive shells.
Anything that writes to the channel corrupts it.

lucy's `/.cshrc` line 156:

```csh
# Ensure correct escape character
stty intr ^C
```

There is no tty, so `stty` fails, and its complaint lands on the launch channel:

```
MPI: lucy: 0x5f57ffff00000015: stty: : Invalid argument
MPI: could not run executable (all_signal.c:206)
```

`arshell` tolerates this — it prints the error and still returns the answer, which is
why it looks cosmetic. MPT does not.

### The fix

```csh
if ( $?prompt ) stty intr ^C
```

**Structure matters.** `set path` must stay *above* any interactive-only guard, unguarded:
ranks need PATH, and must not get output. The general form:

```csh
set path = ( /usr/sbin /usr/bsd /usr/etc /usr/nekoware/bin $path )

if ( $?prompt ) then
	stty intr ^C
	set history = 100
endif
```

### The test

```sh
csh -c true          # must print absolutely nothing
csh -l -c true       # same, and also exercises .login
```

Run both on **every** host before blaming MPI for anything. Note that `.login` is read
only by login shells, so which of the two files matters depends on how the launch shell
is started — test both and guard both.

---

## Trap 3 · `/etc/hosts` canonical names

In `/etc/hosts` the **first** name after the address is canonical; the rest are aliases.

lucy had:

```
192.0.2.8       lucy.local lucy
```

and aurora had:

```
192.0.2.8       lucy
```

So lucy reverse-resolved to `lucy.local`, which is what arrayd logged
(`REQUEST REMEXT(19) from root@lucy.local`), and which aurora could not resolve at all:

```
aurora# ping lucy.local
ping: lucy.local: Non-recoverable failure in name resolution
```

`libxmpi.so` carries the matching error `could not locate host '%s' in array '%s'`, and
`arrayd.conf` names the machine plainly as `lucy`.

### The fix

Plain name first, domain form as an alias, **identical on every host**:

```
192.0.2.8       lucy    lucy.local
192.0.2.17      arthur  arthur.local
192.0.2.16      aurora  aurora.local
```

No daemon restart needed — `gethostbyaddr` reads the file each time.

While you are there, make the fabric aliases agree too. Ours disagreed: lucy said
`lucy-myrinet` / `aurora-myrinet`, aurora said `lucy-myri` / `aurora-myri`. Harmless
today; lethal the moment you point `arrayd.conf`'s `HOSTNAME` at a fabric address to
move MPI's TCP path off ethernet (`PLATFORM-FACTS.md`, "Which interface does MPI
actually use?").

---

## Trap 4 · Read arrayd's own log instead of guessing

Every failure in this sequence was something the daemon knew and MPT could not relay.
`arrayd` takes its flags from `/etc/config/arrayd.options`, which the init script passes
through verbatim:

```sh
echo '-dv 9' > /etc/config/arrayd.options
/etc/init.d/array restart
# ... reproduce the failure ...
tail -40 /var/adm/SYSLOG
```

Empty the file and restart when finished.

Its real options, read out of the binary's own error strings:

| Flag | Takes |
|---|---|
| `-dl` | logging value |
| `-dr` | run value |
| `-dv` | debug value |
| `-f` | config filename |
| `-m` | machine ID |
| `-p` | port number |

An earlier note in this repo suggested `arrayd -n -v` for foreground diagnostics. Those
flags are unverified, and that invocation produced no output and left the daemon stopped
— which then caused a *different* failure (`asgetnetinfo_array('(null)') failed : array
services not available`, which simply means arrayd is not running). Use
`/etc/config/arrayd.options` instead.

### Reading a healthy launch

```
ACCEPTED remote connection from lucy on port 40239
REQUEST REMEXT(19) from root@lucy exec by user root, cmd='cd /usr/people/you; exec ./mpi-hello'
About to send signal 23 to 1 process(es) in ASH 0x61f6ffff0000002c
About to send signal 25 to 1 process(es) in ASH 0x61f6ffff0000002c
```

Signals 23 and 25 are `SIGSTOP` and `SIGCONT` on IRIX. That is MPT's normal startup
choreography — hold each rank until the whole job is launched, then release them all.
Seeing that pair means Array Services did its entire job correctly and anything still
wrong is inside MPI.

---

## Trap 5 · `ascheck` under SIMPLE authentication

`arrayd -c -f` parses one file on one machine. `ascheck` contacts **every** machine and
cross-checks, which is the only thing that proves both nodes agree about what the array
means. Pulled from its own diagnostics, it checks: IP resolution per server, daemon
reachability, `arrayd`/`libarray` protocol version match, `asmachid` versus `LOCAL
IDENT`, duplicate array IDs, `SERVER IDENT` staleness, **the same array name resolving to
different machine lists on different servers**, and multiple arrays with no default.

A clean run names each server:

```
    lucy: <net> 42718
    aurora: <net> 42718
Analyzing configuration data

No errors were found in the current array configuration
```

Two things worth knowing before you trust a failure:

- With `AUTHENTICATION SIMPLE`, plain `ascheck` may report a healthy node as unreachable.
  Its own text says so: *"If array services authentication is in use, you may need to use
  either the `-Kl`/`-Kr` or `-F` options."* Try `ascheck -F` before editing a working
  `arrayd.auth`.
- `IDENT unknown` in `ainfo machines` is the `asmachid` suggestion, not an error. It means
  the kernel cannot mint global array session handles unaided and asks arrayd each time.
  `/usr/etc/arrayconfig -i` on each host plus a reboot sets it. **It does not block
  multi-host MPI** — we confirmed this by getting a two-host job running with both hosts
  still reporting `unknown`.

---

## Trap 6 · The interconnect auto-probe hangs — cause still unknown

With no `MPI_USE_*` variable set, a two-host job **hangs**. Not an error: both ranks
launch, both spin at 100% CPU, and nothing ever attempts a connection.

```
aurora# ps -ef | grep mpi-hello
    root  1999  1816  0 15:07:55 ?  1:34 ./mpi-hello      <- 94s CPU in 90s wall
aurora# netstat -an | grep SYN_SENT
                                                          <- nothing
```

Forcing the transport fixes it, set **on every host** — do not rely on it propagating
from the launching host:

```sh
setenv MPI_USE_TCP 1
```

MPT's selection variables, listed from `libmpi.so`:

```
MPI_USE_XPMEM   MPI_USE_GSN   MPI_USE_GM   MPI_USE_HIPPI   MPI_USE_TCP
MPI_XPMEM_ON    MPI_GSN_ON    MPI_GM_ON
```

**Which probe hangs is not yet established.** GM is ruled out: with `MPI_USE_GM 1` the
probe fails cleanly in about a second and falls back (see below). HIPPI is ruled out:
lucy's `ess0` was already down when the hang was reproduced —

```
ess0: flags=4022<BROADCAST,NOTRAILERS,DRVRLOCK>
        inet 198.51.100.8 netmask 0xffffff00 broadcast 198.51.100.255
```

— no `UP` flag, though note it still carries an address, and `asgetnetinfo_array` reports
configured interfaces, so an address on a down interface has not been excluded as a
source of confusion. GSN and XPMEM are untested. Worth resolving, because
`MPI_USE_TCP 1` is currently a required workaround rather than a choice.

---

## MPI over Myrinet: blocked on the mapper, not on anything fixable

Asked for GM explicitly:

```sh
setenv MPI_USE_GM 1
setenv MPI_GM_VERBOSE 1
```

```
MPI: Unable to use GM (Myrinet) OS bypass interconnect. Using TCP/IP instead.
MPI:Conflicting gmID (2,1) for host aurora in myrinet array.     <- on lucy
MPI:Conflicting gmID (2,1) for host lucy   in myrinet array.     <- on aurora
Unable to set up gm: Found gmID conflict.
```

**gmIDs from `gm_simpleroute` are local, not global.** Each board calls itself gmID 1 and
its peer gmID 2, so the same host is 1 from one side and 2 from the other. MPT needs one
globally agreed node id per host and refuses the pair. Perfectly symmetric, which is why
each host reports it about the other.

That numbering is exactly what a mapper exists to fix. `gm_simpleroute` deliberately
bypasses node-id assignment — the IRIX patch works *because* it keys the route off the
peer's MAC instead (see the `irix-2.0.8` branch) — which is sufficient for `gm_allsize`,
which needs only a route, and insufficient for MPT, which needs an identity.

So **MPI over Myrinet needs the mapper, and on a back-to-back link the mapper cannot
complete**. This is not a configuration problem and no further patch addresses it. It is
waiting on the crossbar switch. The link itself is real and measured: 131.7 MB/s inbound,
80.5 MB/s outbound, 15.7 µs half round trip.

**Also set `MPI_GM_VERBOSE`** whenever you set `MPI_USE_GM`. MPT falls back to TCP
*silently*; without it, a job that quietly ran over ethernet is indistinguishable from one
that used Myrinet.

### A libgm defect this exposed

The GM setup-failure path frees memory it does not own, twice per host:

```
NOTICE: libgm/gm_dma_malloc.c:597:__gm_dma_free():userland
gm_dma_free: pointer does not belong to this port.
```

Harmless here because GM is already giving up, but it is a real defect in the error path
of the IRIX port and belongs with the other fourteen fixes on `irix-2.0.8`.

---

## Two GM housekeeping notes that cost time today

**`gm_mapper` holds GM port 1.** `gm_simpleroute` opens port 1, so a running mapper makes
it fail with `open failed: busy`. `gm_board_info` shows you the holder:

```
Port: Status  PID
   1:   BUSY  1199
```

Since the `_fork` fix the mapper actually daemonises and survives, so it is now capable of
sitting there unnoticed. On a back-to-back fabric it cannot map anything — keep
`chkconfig myrinet_mapper off` until the switch arrives.

**A rebuild silently reverts the `gm_simpleroute` patch** if the patched source is not in
the build tree. Ours lived on `/data88` rather than in the tree, so a rebuild regenerated
the pristine binary and the install overwrote the good one. It fails like this:

```
setting back-to-back route
could not set unique ID 1: invalid parameter
```

which cannot happen in the patched build, where that call sits inside `if (!back)`.
Detect it in one line:

```sh
strings /usr/myricom/bin/gm_simpleroute | grep zero-length
```

Silence means it is the unpatched original. Both machines should build from the
`irix-2.0.8` branch, not from a pristine tarball plus a file on a fileserver.

---

## Checklist

Before calling a multi-host MPI problem an MPI problem:

1. `csh -c true` and `csh -l -c true` silent on every host
2. `/etc/hosts` identical everywhere, plain names canonical
3. `ascheck` clean (try `-F` under SIMPLE auth before believing a failure)
4. `array uptime` answering for every host
5. Same MPT build on every host — `versions -av | egrep -i 'mpt|mpi|sma'`
6. The binary at the same absolute path everywhere, `-64`
7. `-d` on the `mpirun` line
8. A **real MPI binary**, never `hostname`
9. `MPI_USE_TCP 1` on every host until trap 6 is resolved
10. `-dv 9` in `/etc/config/arrayd.options` when any of the above still fails

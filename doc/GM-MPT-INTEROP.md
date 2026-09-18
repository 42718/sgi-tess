# Will MPT 1.9 use your GM 1.6.4?

Evidence-based answer: **yes in principle — the interface is compatible — but three
things stand in the way, and only one of them was on my original list.** Everything
below was read out of the actual binaries, headers and build records, not inferred.
Every claim names the file it came from so it can be re-checked.

In dependency order: **(1)** the build produced a static `libgm.a`, not the `libgm.so`
that MPT dlopens; **(2)** `gm_register_cmd_memory` is missing and needs a three-line
shim; **(3)** the GM mapper has never been run, so the fabric has no node IDs or
routes and nothing can cross it yet. All three are small, known fixes. The parts that
could have been fatal — struct layouts, API version, ABI — check out clean.

Sources used:

| What | Where it came from |
|---|---|
| `libmpi.so` (64-bit, MIPS-IV) | extracted from `dist/mpi.sw64`, *IRIX 6.5 Applications August 2006* |
| SGI's `libgm.so` + `gm.h` | extracted from `myrinet.sw`, `6.5.13_myrinet_1.0.2.tar` — used only as the reference for what MPT expects |
| GM 1.6.4 `include/gm.h` | `ClaudeData/myrinet-1.6.4/` |

Tools: `doc/instx.py` for the inst images, and a small big-endian MIPS ELF reader
(macOS `nm`/`readelf` will not read IRIX MIPS objects).

## How MPT actually reaches Myrinet

Not by linking. `libmpi.so`'s only `DT_NEEDED` entries are `libarray.so`, `libc.so.1`
and `libexc.so`, and it has **zero undefined `gm_*` symbols**. Instead it imports
`dlopen`, `dlsym`, `dlclose`, `dlerror` and contains the string:

```
/usr/myricom/lib64/libgm.so
```

So **MPT dlopens GM at runtime, from that hardcoded absolute path**, and resolves each
function it needs by name. Three consequences:

1. **The path is not searched — it is literal.** Your GM's shared library has to *be*
   `/usr/myricom/lib64/libgm.so`, or be symlinked there. `LD_LIBRARY64_PATH` will not help.
2. **It must be 64-bit.** `lib64` in the path, and MPT's GM support exists only in the
   64-bit MPI library. This is why Tess is a `-64` build.
3. **Version compatibility is per-symbol**, decided at `dlsym` time, not at link time.
   Which makes it checkable in advance — and shimmable.

MPT also defines 26 of its own `MPI_SGI_gm_*` functions, so the GM transport logic is
inside `libmpi`; your `libgm` only has to provide the primitives.

## The 23 functions MPT looks up

Extracted from the string table of `libmpi.so` (each appears alongside an internal
`gm_xxx_p` function-pointer name, which is the dlsym-into-pointer idiom):

```
gm_allow_remote_memory_access   gm_get_node_id            gm_provide_receive_buffer
gm_close                        gm_host_name_to_node_id   gm_receive
gm_deregister_memory            gm_init                   gm_register_cmd_memory
gm_directed_send_with_callback  gm_min_size_for_length    gm_register_memory
gm_dma_free                     gm_num_receive_tokens     gm_send_with_callback
gm_dma_malloc                   gm_num_send_tokens        gm_set_acceptable_sizes
gm_finalize                     gm_open                   gm_unknown
gm_get_host_name                gm_perror
```

Checked against GM 1.6.4's `include/gm.h`: **22 of 23 are present.**

## The one gap

`gm_register_cmd_memory` exists in SGI's library and **not anywhere in GM 1.6.4** — not in
`gm.h`, not in `libgm/`. It is exported by SGI's `libgm.so` (confirmed in its dynamic symbol
table), and SGI's `gm.h` declares it immediately after `gm_register_memory` with an
**identical signature**:

```c
/* SGI gm.h lines 1485-1488 */
GM_ENTRY_POINT gm_status_t gm_register_memory     (struct gm_port *p, void *ptr, gm_size_t length);
GM_ENTRY_POINT gm_status_t gm_register_cmd_memory (struct gm_port *p, void *ptr, gm_size_t length);
```

GM 1.6.4 has `gm_register_memory` and `gm_register_buffer`, but no `cmd` variant — the
distinction appears to have gone away. Since the signature is identical and
`gm_register_memory` is the general case, a forwarding shim is well-founded:
`tools/gm_cmd_shim.c`.

Whether the shim is even needed depends on whether MPT treats the lookup as mandatory. The
error strings suggest it does (see below), but one run settles it.

## Binary compatibility of what crosses the interface

`dlsym` succeeding proves nothing about struct layouts — MPT interprets what `gm_receive()`
returns. So these were compared directly between SGI's `gm.h` and GM 1.6.4's:

| Item | Result |
|---|---|
| `union gm_recv_event` | **identical** — same 10 significant lines. This is the one that matters most |
| `enum gm_status` values | **all 28 shared codes have identical numeric values** (`GM_SEND_TIMED_OUT`=15, `GM_SEND_DROPPED`=19, `GM_ABORTED`=25, …) |
| new codes in 1.6.4 | `GM_ACCESS_DENIED`, `GM_UNTRANSLATED_SYSTEM_ERROR` — appended, so old code is unaffected |
| `GM_NUM_STATUS_CODES` | 26 vs 29 — a *count*, not a status; differs only because codes were appended |
| `gm_size_t` | `gm_u64_t` in both |
| `gm_u64_t` / `gm_u32_t` / `gm_u16_t` | `unsigned long` / `unsigned int` / `unsigned short` in both |
| `GM_API_VERSION_1_0 … 1_3` | same constants `0x100`–`0x103` in both |

That last row is the designed compatibility mechanism: `gm_open()` takes an API version, so
a client built against 1.3 is meant to keep working against a newer library. MPT asks for an
API version it knows, and GM 1.6.4 still offers those.

## Diagnosing it on the machine

`libmpi.so` carries four distinct failure messages, which between them localise any problem
exactly. Run with `MPI_USE_GM=1 MPI_GM_VERBOSE=1` and match what you see:

| Message | Meaning | Fix |
|---|---|---|
| `MPI:Unable to locate GM library.` | `dlopen("/usr/myricom/lib64/libgm.so")` failed | wrong path, or not 64-bit. Symlink it; check `file` |
| `MPI:Unable to find symbols in GM DSO.` | dlopen worked, a `dlsym` failed | **this is the `gm_register_cmd_memory` case** — apply the shim |
| `MPI: unable to initialize gm on host %s %d %d.` | `gm_init`/`gm_open` failed | driver not loaded, board down, mapper not run |
| `MPI:Unable to find host %s in myrinet array.` | `gm_host_name_to_node_id` did not recognise the hostname | see below — this one is a configuration trap |

That last message reveals a dependency worth knowing about: **MPT looks up each host in the
Myrinet fabric by the name Array Services gave it.** So the GM host name the mapper knows
must match the `hostname` in `arrayd.conf`. Since `arrayd.conf` names aurora plainly as
`aurora`, GM must also know it as `aurora` — not `aurora-myri` or a fully-qualified variant.

## The ABI question, answered from the source

GM's configure logic makes the IRIX build **64-bit only**: for `mips-sgi-irix*` it sets
`enable_64b=yes` and never sets `enable_32b`, and `LIBGM_32` is never assigned in
`drivers/irix/make-os.in`, so `release: $(LIBGM) $(LIBGM_32)` resolves to the native library
alone. Upstream says so in as many words:

> *"We have determined that it is necessary to make the GM driver 64-bit… For the time being,
> we are assuming that libgm will only be needed in 64-bit format as well."*

So MPT's 64-bit-only GM bypass is **not** blocked by ABI — which is what `-64` was chosen for.

For the record, the `_32.o` objects sitting in `libgm/` are a red herring: they are **ELF
32-bit MSB SPARC**, leftovers from an unrelated Solaris build, and `libgm/libgm.mak` is a
Microsoft NMAKE file. There is not one MIPS artifact in that tree — it is source, not output.

## What the build actually produced — three blockers, in order

Two further investigations (the GM 1.6.4 tree, and the March–May session in which it was
built) line up exactly with the binary analysis above. In dependency order:

### 1. There is no `libgm.so` — only a static archive

The build produced:

```
myrinet-1.6.4/binary/.gm_uninstalled_libs/lib/libgm.la
myrinet-1.6.4/binary/.gm_uninstalled_libs/lib/.libs/libgm.a
```

No shared object. And the cause is identifiable in the source: `drivers/irix/make-os.in:317-318`
links libgm without `-rpath` and without `$(LDFLAGS)`, so `IRIX_64_BIT_LDFLAGS = -rpath
/usr/myricom/lib64` never reaches libtool. With an empty rpath, libtool takes its
convenience-library branch (`ltmain.sh:2203-2212`) and emits a `.al` archive plus the `.la`
wrapper instead of a `.so`.

**MPT dlopens `/usr/myricom/lib64/libgm.so`. A static archive cannot be dlopened, so this
blocks MPT completely** — before any question of symbols or struct layouts arises. The fix is
that one rule: pass `-rpath /usr/myricom/lib64` and `-version-info`, and invoke `$(LIBTOOL)`
rather than a bare `libtool` from `$PATH`. `ltconfig:1950-1977` already knows how to build IRIX
shared libraries, so nothing deeper is missing.

### 2. `gm_register_cmd_memory` still has to be added

As above — `tools/gm_cmd_shim.c`, compiled into libgm itself.

Note this also rules out the tempting shortcut of keeping *SGI's* libgm (which has the symbol)
against the new driver. GM enforces a build-ID match between library and driver, and that exact
failure was already observed during the build:

> `The GM user library linked with this program may not be compatible with the installed driver.`
> `Driver build ID is "1.6.4-supernova …"` / `User lib build ID is "1.4.1pre10_16ports_for_SGI …"`

So the symbol has to be added to the 1.6.4 library; SGI's cannot be substituted.

### 3. The mapper has never been run

`gm_board_info` on the Origin 350 reports:

```
Could not determine this node ID.
This is node 0 (o350)  node_type=0
   *** Node ID not set, mapper not yet run?
The mapper 48-bit ID was: 00:00:00:00:00:00
   *** No routes found ***
```

The mapper binaries were built (`mapper`, `file_mapper`, `load_routes`, `split_routes`) but
never executed, and no GM traffic between two machines has been tested — `gm_allsize`,
`gm_nway`, `gm_dirsend`, `gm_stress` were all compiled and none was run. **Until the mapper
runs, there are no node IDs and no routes, so nothing can cross the fabric** — not MPT, not
GM's own tests. This is a prerequisite for everything else, and it is also what MPT's
`MPI:Unable to find host %s in myrinet array.` message would be reporting.

So the honest state is: **driver loads and the board initialises; the fabric has never carried
traffic.** That is a good deal further than nothing, and it is less far than it looks.

### Two hardware notes from the same evidence

- The working card is the **M3F-PCI64D / M3-MV2.0, LANai 9.3** (serial 138681, 2 MB SRAM,
  133 MHz). Verified via `gm_board_info`.
- The **PCI-X card (M3F-PCIXD-2) does not work** with this driver:
  `gm_init_eeprom_dependent_functions: lanai 0x0 not supported`, followed by
  `Could not copy EEPROM`. Definitive, from the boot log.

## IP over GM: it exists on IRIX, and it is unfinished

Better than expected, with a caveat. GM 1.6.4 carries a genuine IRIX `ifnet` driver —
`drivers/irix/ip/gx.c` (56 KB) — that attaches an **`ifconfig`-able `myri0`** interface with
`GM_IP_MTU` 9000 (jumbo) or 3752. It is compiled into `myrigm.o`, and the attach is enabled
upstream (`GM_IRIX_IFNET_READY 1`).

But its author flagged it unfinished (`gx.h:1` — *"nasty ifnet driver thing"*; an "empirical
magic number" comment), the `ml load` and `ifconfig myri0` steps are explicitly *not* done by
the installer, the ethernet install hooks it calls are missing from the IRIX stub, and
upstream's last written word on the subject is `CHANGES:319` — *"GM is now supported on IRIX
6.5, except that IP support is not working."*

So: **`myri0` is worth ten minutes of `ifconfig`, not a plan.** If it comes up, assign
`10.42.2.0/24` and MPT's TCP fallback lands on Myrinet instead of ethernet. If it does not,
that subnet stays unused and the fallback is ethernet.

## Verdict

The two things that would have been show-stoppers are both fine: the receive-event union is
byte-identical, and the API version constants match. The remaining risk is one absent symbol
with a known signature, which is a small, well-understood patch rather than a research
project. If it does turn out that MPT will not drive GM 1.6.4, the fallback is not ethernet —
it is a raw GM transport of our own behind the link seam in `DESIGN.md` §6b, using the same
`libgm` directly. You would then be using GM through an API you control instead of through
MPT's 2003 expectations of it.

## GM 2.0.8 on IRIX — it works (8-9 September 2026)

Everything above was written about GM 1.6.4. **2.0.8 now builds and runs on lucy**: the driver
attaches, `gm_board_info` reports the board, and two of the three blockers above are closed.
It took eleven build fixes and one real bug fix, all on branch `irix-2.0.8` in
`~/dev/myrinet-gm`, with `drivers/irix/README.build-2.0.8` as the runbook. Eleven fixes were needed; they are on branch `irix-2.0.8` in
`~/dev/myrinet-gm`, one commit per fix with the evidence in each message, and
`drivers/irix/README.build-2.0.8` on that branch is the build runbook.

Why 2.0.8 at all: it makes the **LANai X / PCI-X** card a first-class citizen. `--enable-lX`
defaults to `yes` (`configure.in:56`) and the prebuilt `mcp/gmcp_array_64b_lX_16k.c` firmware
ships with it, so the `M3F-PCIXD-2` that 1.6.4 rejected with
`gm_init_eeprom_dependent_functions: lanai 0x0 not supported` should work. The rev-4 hack on
`supernova` is obsolete. This has **not** been tested on aurora yet — 2.0.8 has only been
built on IP30, never on IP35.

### Where the three blockers stand

| Blocker | Status |
|---|---|
| **1. static archive, no `libgm.so`** | **Fixed.** `libgm.so.1.0` now builds, with `libgm.so` and `libgm.so.1` symlinked to it. `file` reports `ELF 64-bit MSB mips-4 dynamic lib MIPS - version 1` |
| **2. `gm_register_cmd_memory` missing** | **Fixed.** `libgm/gm_register_cmd_memory.c` on the branch forwards to `gm_register_memory`, superseding the loose `tools/gm_cmd_shim.c`. Compiled into `libgm.so.1.0` on lucy and confirmed with `nm`: `0040adb0 T gm_register_cmd_memory`. **All 23 symbols MPT resolves by dlsym are now exported by the library at the path it opens** |
| **3. mapper never run** | **Unchanged, and now the only one left.** `gm_board_info` on lucy under 2.0.8 says it plainly: `Mapper is 00:00:00:00:00:00`, `Map version is 0`, `0 hosts`, `Network is NOT fully configured` |

The 22 were checked individually against `libgm.so.1.0` with `nm -B`, not against `gm.h`:
`gm_init gm_open gm_close gm_finalize gm_receive gm_perror gm_unknown` (7),
`gm_dma_malloc gm_dma_free gm_register_memory gm_deregister_memory` (4),
`gm_send_with_callback gm_directed_send_with_callback gm_provide_receive_buffer
gm_set_acceptable_sizes` (4), `gm_get_host_name gm_get_node_id gm_host_name_to_node_id
gm_min_size_for_length` (4), `gm_num_receive_tokens gm_num_send_tokens
gm_allow_remote_memory_access` (3).

### The trap worth remembering

**Unset `CFLAGS`, `CPPFLAGS` and `LDFLAGS` before running `configure`.** `configure:2918-2932`
picks libtool's linker ABI by compiling one probe object with `$CC $CFLAGS` and reading
`file(1)` output. The probe has no trailing `-64`, so a nekoware `CFLAGS` carrying `-n32` makes
the probe object N32 and bakes `LD="/usr/bin/ld -n32"` into the generated `libtool`. Every real
compile still gets a trailing `-64` and wins, so the objects are correct and only the shared
link is wrong — it fails with `ld32: INFO 46 : No objects linked.` Check with `grep LD= libtool`;
it must say `-64`. Fixing it needs `rm config.cache` and a full `configure`, since
`config.status` replays the old answers.

### What had to be fixed, and what that says about the port

Upstream does not claim IRIX for GM-2: `README:89-110` lists GM-2.0 as Linux 2.4 and Windows
only, with IRIX 6.5 under "an earlier release". The `drivers/irix` files are dated 2002 to 2003
and `drivers/irix/README.building` still describes testing against `gm-1.4pre14`. The eleven
fixes are consistent with that: nothing deep is wrong with the port, but nobody had run the
IRIX `release:` target to completion, in 2.0.8 **or** in 1.6.4.

- Four are things 1.6.4 had right and 2.0.8 lost: `LIBGM` unset, the recipe linking a
  nonexistent `$(LIB_OBJS)`, four stray slashes on `.gm_make_id`, and `gm_eprintf.c` moved into
  the kernel library without `<stdarg.h>`.
- Three are latent in **both** versions and only surfaced now: `$(binary)/README` with no rule,
  `$(sbindir)/gm_mapper` with no `.mkdir` guard, and `gm_sync.s` that no link line ever
  included, so its assembler rule had never once run.
- One is upstream's own instruction, ignored by upstream: `make-os.in` says "Comment out the
  following line until `gx.c` is ready to use" directly above the line that adds `gx.c`.
- Two are files 2.0.8 simply omits for irix while shipping them for every other Unix:
  `drivers/irix/gm_arch_install`, and `-rpath` on the libgm link that the mapper link already
  gets from the same variable.

### IP over GM is now settled: no

The section above rated `myri0` as "worth ten minutes of `ifconfig`, not a plan". That is now
decided: **`drivers/irix/ip/gx.c` does not compile against GM-2 at all.** GM-2 tightened its
byte-order types (`gm_hton_dp()` returns `gm_dp_n_t`, `include/gm_simple_types.h:107`) and
dropped a trailing argument from both `gm_ethernet_broadcast()` and `gm_ethernet_send()`
(`include/gm_ether.h:137-140,152-155`), and `gx.c` predates all of it — six errors. Reviving IP
would be a real port of a driver its own author called unfinished and upstream called broken
(`CHANGES:531`). It is out, and `10.42.2.0/24` stays unused.

### The attach panic, and what it was (9 September 2026)

Installed and booted on lucy, the 2.0.8 driver killed the machine at attach, three times, always
the same way:

```
PANIC: CPU 0: KERNEL FAULT
EXC code:20, 'Write Address Error'
Bad addr: 0xa8000000484d944d
```

**The cause was a one-byte alignment error, and MIPSpro had reported it in the first build of
the day.** GM-2 modernised the PCI config structure in `include/gm_lanai.h` to model the
capabilities pointer that later PCI revisions put at offset 0x34:

| 1.6.4 | 2.0.8 |
|---|---|
| `gm_u32_t Reserved[2];` | `gm_u8_t Capabilities_Pointer;` then `gm_u8_t Reserved[7];` |

That moves `Reserved[0]` from struct offset 52 to **53** and turns it from a word into a byte.
`drivers/irix/gm/gm_irix.c:3292-3293`, unchanged since 1.6.4, still read two config dwords into
it, so `gm_arch_read_pci_config_32()` performed a 32-bit store to a byte-aligned address. x86
permits that; MIPS raises an address error. Every panic address ended `...4d`, and 53 mod 4 = 1,
which is `struct_base + 53` exactly. The compiler had said so twice:

```
cc-1164 cc: WARNING File = ./drivers/irix/gm/gm_irix.c, Line = 3292
  Argument of type "gm_u8_t *" is incompatible with parameter of type "gm_u32_t *".
```

The fix reads offset 52 as the single byte GM-2's own struct says it is and drops the second
dword read. Both values only fed a debug printer, so nothing functional is lost.

**How it was located, since the method generalises.** The faulting PC lives in the loaded
module, not the kernel, so it cannot be resolved against `unix.N`, whose symbols sit at
`a8000000_20xxxxxx`. Printing the address of a known function at driver init gives the load
base, and the rest is subtraction:

```
ANCHOR gm_init = 0xc000000003cf7528
nm offset      =         0x0003b528   =>  load base 0xc000000003cbc000
PC             = 0xc000000003ce3d50   =>  offset    0x27d50
nm: 00027d04 T gm_arch_read_pci_config_32       (0x4c into that function)
```

**Three cautions worth keeping**, all learned expensively:

- Never register an untested driver for boot-time loading. A panic at attach lands before
  multi-user and costs a single-user rescue. Keep the three `/var/sysgen` files absent, load with
  `ml ld -v -c <object> -p myrigm_`, and register only once it has proved itself. Note that a
  manual load alone does not attach anything, because init's `pciio_iterate()` only finds devices
  already bound to the prefix, so the final proof does require registration and a reboot.
- Do not build with `--enable-debug` for this. It turns on GM's own kernel logging, whose print
  path faults on this 2-CPU machine and produces a second, unrelated panic that masks the real
  one. `--enable-printing=9` alone gives the IRIX layer's tracing without it.
- Turn `chkconfig savecore` on before testing. The dump from the first panic went to swap and was
  overwritten before `savecore` ran.

### What lucy's board actually is

`gm_board_info` under the working driver reports an **M3F-PCI64B-8H, LANai 9.3, 8 MB SRAM,
serial 169214, 134 MHz**, MAC `00:60:dd:49:56:6b`. That is *not* the M3F-PCI64D with 2 MB that
this document records from the Origin 350, so there are at least two different LANai 9 boards in
the collection, and the earlier worry about having only one is unfounded.

### Still to do

1. **Run the mapper.** This is blocker 3 and now the only thing between lucy and a working
   fabric. `gm_mapper` is installed at `/usr/myricom/sbin/gm_mapper`; note that 2.0.8 renamed it
   from 1.6.4's `mapper`, while `drivers/irix/etc/init.d/myrinet` still starts
   `${MYRINET_ROOT}/sbin/mapper`, so that script needs the new name before `chkconfig
   myrinet_mapper on` means anything.
2. **`drivers/irix/gm_install` is still uncorrected.** lucy was installed by hand instead: the
   driver object to `/var/sysgen/boot/`, `libgm.so*` to `/usr/myricom/lib64/`, and
   `bin`, `sbin`, `etc`, `include` copied over with the 1.6.4 versions moved aside as `*.1.6.4`.
   The script's relative paths predate the `binary/.gm_uninstalled_*` layout, and it copies two
   `myrinet_mapper` config files that 2.0.8 no longer installs (1.6.4 placed them from
   `make-os.in:284-291`).
3. **aurora has never seen 2.0.8.** It is IP35, it needs its own native build, and it is where
   the PCI-X M3F-PCIXD-2 question gets answered, since `--enable-lX` is on by default and the
   LANai-X firmware ships. Take `~/Downloads/gm-2.0.8-irix.tar.gz`, which carries every fix.
4. **Then MPT.** `libgm.so` already exports all 23 symbols including the shim, so the remaining
   unknowns are whether `gm_host_name_to_node_id` agrees with the names in `arrayd.conf` and
   whether MPT's 2003 expectations of GM survive a 2.0.8 library.

---

## Answered: MPT will not use this GM, and the reason is the mapper (12 September 2026)

"Still to do" item 4 above asked whether `gm_host_name_to_node_id` agrees with the names in
`arrayd.conf` and whether MPT's 2003 expectations survive a 2.0.8 library. Both questions are
now answered, and neither is the obstacle.

With MPT 1.9 running a real two-host MPI job and GM requested explicitly:

```
MPI: Unable to use GM (Myrinet) OS bypass interconnect. Using TCP/IP instead.
MPI:GM kernel ID 2.0.8_IRIX_rc20031103153228PST ...
MPI:Conflicting gmID (2,1) for host aurora in myrinet array.     <- reported on lucy
MPI:Conflicting gmID (2,1) for host lucy   in myrinet array.     <- reported on aurora
Unable to set up gm: Found gmID conflict.
```

MPT loaded the 2.0.8 library, called into it successfully, read the kernel build ID, and
resolved both hostnames against the Myrinet array. The names agree. The library binds. **The
node ids do not.**

`gm_simpleroute` gives each board a purely local numbering: itself gmID 1, its peer gmID 2. So
aurora is 2 as seen from lucy and 1 as seen from itself. MPT requires one globally agreed node
id per host across the array, sees the pair (2,1) for the same machine, and refuses — symmetric
on both sides, which is why each host reports the conflict about the other.

Assigning globally consistent ids by hand is not available to us: GM-2 derives node ids from
unique ids, `_gm_set_node_id()` accepts only the id the node already has, and
`_gm_set_unique_id()` merely confirms the existing relationship. That restriction is what forced
the `gm_simpleroute` patch on `irix-2.0.8` to skip id assignment entirely and key the route off
the peer's MAC. That was the right call for `gm_allsize`, which needs only a route. It is
structurally insufficient for MPT, which needs an identity.

Handing out globally unique node ids is precisely the mapper's job, and blocker 3 — "the mapper
has never been run" — turns out not to be a matter of running it. On a back-to-back link the
mapper cannot complete at all: the peer receives its one-hop probe with the route byte
unconsumed and drops it (`badcrc__unstripped_route_cnt` exactly half of `netrecv_cnt`), so
`lx_map_explore()` gives up with "mapper is disconnected".

**Verdict: MPI over Myrinet is blocked on the crossbar switch, not on the port, the library, the
ABI, or any remaining patch.** — **WRONG ON BOTH COUNTS. Superseded by the section below
(17 September 2026): the mapper needs no switch, and the mapper was never what MPT needed.** The three original blockers are all cleared. The link itself is
real and measured — 131.7 MB/s inbound, 80.5 MB/s outbound, 15.7 µs half round trip — and
`gm_allsize` uses it happily, because a route without an identity is enough for it.

Two side findings from the same run:

1. **MPT falls back to TCP silently.** Without `MPI_GM_VERBOSE` set, a job that quietly ran over
   ethernet looks identical to one that used Myrinet. Always set it alongside `MPI_USE_GM`.
2. **`libgm`'s GM setup-failure path double-frees.** `gm_dma_free: pointer does not belong to
   this port` fires twice per host during teardown (`libgm/gm_dma_malloc.c:597`). Harmless while
   giving up, but a real defect in the IRIX port's error path.

The full multi-host MPI bring-up, including the five non-MPI traps that cost most of the day,
is written up in `doc/MPI-HELLO.md`.


## Reversed: MPT does use GM, and no switch was ever needed (17 September 2026)

Both claims in the verdict above are false, and they were false for the same reason: a
conclusion drawn from an asymmetric test, then repeated as a fact.

### The mapper maps a back-to-back pair

It needs a mapper running on **both** machines. `lx_map_explore()` in
`mapper/lx/lx_mapper.c:526` looks for a directly connected host with a zero-length route
*before* it goes looking for a crossbar, so the topology is supported by design. But the reply
to a scout comes from the peer's own `gm_mapper` process, via `lx_map_receive_callback()`. A
mapper scouting a machine that is not running one is talking to nobody: the zero-route scouts
are accepted and never answered, it falls through to the one-hop probe, and the peer drops that
as an unstripped route. `badcrc__unstripped_route_cnt` at exactly half of `netrecv_cnt` was the
signature of a missing daemon, not of an unsupported fabric.

With `gm_mapper` running on both, both boards agree:

```
Mapper is 00:60:dd:49:56:6b.
Map version is 21.
2 hosts.
Network is fully configured.
```

Two bugs had to be fixed before the daemon would start at all under 2.0.8, both now on the
`irix-2.0.8` branch of `myrinet-gm`:

- `drivers/irix/etc/init.d/myrinet` started `${MYRINET_ROOT}/sbin/mapper`; 2.0.8 installs it as
  `gm_mapper`, so `chkconfig myrinet_mapper on` silently started nothing.
- `drivers/irix/etc/config/myrinet_mapper.options` contained the single line
  `/usr/myricom/etc/gm/active.args`. The init script cats that file onto the command line; GM-1's
  mapper took an argument file that way and GM-2's parses `--flags` only, so it printed its usage
  and exited.

### The mapper is not what MPT wanted

This is the part that makes the switch irrelevant. In GM-2, `gm_get_node_id()` is a constant:

```c
  /* The local loopback node ID is always 1. */
  *node_id = 1;
```

`libgm/gm_get_node_id.c`. Every GM-2 host reports gmID 1 as its own, map or no map, switch or no
switch. In 1.6.4 the same function asked the driver for the id the mapper had assigned, which is
the behaviour MPT was written against. So a crossbar would have produced exactly the conflict
above, and buying one to fix this would have bought nothing.

### What MPT actually enforces

Traced by instrumenting the two functions MPT resolves, and logging what it asked and got:

```
lucy   : get_node_id = 1   lookup(aurora) = 2   lookup(lucy) = 1
aurora : get_node_id = 1   lookup(aurora) = 1   lookup(lucy) = 2
```

MPT resolves every host in the job by name, locally, on every rank, and each answer must equal
the gmID that host reports for itself. Stock GM-2 fails the uniqueness half: both hosts claim 1.
Forcing distinct ids (11 and 12) fails the agreement half instead, because the name lookups still
answer 1 and 2. On a directly cabled pair exactly one value satisfies both: **2**, because each
board's handle for the other board is 2. Sends stay correct, since "node 2" means "the other
board" from either side.

`GM_MPT_NODE_ID=2` on both hosts, read by a shim in `gm_get_node_id()` and applied to the local
host in `gm_host_name_to_node_id()`, is the whole fix. Committed on `irix-2.0.8`. Then:

```
MPI: Using the GM (Myrinet) OS bypass interconnect.
MPI:hrank       grank   port      gmID  Myrinet hostname
        0           0      2         2    aurora
        1           1      2         2    lucy
```

This reconciles two hosts and only two. A third machine needs genuinely global ids, which GM-2
does not have, and therefore a host table plus translation on the send and receive paths. Since
arthur has no Myrinet, a three-host job is TCP throughout anyway, so the limit costs nothing
today.

### And it is slower than the gigabit plan

Measured lucy ↔ aurora, one fibre, no switch, 1 MB messages:

| path | latency | 1 MB bandwidth |
|---|---|---|
| raw GM, `gm_allsize` | 15.4 µs | **50.9 MB/s** |
| MPI over GM, MPT 1.9 | 18.2 µs | **10.1 MB/s** |

MPT delivers one fifth of the fabric, and the gap is inside MPT. Ruled out by measurement:
`MPI_GM_PAYLOAD` at its 16 KB ceiling changes nothing, `MPI_BUFS_PER_HOST` and
`MPI_BUFS_PER_PROC` at 128 change nothing, copies into GM's DMA memory run at 240 MB/s on lucy
and 1030 MB/s on aurora, and MPT registers memory **9 times for 8.5 MB at startup**, not per
transfer. About 39 ms per megabyte per direction is spent in MPT's own protocol.
`MPI_BUFFER_MAX` cannot help: on IRIX its single-copy path is same-host only unless XPMEM is
involved, and XPMEM is NUMAlink.

For the frame budget in DESIGN.md, a 1920×1200 frame at 3 B/px is 6.9 MB:

| aurora's link | transfer | share of a 6.8 s frame |
|---|---|---|
| MPI over GM, measured | 0.68 s | 10% |
| gigabit, design estimate | 0.15 s | 2.2% |
| raw GM, measured | 0.14 s | 2.0% |

So **the gigabit decision stands**, and MPI-over-GM does not replace it. What changed is the
status of the link seam in DESIGN.md §6b: raw GM is now a measured 5× over MPT on the same
wire, which makes the seam the obvious home for tile payloads on the lucy↔aurora pair rather
than a someday item.

Correctness of the MPT path is not in doubt, for what it is worth: a 1 byte to 1 MB ping-pong
with rank-stamped payloads verified every byte, and the boards counted 129231 packets each way
with no drops, bad CRCs, nacks or resends.

### Side finding, corrected

The "double-free" noted above is not one. `gm_dma_free: pointer does not belong to this port`
fires during MPT's GM **setup** path, before the conflict message, not during teardown. It
disappeared entirely once GM setup succeeded, so it is what MPT does while abandoning a failed
attempt. Still worth a look if it ever reappears on a healthy run.

## Do not use GM_MPT_NODE_ID for real work (18 September 2026)

The shim gets MPT past its gmID check, and then the data path fails
intermittently. Nine runs of the tiler across lucy and aurora:

| mapper | ranks on aurora | frame | result |
|---|---|---|---|
| live | 1 | 800×600 | `seqno error packet(252) seq = 454 exp seq = 540` |
| live | 2 | 800×600 | completed |
| live | 4 | 1920×1200 | `misdirected packet(253) sender node = 2 port = 2` |
| off, static routes | 2 | 800×600 | completed |
| off, static routes | **4** | 800×600 | **misdirected, three times in one run** |
| off, static routes | 2 | 1920×1200 | completed |
| off, static routes | 4 | 1920×1200 | completed |

Two ranks on aurora never failed. Four ranks failed three times in four. The
mapper makes it worse — it reinstalls routes under a live job, and the sequence
error above is a connection reset mid-flight — but stopping it does not fix the
problem, so the mapper is not the cause.

The cause is the trick itself. `GM_MPT_NODE_ID=2` on both hosts makes every host
claim node 2 and every name resolve to 2. That is coherent for *sending*, where
"node 2" means the other board from either side, and incoherent for *receiving*,
where a packet from aurora arrives at lucy as `sender node = 2 port = 2` and
lucy's own rank is also node 2 port 2. MPT demultiplexes on that pair. With one
or two ranks the port numbers usually keep it distinguishable; with four they do
not, and a header lands on the wrong connection — hence tags like 271041792.

**So the shim is a diagnostic, not a setting to run production work under.** It
proved what MPT requires and what GM-2 cannot provide. It does not make MPI over
Myrinet safe here, and a run that completes may have done so by luck of port
assignment rather than because the addressing was sound.

Making it sound needs genuine per-host identity: a host table plus translation on
the send and receive paths inside libgm, rewriting node ids in both directions.
That is real work for a transport measured at **10.1 MB/s**, against gigabit's
~46 and raw GM's 50.9. It is not worth doing.

**Standing recommendation.** MPI runs over TCP on this cluster, which is what
DESIGN.md already chose with the gigabit card to aurora. GM stays for raw
transport behind the link seam (§6b), where we own the protocol and get the full
50.9 MB/s. Leave `MPI_USE_GM` and `GM_MPT_NODE_ID` unset for real runs; set them
only to reproduce the findings above.

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

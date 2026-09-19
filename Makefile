#
# Tess - one shared tree, several machines building in it at once.
#
# The tree lives on NFS and every machine builds natively from it, because
# lucy and aurora have no ssh and a cross-compiler is not MIPSpro. Output goes
# to build/`uname -m`, so IP27, IP30, IP35 and the Mac can all build at the
# same time without overwriting each other's objects.
#
#   gmake              everything this machine can build
#   gmake probe        tess-probe    (inventory, needs nothing)
#   gmake tiler        tess-tiler    (the milestone 0 backend, from baseline/)
#   gmake node         tess-node     (MPI, no Motif)
#   gmake ui           tess-ui       (Motif, no MPI)
#   gmake info         what this machine looks like to the build
#   gmake fresh        clean this architecture, then rebuild it
#   gmake clean        this architecture only
#   gmake distclean    every architecture
#
# Use `gmake fresh` after editing from the Mac. The SGI clocks run ahead of the
# Mac's and the tree is shared, so a file written over NFS arrives looking older
# than the binary built from the previous version, and make will report success
# having done nothing. There is no git on the SGIs and none is needed: the NFS
# folder is the working tree, so a commit on the Mac is already visible here.
#
# The per-architecture split is about not colliding on a shared filesystem, not
# about producing different code. DESIGN.md section 8 requires identical machine
# code across lucy, arthur and aurora, so that tiles computed on different hosts
# have identical escape-time boundaries: same flags everywhere, no -TARG tuning,
# never -Ofast.
#

ARCH    := $(shell uname -m)
SYS     := $(shell uname -s)
BUILD   := build/$(ARCH)
COMMON  := src/common
VPATH    = $(COMMON):src/probe:src/node:src/ui

INVENTORY_SRC = $(COMMON)/tess_inventory.c
WIRE_SRC      = $(COMMON)/tess_wire.c
MANDEL_SRC    = $(COMMON)/tess_mandel.c
COMMON_SRCS   = $(WIRE_SRC) $(MANDEL_SRC)
COMMON_HDRS   = $(COMMON)/tess_types.h $(COMMON)/tess_proto.h \
                $(COMMON)/tess_wire.h $(COMMON)/tess_mandel.h

ifeq ($(findstring IP,$(ARCH)),IP)
# ---- IRIX, MIPSpro. The authoritative build. DESIGN.md section 8. ----
CC        = cc
ABI       = -64 -mips4
CFLAGS    = $(ABI) -O2 -I$(COMMON)
# MPI_SGI_stat_get lives in mpi_ext.h. Probe for it rather than assume: where
# it is missing, the transport counters report "unknown" instead of guessing.
HAVE_MPI_EXT := $(shell test -f /usr/include/mpi_ext.h && echo 1 || echo 0)
NODE_CFLAGS = $(ABI) -O3 -OPT:roundoff=0:IEEE_arithmetic=1 -I$(COMMON) \
              -DTESS_HAVE_MPI_EXT=$(HAVE_MPI_EXT)
# -lpthread goes back when threads do: DESIGN.md section 8 puts it after
# -lmpi, but v1 has no threads and ld64 warns about an unused library,
# which hides the warnings worth reading.
NODE_LIBS = -lmpi -lm
MOTIF     = /usr/Motif-2.1
UI_CFLAGS = $(ABI) -O2 -I$(COMMON) -I$(MOTIF)/include
# -lSgm when the first Sgm widget appears, -limage when .rgb saving lands in
# build 5. Both warn as unused today.
UI_LIBS   = -L$(MOTIF)/lib64 -Wl,-rpath,$(MOTIF)/lib64 \
            -lXm -lXt -lXext -lX11 -lm
else
# ---- macOS, the portability gate. Not a substitute: LP64 little-endian. ----
CC        = clang
CFLAGS    = -std=c89 -pedantic -Wall -O2 -I$(COMMON)
NODE_CFLAGS = $(CFLAGS)
NODE_LIBS = -lm
UI_CFLAGS = $(CFLAGS)
UI_LIBS   = -lm
MPICC     = mpicc
endif

PROBE  = $(BUILD)/tess-probe
TILER  = $(BUILD)/tess-tiler
NODE   = $(BUILD)/tess-node
UI     = $(BUILD)/tess-ui

NODE_SRCS = $(wildcard src/node/*.c)
UI_SRCS   = $(wildcard src/ui/*.c)

.PHONY: all probe tiler node ui info gate fresh clean distclean

all: probe tiler node ui

info:
	@echo "arch      $(ARCH) ($(SYS))"
	@echo "build dir $(BUILD)"
	@echo "cc        $(CC) $(CFLAGS)"
	@echo "mpi_ext   $(if $(filter 1,$(HAVE_MPI_EXT)),present,absent: transport counters disabled)"
	@echo "node      $(if $(NODE_SRCS),$(words $(NODE_SRCS)) source(s),no sources yet)"
	@echo "ui        $(if $(UI_SRCS),$(words $(UI_SRCS)) source(s),no sources yet)"

$(BUILD):
	mkdir -p $(BUILD)

# ---- tess-probe: inventory only, no MPI, no X, builds anywhere ----
probe: $(PROBE)

$(PROBE): src/probe/tess_probe.c $(INVENTORY_SRC) $(COMMON)/tess_inventory.h | $(BUILD)
	$(CC) $(CFLAGS) -o $@ src/probe/tess_probe.c $(INVENTORY_SRC)

# ---- tess-tiler: milestone 0. The 2001 row tiler, C89 corrected, kept
#      buildable so there is always a working MPI backend to test the cluster
#      with while tess-node is being written. Not the design's architecture:
#      rows not tiles, RGB not iteration counts, no epochs. ----
tiler:
ifeq ($(findstring IP,$(ARCH)),IP)
	@$(MAKE) $(TILER)
else
	@echo "tiler: needs MPT, IRIX only"
endif

$(TILER): baseline/mandelmpi-ppm-tiler.c | $(BUILD)
	$(CC) $(NODE_CFLAGS) -o $@ baseline/mandelmpi-ppm-tiler.c $(NODE_LIBS)

# ---- tess-node: MPI plus the compute modules. No Motif, ever. ----
node:
ifeq ($(strip $(NODE_SRCS)),)
	@echo "node: no sources in src/node yet, nothing to build"
else
ifeq ($(findstring IP,$(ARCH)),IP)
	@$(MAKE) $(NODE)
else
	@echo "node: needs MPI headers, IRIX only on this machine"
endif
endif

$(NODE): $(NODE_SRCS) $(COMMON_SRCS) $(INVENTORY_SRC) $(COMMON_HDRS) | $(BUILD)
	$(CC) $(NODE_CFLAGS) -o $@ $(NODE_SRCS) $(COMMON_SRCS) $(INVENTORY_SRC) \
	  $(NODE_LIBS)

# ---- tess-ui: Motif, no MPI. Only where Motif is installed. ----
ui:
ifeq ($(strip $(UI_SRCS)),)
	@echo "ui: no sources in src/ui yet, nothing to build"
else
ifeq ($(findstring IP,$(ARCH)),IP)
	@$(MAKE) $(UI)
else
	@echo "ui: needs X11 and Motif headers, IRIX only on this machine"
endif
endif

$(UI): $(UI_SRCS) $(COMMON_SRCS) $(COMMON_HDRS) | $(BUILD)
	$(CC) $(UI_CFLAGS) -o $@ $(UI_SRCS) $(COMMON_SRCS) $(UI_LIBS)

fresh:
	@$(MAKE) clean
	@$(MAKE) all

# What the Mac can still verify without MPI, X or Motif: the portable half,
# which is where byte-order and C89 mistakes live. DESIGN.md section 8 calls the
# Mac build a gate, not a substitute.
gate:
	$(CC) -std=c89 -pedantic -Wall -I$(COMMON) -c $(WIRE_SRC) -o /dev/null
	$(CC) -std=c89 -pedantic -Wall -I$(COMMON) -c $(MANDEL_SRC) -o /dev/null
	$(CC) -std=c89 -pedantic -Wall -I$(COMMON) -c $(INVENTORY_SRC) -o /dev/null
	@echo "gate: portable sources are C89 clean"

clean:
	rm -rf $(BUILD)

distclean:
	rm -rf build

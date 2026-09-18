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
#   gmake clean        this architecture only
#   gmake distclean    every architecture
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
INVENTORY_OBJ = $(BUILD)/tess_inventory.o

ifeq ($(findstring IP,$(ARCH)),IP)
# ---- IRIX, MIPSpro. The authoritative build. DESIGN.md section 8. ----
CC        = cc
ABI       = -64 -mips4
CFLAGS    = $(ABI) -O2 -I$(COMMON)
NODE_CFLAGS = $(ABI) -O3 -OPT:roundoff=0:IEEE_arithmetic=1 -I$(COMMON)
NODE_LIBS = -lmpi -lpthread -lm
MOTIF     = /usr/Motif-2.1
UI_CFLAGS = $(ABI) -O2 -I$(COMMON) -I$(MOTIF)/include
UI_LIBS   = -L$(MOTIF)/lib64 -Wl,-rpath,$(MOTIF)/lib64 \
            -lXm -lSgm -lXt -lXext -lX11 -limage -lm
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

.PHONY: all probe tiler node ui info clean distclean

all: probe tiler node ui

info:
	@echo "arch      $(ARCH) ($(SYS))"
	@echo "build dir $(BUILD)"
	@echo "cc        $(CC) $(CFLAGS)"
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
	@$(MAKE) $(NODE)
endif

$(NODE): $(NODE_SRCS) $(INVENTORY_SRC) $(COMMON)/tess_inventory.h | $(BUILD)
	$(CC) $(NODE_CFLAGS) -o $@ $(NODE_SRCS) $(INVENTORY_SRC) $(NODE_LIBS)

# ---- tess-ui: Motif, no MPI. Only where Motif is installed. ----
ui:
ifeq ($(strip $(UI_SRCS)),)
	@echo "ui: no sources in src/ui yet, nothing to build"
else
	@$(MAKE) $(UI)
endif

$(UI): $(UI_SRCS) | $(BUILD)
	$(CC) $(UI_CFLAGS) -o $@ $(UI_SRCS) $(UI_LIBS)

clean:
	rm -rf $(BUILD)

distclean:
	rm -rf build

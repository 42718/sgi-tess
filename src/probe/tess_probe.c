/*
 * tess_probe.c - print this host's inventory as one line, then exit.
 *
 * The preflight fallback from DESIGN.md when pmcd is not answering, and the
 * thing that proves arshell works before mpirun is attempted:
 *
 *     arshell aurora /cluster/dev/sgi-tess/build/IP35/tess-probe
 *     # tess-probe 1 host=aurora cpus=4 online=4 mhz=1000 nodes=1 ...
 *
 * -v adds a human-readable block underneath; the machine line is always first
 * so a caller can read one line and stop.
 *
 * C89 throughout.
 */

#include <stdio.h>
#include <string.h>

#include "tess_inventory.h"

int main(int argc, char **argv)
{
    TessInventory inv;
    char line[512];
    int verbose;
    int i;

    verbose = 0;
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose = 1;
        } else {
            fprintf(stderr, "usage: %s [-v]\n", argv[0]);
            return 2;
        }
    }

    tess_inventory(&inv);
    tess_inventory_line(&inv, line, (int)sizeof line);
    printf("%s\n", line);

    if (verbose) {
        printf("\n");
        printf("host          %s\n", inv.host);
        printf("cpus          %d configured, %d online\n", inv.cpus, inv.online);
        printf("clock         %d MHz\n", inv.mhz);
        printf("NUMA nodes    %d\n", inv.nodes);
        printf("memory        %ld MB total, %ld MB free\n",
               inv.memkb / 1024, inv.freekb / 1024);
        printf("release       %s\n", inv.irix);
        printf("ABI           %d-bit\n", inv.abi);
        printf("GM            %s\n", inv.gm ? "libgm present" : "no");
        printf("HIPPI         %s\n", inv.hippi ? "interface present" : "no");
    }

    fflush(stdout);
    return 0;
}

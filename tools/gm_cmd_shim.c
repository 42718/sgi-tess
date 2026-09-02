/*
 * gm_cmd_shim.c — supply gm_register_cmd_memory() for MPT 1.9 over GM 1.6.4.
 *
 * WHY
 *   SGI's MPT 1.9 does not link against libgm. It dlopens the literal path
 *   /usr/myricom/lib64/libgm.so and resolves 23 gm_* functions with dlsym().
 *   Twenty-two of those exist in GM 1.6.4. One does not:
 *
 *       gm_register_cmd_memory()
 *
 *   SGI's own libgm exports it, and SGI's gm.h declares it directly after
 *   gm_register_memory() with an identical signature:
 *
 *       gm_status_t gm_register_memory     (struct gm_port *, void *, gm_size_t);
 *       gm_status_t gm_register_cmd_memory (struct gm_port *, void *, gm_size_t);
 *
 *   In GM 1.6.4 the distinction is gone — there is only gm_register_memory() (and
 *   gm_register_buffer()). Forwarding is therefore the natural reading, and it is what
 *   this does.
 *
 *   Only needed if MPT reports "MPI:Unable to find symbols in GM DSO." See
 *   doc/GM-MPT-INTEROP.md for the full evidence and the diagnostic table.
 *
 * HOW
 *   Build it into libgm itself, so that the single library MPT dlopens exports every
 *   symbol MPT looks for:
 *
 *       cc -64 -mips4 -c -I/usr/myricom/include gm_cmd_shim.c
 *       # then relink your libgm.so with gm_cmd_shim.o included, e.g.
 *       ld -shared -64 -o libgm.so <your gm objects> gm_cmd_shim.o -all
 *
 *   A separate .so will NOT work: dlsym() on MPT's handle searches that one library.
 *
 * CAVEAT
 *   If "cmd memory" needed different treatment from ordinary registered memory in
 *   GM 1.4 — a distinct DMA window for the command queue, say — then forwarding is
 *   an approximation and DMA could misbehave rather than fail cleanly. Verify with a
 *   real transfer, not just a successful MPI_Init: run tools/pingpong.c over the GM
 *   pair and check the data arrives intact at every message size.
 *
 * C89, MIPSpro.
 */

#include <gm.h>

gm_status_t
gm_register_cmd_memory (struct gm_port *p, void *ptr, gm_size_t length)
{
    return gm_register_memory (p, ptr, length);
}

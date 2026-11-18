// REXPaint (.xp) loader: gzip stream + layer/tile records (TCOD_load_xp_from_memory).
#include <stddef.h>
#include <stdint.h>

#include "libtcod/console_rexpaint.h"
#include "libtcod/console.h"

enum { MAX_LAYERS = 4 };

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size == 0 || size > 1 << 20) return -1;  // The length parameter is an int; cap for memory.
  TCOD_Console* out[MAX_LAYERS] = {NULL};
  const int count = TCOD_load_xp_from_memory((int)size, data, MAX_LAYERS, out);
  if (count < 0) return -1;  // Rejected by the loader.
  for (int i = 0; i < MAX_LAYERS; ++i) {
    if (out[i]) TCOD_console_delete(out[i]);
  }
  return 0;
}

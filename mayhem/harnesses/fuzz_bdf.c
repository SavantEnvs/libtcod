// BDF bitmap font loader (TCOD_load_bdf_memory). Upstream fixed a leak on a malformed-bitmap path here.
#include <stddef.h>
#include <stdint.h>

#include "libtcod/tileset.h"
#include "libtcod/tileset_bdf.h"

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size == 0 || size > 1 << 20) return -1;  // TCOD_load_bdf_memory takes an int length; cap for memory.
  TCOD_Tileset* tileset = TCOD_load_bdf_memory((int)size, data);
  if (!tileset) return -1;  // Rejected by the parser.
  TCOD_tileset_delete(tileset);
  return 0;
}

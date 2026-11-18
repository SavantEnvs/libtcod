// PNG tilesheet loader (TCOD_tileset_load_mem -> lodepng -> tileset slicing).
// The first two bytes pick the sheet's columns/rows, the rest is the PNG.
#include <stddef.h>
#include <stdint.h>

#include "libtcod/tileset.h"

enum { MAX_TILES = 4096 };

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size < 3) return -1;  // Two shape bytes plus at least one byte of PNG.
  const int columns = 1 + data[0] % 32;
  const int rows = 1 + data[1] % 32;
  static int charmap[MAX_TILES];
  for (int i = 0; i < MAX_TILES; ++i) charmap[i] = 0x20 + i;
  TCOD_Tileset* tileset = TCOD_tileset_load_mem(size - 2, data + 2, columns, rows, columns * rows, charmap);
  if (!tileset) return -1;  // Not a PNG, or the sheet shape does not fit.
  TCOD_tileset_delete(tileset);
  return 0;
}

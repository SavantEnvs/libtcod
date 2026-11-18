// FOV through the public API: map size, transparency layout, origin, radius and algorithm all come
// from the input. Small maps are the interesting ones (upstream fixed tiny-map buffer overflows in
// the permissive and restrictive algorithms), so sizes start at 1x1.
#include <stddef.h>
#include <stdint.h>

#include "libtcod/fov.h"

enum { HEADER = 7, MAX_DIM = 32 };

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size < HEADER) return -1;  // Header (7 bytes) is missing.
  const int width = 1 + data[0] % MAX_DIM;
  const int height = 1 + data[1] % MAX_DIM;
  const TCOD_fov_algorithm_t algo = (TCOD_fov_algorithm_t)(data[2] % NB_FOV_ALGORITHMS);
  // TCOD_map_compute_fov documents that the point of view must be within the map.
  const int pov_x = data[3] % width;
  const int pov_y = data[4] % height;
  const int radius = data[5];
  const bool light_walls = data[6] & 1;
  TCOD_Map* map = TCOD_map_new(width, height);
  if (!map) return -1;
  const uint8_t* bits = data + HEADER;
  const size_t n_bits = (size - HEADER) * 8;
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      const size_t i = (size_t)y * (size_t)width + (size_t)x;
      const bool transparent = i < n_bits ? (bits[i / 8] >> (i % 8)) & 1 : true;
      TCOD_map_set_properties(map, x, y, transparent, true);
    }
  }
  const TCOD_Error err = TCOD_map_compute_fov(map, pov_x, pov_y, radius, light_walls, algo);
  if (err == TCOD_E_OK) {
    // The origin cell is always visible; read the result back so the whole map is touched.
    volatile int seen = 0;
    for (int y = 0; y < height; ++y)
      for (int x = 0; x < width; ++x) seen += TCOD_map_is_in_fov(map, x, y);
    (void)seen;
  }
  TCOD_map_delete(map);
  return 0;
}

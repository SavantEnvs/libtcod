// Console text rendering: UTF-8 decoding, libtcod color-control codes and word wrapping
// (TCOD_console_printn_rect / TCOD_console_get_height_rect_n).
#include <stddef.h>
#include <stdint.h>

#include "libtcod/console.h"
#include "libtcod/console_printing.h"

enum { HEADER = 4, MAX_DIM = 40 };

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size < HEADER) return -1;  // Header (4 bytes) is missing.
  const int width = 1 + data[0] % MAX_DIM;
  const int height = 1 + data[1] % MAX_DIM;
  const TCOD_alignment_t alignment = (TCOD_alignment_t)(data[2] % 3);
  const TCOD_bkgnd_flag_t flag = (TCOD_bkgnd_flag_t)(data[3] % TCOD_BKGND_ALPH);
  const char* text = (const char*)data + HEADER;
  const size_t n = size - HEADER;
  TCOD_Console* console = TCOD_console_new(width, height);
  if (!console) return -1;
  const TCOD_ColorRGB fg = {255, 255, 255};
  const TCOD_ColorRGB bg = {0, 0, 0};
  const int h1 = TCOD_console_printn_rect(console, 0, 0, width, height, n, text, &fg, &bg, flag, alignment);
  const int h2 = TCOD_console_get_height_rect_n(console, 0, 0, width, height, n, text);
  TCOD_Error e = TCOD_console_printn(console, 0, 0, n, text, &fg, &bg, flag, alignment);
  TCOD_console_delete(console);
  return (h1 < 0 && h2 < 0 && e < 0) ? -1 : 0;
}

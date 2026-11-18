// ASCII Paint (.asc) loader: TCOD_console_from_file reads the "ASCII-Paint v<ver>" / "<w> <h>" header
// with fscanf, then (for a name ending in .asc) TCOD_console_read_asc reads w*h cells with fgetc. The
// loader only takes a path, so each input is written unchanged to a scratch .asc file.
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <unistd.h>

#include "libtcod/console.h"
#include "libtcod/console_etc.h"
#include "tmpfile_input.h"

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  char path[PATH_MAX];
  if (mayhem_write_tmp(data, size, ".asc", path, sizeof path) != 0) return -1;
  TCOD_Console* con = TCOD_console_from_file(path);
  unlink(path);
  if (!con) return -1;  // Rejected by the loader.
  TCOD_console_delete(con);
  return 0;
}

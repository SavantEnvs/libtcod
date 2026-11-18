// ASCII Paint (.apf) RIFF loader as a file-input program: TCOD_console_load_apf walks the RIFF chunks
// (sett, imgd, layr) of the file it is given and paints the console from the layer data. It is a
// separate process per input, not an in-process libFuzzer target, because the loader returns on every
// error path without fclose()ing its FILE*, so in one process the leaked streams pile up until fopen()
// fails and every later input is rejected.
#include <stdio.h>

#include "libtcod/console.h"
#include "libtcod/console_etc.h"

int main(int argc, char** argv) {
  if (argc < 2) {
    fprintf(stderr, "usage: %s APF_FILE\n", argv[0]);
    return 2;
  }
  TCOD_Console* con = TCOD_console_new(1, 1);
  if (!con) return 1;
  const bool ok = TCOD_console_load_apf(con, argv[1]);
  TCOD_console_delete(con);
  return ok ? 0 : 1;
}

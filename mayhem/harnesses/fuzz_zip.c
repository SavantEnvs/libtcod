// libtcod's compressed save-file reader: TCOD_zip_load_from_file reads a gzip stream (an int length,
// then that many bytes), and the TCOD_zip_get_* readers decode values from the loaded buffer. The
// loader only takes a path, so each input is written unchanged to a scratch file. The readers have no
// end-of-data signal besides TCOD_zip_get_remaining_bytes, so the harness checks that enough bytes
// remain before each read (and before a skip, the skipped bytes too), as a careful caller would, and lets the input's own bytes pick which reader runs next. The reads are
// capped so that a reader that moves the offset backwards (a negative string length) cannot keep the
// harness's own loop going forever.
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include "libtcod/zip.h"
#include "tmpfile_input.h"

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  char path[PATH_MAX];
  if (mayhem_write_tmp(data, size, "", path, sizeof path) != 0) return -1;
  TCOD_zip_t zip = TCOD_zip_new();
  const int loaded = TCOD_zip_load_from_file(zip, path);
  unlink(path);
  if (loaded <= 0) {  // Rejected by the loader.
    TCOD_zip_delete(zip);
    return -1;
  }
  volatile size_t sink = 0;
  char buf[64];
  for (int reads = 0; reads < 256 && TCOD_zip_get_remaining_bytes(zip) > 0; ++reads) {
    const uint8_t op = (uint8_t)TCOD_zip_get_char(zip) % 6;
    // Bytes the next read takes before any length it decodes: 4 for an int/float or a string/data
    // length prefix, 3 for a color, 1 for a skip count.
    static const uint32_t need[6] = {4, 4, 3, 4, 4, 1};
    if (TCOD_zip_get_remaining_bytes(zip) < need[op]) break;
    switch (op) {
      case 0: sink += (size_t)TCOD_zip_get_int(zip); break;
      case 1: {
        // Keep the float's bits: casting a NaN/huge/negative float to an integer is UB.
        const float f = TCOD_zip_get_float(zip);
        uint32_t bits;
        memcpy(&bits, &f, sizeof bits);
        sink += bits;
        break;
      }
      case 2: sink += TCOD_zip_get_color(zip).r; break;
      case 3: {
        // The string starts right after its 4-byte length, so only the bytes that remain after that
        // length are known to be there; bound the scan to them instead of trusting a NUL.
        const size_t avail = TCOD_zip_get_remaining_bytes(zip) - 4;
        const char* s = TCOD_zip_get_string(zip);
        if (s) sink += strnlen(s, avail);
        break;
      }
      case 4: sink += (size_t)TCOD_zip_get_data(zip, (int)sizeof buf, buf); break;
      default: {
        // TCOD_zip_skip_bytes requires the skipped bytes to exist (TCOD_IFNOT asserts it).
        const uint8_t n = (uint8_t)TCOD_zip_get_char(zip);
        if (TCOD_zip_get_remaining_bytes(zip) < n) goto done;
        TCOD_zip_skip_bytes(zip, n);
        break;
      }
    }
  }
done:
  (void)sink;
  TCOD_zip_delete(zip);
  return 0;
}

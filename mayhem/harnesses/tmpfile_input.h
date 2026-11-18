// Writes one fuzz input, byte for byte, to a fresh mkstemps() file under $TMPDIR (default /tmp), for
// libtcod loaders that only take a path. The caller unlinks the returned path when it is done.
#ifndef MAYHEM_TMPFILE_INPUT_H
#define MAYHEM_TMPFILE_INPUT_H
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Returns 0 and fills `path` on success, -1 if the scratch file could not be written.
static int mayhem_write_tmp(const uint8_t* data, size_t size, const char* suffix, char* path, size_t path_size) {
  const char* dir = getenv("TMPDIR");
  if (!dir || !*dir) dir = "/tmp";
  if (snprintf(path, path_size, "%s/libtcod-XXXXXX%s", dir, suffix) >= (int)path_size) return -1;
  const int fd = mkstemps(path, (int)strlen(suffix));
  if (fd < 0) return -1;
  size_t done = 0;
  while (done < size) {
    const ssize_t n = write(fd, data + done, size - done);
    if (n <= 0) {
      close(fd);
      unlink(path);
      return -1;
    }
    done += (size_t)n;
  }
  close(fd);
  return 0;
}
#endif  // MAYHEM_TMPFILE_INPUT_H

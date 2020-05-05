#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <kver.h>

/* See kernel_version at https://www.kernel.org/doc/Documentation/x86/boot.txt
 * for more details on what this function is doing.
 */
int kver(const char * const path, char * const buf, const size_t len) {
  if (!path || !buf || !len) {
    errno = EINVAL;
    return -1;
  }

  FILE * const fp = fopen(path, "rb");

  if (!fp) {
    perror("fopen");
    return -1;
  }

  // Seek to the position of the 2-byte offset value
  int16_t offset;
  if (fseek(fp, 0x20E, SEEK_SET) < 0) {
    perror("fseek");
    goto KVER_ERROR;
  }

  // Read the offset value
  if (fread(&offset, sizeof(offset), 1, fp) != 1) {
    perror("fread");
    goto KVER_ERROR;
  }
  
  // Seek to the offset value + 0x200
  if (fseek(fp, 0x200 + offset, SEEK_SET) < 0) {
    perror("fseek");
    goto KVER_ERROR;
  }

  // Read the version string into the buffer
  if (fread(buf, len, 1, fp) != 1) {
    perror("fread");
    goto KVER_ERROR;
  }

  if (fclose(fp))
    perror("fclose");

  // Ensure that the version string terminates
  buf[len] = '\0';

  // Trim down to the semantic version (the first token before the space)
  if (!strtok(buf, " "))
    return -1;

  return 0;

KVER_ERROR:
  if (fclose(fp))
    perror("fclose");
  return -1;
}

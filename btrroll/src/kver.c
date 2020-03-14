#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <kver.h>

int kver(const char * const path, char * const buf, const size_t len) {
  FILE *fp;
  short offset;

  if ((fp = fopen(path, "rb")) == NULL)
    return -ENOENT;

  if (fseek(fp, 0x20E, SEEK_SET) < 0)
    return -errno;

  if (fread(&offset, sizeof(offset), 1, fp) != 1)
    return -1;

  if (fseek(fp, 0x200 + offset, SEEK_SET) < 0)
    return -errno;

  const int ret = fread(buf, len, 1, fp);
  fclose(fp);

  buf[len] = '\0';
  strtok(buf, " ");

  return ret == 1 ? 0 : -1;
}

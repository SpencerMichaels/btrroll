#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <cmdline.h>

#define CMDLINE_PATH "/proc/cmdline"

int cmdline_get(
    char * const buf,
    const size_t len)
{
  FILE *fp;

  if ((fp = fopen(CMDLINE_PATH, "rb")) == NULL)
    return -ENOENT;

  if (!fgets(buf, len, fp))
    return -1;

  // Remove the trailing newline
  const size_t n = strlen(buf);
  if (buf[n-1] == '\n')
    buf[n-1] = '\0';

  return 0;
}

char *cmdline_find_delim(
    char * const cmdline,
    const char * const key,
    char * const buf,
    const size_t len,
    char delim)
{
  const char delim_arr[] = { delim };
  const size_t cmdline_len = strlen(cmdline);

  const size_t key_len = strlen(key);
  char *token = strtok(cmdline, delim_arr);
  char *ret = NULL;

  do {
    if (!strncmp(token, key, key_len)) {
      char *value = token + key_len;
      const char next = *value;

      // skip if token is only a strict prefix
      if (next != delim && next != '\0' && next != '=' && next != '\n')
        continue;

      if (next == '=')
        value++;

      const size_t value_len = strlen(value);
      strncpy(buf, value, len);
      if (value - cmdline + value_len != cmdline_len) {
        value[value_len] = delim;
      }
      buf[len-1] = '\0';
      ret = token;
    }
    // Patch the string as we go along
    if (token > cmdline)
      *(token-1) = delim;
    if (ret)
      return ret;
  } while ((token = strtok(NULL, delim_arr)));

  return NULL;
}

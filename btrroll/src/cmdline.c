#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <cmdline.h>

#define CMDLINE_PATH "/proc/cmdline"

// Read the contents of the kernel command line into the given buffer
int cmdline_read(
    char * const buf,
    const size_t len)
{
  if (!buf || !len) {
    errno = EINVAL;
    return -1;
  }

  FILE * const fp = fopen(CMDLINE_PATH, "rb");
  if (!fp) {
    perror("fopen");
    return -1;
  }
  
  if (!fgets(buf, len, fp)) {
    perror("fgets");
    return -1;
  }

  // Remove the trailing newline
  const size_t n = strlen(buf);
  if (buf[n-1] == '\n')
    buf[n-1] = '\0';

  return 0;
}

// Look up a value by key in the kernel cmdline, with pairs separated by delim
char *cmdline_find_delim(
    char * const cmdline,
    const char * const key,
    char * const buf,
    const size_t len,
    char delim)
{
  if (!cmdline || !key || !buf || len == 0 || delim == '\0') {
    errno = EINVAL;
    return NULL;
  }
  errno = 0;

  const char delim_arr[] = { delim };
  const size_t cmdline_len = strlen(cmdline);

  const size_t key_len = strlen(key);
  char *token = strtok(cmdline, delim_arr);
  char *found = NULL;

  do {
    // If this token starts with the key we're looking for...
    if (!strncmp(token, key, key_len)) {
      char *value = token + key_len;
      const char next = *value++;

      // The key matches, but there is no associated value
      if (next == delim || next == '\n' || next == '\0') {
        buf[0] = '\0';
        found = token;
      }

      // The key matches, and there is a value
      else if (next == '=') {
        // Copy the value into the output buffer and ensure that it terminates
        strncpy(buf, value, len);
        buf[len-1] = '\0';

        // Restore the cmdline string by replacing the strtok-inserted null
        // byte (if present) with the original delimiter character
        const size_t value_len = strlen(value);
        if (value - cmdline + value_len != cmdline_len)
          value[value_len] = delim;

        found = token;
      }

      // The key is only a prefix of the token, not a real match. Skip it.
      else
        continue;
    }

    // Patch the original delimeter character back into the cmdline string
    if (token > cmdline)
      *(token-1) = delim;
    if (found)
      return found;
  } while ((token = strtok(NULL, delim_arr)));

  return NULL;
}

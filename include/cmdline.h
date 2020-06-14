#ifndef __CMDLINE_H__
#define __CMDLINE_H__

#include <stddef.h>

/* Read /proc/cmdline into a buffer. Returns 0 on success, -1 otherwise.
 */
int cmdline_read(
    char * const buf,
    const size_t len);

/* Find a key=value pair in a set of pairs separated by the delimiter `delim`.
 * Returns a pointer to the start of the key within the string if found; NULL
 * othewise. If there is a value, it will be copied into `buf`. If multiple
 * pairs with the same key are present, the last one will be returned.
 */
char *cmdline_find_delim(
    char * const cmdline,
    const char * const key,
    char * const buf,
    const size_t len,
    char delim);

// Same as the above, but assumes the delimiter to be a space character
static inline char *cmdline_find(
    char * const cmdline,
    const char * const key,
    char * const buf,
    const size_t len)
{
  return cmdline_find_delim(cmdline, key, buf, len, ' ');
}

#endif

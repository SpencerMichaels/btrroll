#ifndef __CMDLINE_H__
#define __CMDLINE_H__

#include <stddef.h>

int cmdline_get(
    char * const buf,
    const size_t len);

char *cmdline_find_delim(
    char * const cmdline,
    const char * const key,
    char * const buf,
    const size_t len,
    char delim);

static inline char *cmdline_find(
    char * const cmdline,
    const char * const key,
    char * const buf,
    const size_t len)
{
  return cmdline_find_delim(cmdline, key, buf, len, ' ');
}

#endif

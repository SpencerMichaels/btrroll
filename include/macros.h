#ifndef __MACROS_H__
#define __MACROS_H__

#include <stdbool.h>
#include <stdio.h>

#define lenof(arr) sizeof(arr)/sizeof(arr[0])

#define eprintf(...) \
  fprintf(stderr, __VA_ARGS__)

#define str_and_len(s) \
  s, strlen(s)

#define arr_and_size(a) \
  a, sizeof(a)

#define CLEANUP_DECLARE(ret) \
  __label__ CLEANUP; \
  int ret = 0;

#define FAIL(ret) \
  ret = -1; \
  goto CLEANUP;

#endif

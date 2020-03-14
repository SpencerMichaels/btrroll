#ifndef __MACROS_H__
#define __MACROS_H__

#include <stdio.h>

#define lenof(arr) sizeof(arr)/sizeof(arr[0])

#define eprintf(...) \
  fprintf(stderr, __VA_ARGS__)

#define str_and_len(s) \
  s, strlen(s)


#endif

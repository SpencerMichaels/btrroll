#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <path.h>

char *pathcat(const char *root, const char *child) {
  const size_t root_len = strlen(root);
  const int slash_root = root[root_len-1] == '/' ? 1 : 0;
  const int slash_child = child[0] == '/' ? 1 : 0;

  char * tmp = malloc(root_len + strlen(child) + 1
      + ((int)(!slash_root && !slash_child))
      - ((int)(slash_root && slash_child)));
  if (!tmp)
    return NULL;

  if (!slash_root && !slash_child) {
    sprintf(tmp, "%s/%s", root, child);
  } else {
    strcpy(tmp, root);
    strcat(tmp, child + ((int)(slash_root && slash_child)));
  }

  return tmp;
}

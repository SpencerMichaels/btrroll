#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/types.h>

#include <cmdline.h>
#include <macros.h>
#include <root.h>

// Get the root device and its mount flags from the kernel command line
int get_root(
    char * const root, const size_t root_len,
    char * const flags, const size_t flags_len)
{
  char cmdline[0x1000];

  if (cmdline_read(cmdline, sizeof(cmdline))
      || !cmdline_find(cmdline, "root", root, root_len)
      || !cmdline_find(cmdline, "rootflags", flags, flags_len))
    return -1;

  return 0;
}

// Mount the root device `root` at `mountpoint` with mount flags `flags`
int mount_root(
    const char * const mountpoint,
    const char * const fs_type,
    const char * const root,
    const char * const flags)
{
  if (!mountpoint || !fs_type || !root || !flags) {
    errno = EINVAL;
    return -1;
  }

  char buf[0x200];
  const char *path;
  if (root[0] == '/') {
    // The root is "/path/to/somewhere..."
    path = root;
  } else {
    // The root is "{LABEL,UUID,...}=value"
    char key[32];
    size_t i;
    for (i = 0; isalpha(root[i]) && i < sizeof(key); ++i)
      key[i] = tolower(root[i]);

    // The key must be an alphanumeric string followed by an `=`
    if (root[i] != '=') {
      errno = EINVAL;
      return -1;
    }
    key[i++] = '\0';

    // The key corresponds to a sub-path of /dev/disk/by-X/Y
    snprintf(buf, sizeof(buf), "/dev/disk/by-%s/%s", key, root + i);
    path = buf;
  }

  // TODO: If this happens twice, mounting will fail
  if (mount(path, mountpoint, fs_type, MS_NOATIME, flags)) {
    perror("mount");
    return -1;
  }

  return 0;
}

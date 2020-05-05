#include <btrfsutil.h>
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

int mount_root(
    const char * const source,
    const char * const mountpoint,
    const char * const flags)
{
  if (!source || !mountpoint) {
    errno = EINVAL;
    return -1;
  }

  char buf[0x200];
  const char *path;
  if (source[0] == '/') {
    // The source is "/path/to/somewhere..."
    path = source;
  } else {
    // The source is "{LABEL,UUID,...}=value"
    char key[32];
    size_t i;
    for (i = 0; isalpha(source[i]) && i < sizeof(key); ++i)
      key[i] = tolower(source[i]);

    // The key must be an alphanumeric string followed by an `=`
    if (source[i] != '=') {
      errno = EINVAL;
      return -1;
    }
    key[i++] = '\0';

    // The key corresponds to a sub-path of /dev/disk/by-X/Y
    snprintf(buf, sizeof(buf), "/dev/disk/by-%s/%s", key, source + i);
    path = buf;
  }

  const char * const flags_ = flags ? flags : "";
  fprintf(stderr, "%s\n%s\n%s\n", path, mountpoint, flags_);
  if (mount(path, mountpoint, "btrfs", MS_NOATIME, flags_))
    return -1;

  return 0;
}

/* Utility function. If `str` begins with `target`, return a pointer to the
 * next position in `str` past `target`, otherwise NULL.
 */
static inline char * match_next(
    char * const str,
    const char * const target,
    const size_t len)
{
  if (!strncmp(str, target, len))
    return str + len;
  return NULL;
}

char * get_btrfs_root_subvol_path(
    const char * const mountpoint,
    char * flags)
{
  if (!mountpoint) {
    errno = EINVAL;
    return NULL;
  }

  if (!flags)
    flags = "";

  static const size_t PATH_LEN = 0x1000;
  const size_t flags_len = strlen(flags);

  uint64_t id = 0; // Note: 0 is not a valid subvolume ID in BTRFS
  char * path = NULL;
  char * token = strtok(flags, ",");
  
  // Search through the cmdline keys for a subvolume specifier
  // If multiple keys exist, take the last one
  do {
    char *value;
    if ((value = match_next(token, str_and_len("subvol=")))) {
      // Value formatted as /path/to/subvol
      path = value;
    } else if ((value = match_next(token, str_and_len("subvolid=")))) {
      // Value formatted as an integer ID
      char *endptr;
      id = strtoull(value, &endptr, 10);
      if (!*token || *endptr)
        id = 0;
    }
  } while ((token = strtok(NULL, ",")) != NULL);

  if (path) {
    // The path was explicitly specified with subvol=X, just return the path
    const size_t len = strlen(path);
    char *tmp = malloc(len+1);
    memcpy(tmp, path, len+1);
    return tmp;
  }

  // The path was explicitly specified with subvolid=X; look up the path
  if (id) {
    if (btrfs_util_subvolume_path(mountpoint, id, &path) != BTRFS_UTIL_OK)
      return NULL;
    return path;
  }

  // The path was not specified; look up the mountpoint's default subvolume
  if (btrfs_util_get_default_subvolume(mountpoint, &id) != BTRFS_UTIL_OK)
    return NULL;
  return path;
}

#include <btrfsutil.h>
#include <errno.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <constants.h>
#include <macros.h>

/* Utility function. If `str` begins with `target`, return a pointer to the
 * next position in `str` past `target`, otherwise NULL.
 */
static inline char * match_advance(
    char * const str,
    const char * const target,
    const size_t len)
{
  if (!strncmp(str, target, len))
    return str + len;
  return NULL;
}

// Get the BTRFS root subvolume path from the root flags or partition defaults
char * get_btrfs_root_subvol_path(
    const char * const mountpoint,
    char *flags)
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
    if ((value = match_advance(token, str_and_len("subvol=")))) {
      // Value formatted as /path/to/subvol
      path = value;
    } else if ((value = match_advance(token, str_and_len("subvolid=")))) {
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
    enum btrfs_util_error err = btrfs_util_subvolume_path(mountpoint, id, &path);
    if (err != BTRFS_UTIL_OK) {
      eprintf("error: %s\n", btrfs_util_strerror(err));
      return NULL;
    }
    return path;
  }

  // The path was not specified; look up the mountpoint's default subvolume
  enum btrfs_util_error err = btrfs_util_get_default_subvolume(mountpoint, &id);
  if (err != BTRFS_UTIL_OK) {
    eprintf("error: %s\n", btrfs_util_strerror(err));
    return NULL;
  }
  return path;
}

char * get_subvol_dir_path(char *subvol_path) {
  char *tmp = malloc(strlen(subvol_path) + strlen(SUBVOL_DIR_SUFFIX) + 1);
  if (!tmp)
    return NULL;

  strcpy(tmp, subvol_path);
  strcat(tmp, SUBVOL_DIR_SUFFIX);

  return tmp;
}

int is_subvol_toplevel(char *path) {
  uint64_t id;
  enum btrfs_util_error err = btrfs_util_subvolume_id(path, &id);
  if (err != BTRFS_UTIL_OK) {
    eprintf("error: %s\n", btrfs_util_strerror(err));
    return -1;
  }
  return id == 5;
}

int is_subvol_provisioned(char *path) {
  // Check if the root subvol is already set up for use with btrroll
  struct stat info;
  if (lstat(path, &info)) {
    perror("lstat");
    return -1;
  }

  // TODO: May want a stricter check than ISLNK
  if (!S_ISLNK(info.st_mode))
    return 0;
  return 1;
}

int provision_subvol(char *path) {
  CLEANUP_DECLARE(ret);

  // Allocate scratch space for paths
  char *tmp = malloc(strlen(path) + strlen(SUBVOL_DIR_SUFFIX) +
        strlen(SUBVOL_CUR_NAME) + strlen(SUBVOL_SNAP_NAME) + 2);

  // Create subvol.d
  sprintf(tmp, "%s" SUBVOL_DIR_SUFFIX, path);
  if (mkdir(tmp, 0700)) {
    perror("mkdir");
    FAIL(ret);
  }

  // Move subvol to /.../subvol.d/current (absolute)
  strcat(tmp, "/" SUBVOL_CUR_NAME);
  if (rename(path, tmp)) {
    perror("rename");
    FAIL(ret);
  }

  // Symlink subvol -> subvol.d/current (relative)
  sprintf(tmp, "%s" SUBVOL_DIR_SUFFIX "/" SUBVOL_CUR_NAME, basename(path));
  if (symlink(tmp, path)) {
    perror("symlink");
    FAIL(ret);
  }

  // Create subvol.d/snapshots
  sprintf(tmp, "%s" SUBVOL_DIR_SUFFIX "/" SUBVOL_SNAP_NAME, path);
  if (mkdir(tmp, 0700)) {
    perror("mkdir");
    FAIL(ret);
  }

CLEANUP:
  free(tmp);
  return ret;
}

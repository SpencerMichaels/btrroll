#include <btrfsutil.h>
#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mount.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <cmdline.h>
#include <dialog/cli.h>
#include <dialog/tui.h>
#include <macros.h>
#include <run.h>

#define INITRD_RELEASE_PATH "/etc/initrd-release"

void main_menu();
void snapshot_menu();

int main(int argc, char **argv) {
  /*
  if (!access(INITRD_RELEASE_PATH, F_OK)) {
    eprintf("btrroll: error: could not find " INITRD_RELEASE_PATH "! " \
            "btrroll must be run from within the initial ramdisk.\n");
    return EXIT_FAILURE;
  }
  */

  dialog_backend_t dialog;
  if (dialog_tui_available())
    dialog_tui_init(&dialog);
  else
    dialog_cli_init(&dialog);

  main_menu(&dialog);
  
  dialog_free(&dialog);

  return EXIT_SUCCESS;
}

void main_menu(dialog_backend_t *dialog) {
  static const char *ITEMS[] = {
    "Manage snapshots",
    "Launch a shell",
    "Reboot",
    "Shutdown",
    "Exit (continue booting)"
  };

  int choice = 0;
  while (true) {
    choice = dialog_choose(dialog,
        ITEMS, lenof(ITEMS), choice,
        "Main Menu", "What would you like to do?");

    switch (choice) {
      case 0:
        snapshot_menu(dialog);
        break;
      case 1:
        dialog_clear(dialog);
        run("sh", NULL);
        break;
      case 2:
        // TODO
        break;
      case 3:
        // TODO
        break;
      case 4:
      case DIALOG_RESPONSE_CANCEL:
        return;
      default:
        dialog_ok(dialog, "Error", "Encountered an error."); // TODO
        return;
    }
  }
}

int mount_root(
    const char * const source,
    const char * const mountpoint,
    const char * const flags)
{
  char by[16];
  const char * target;
  {
    size_t i;
    for (i = 0; isalpha(source[i]); ++i) {
      by[i] = tolower(source[i]);
    }
    if (source[i] != '=')
      return -EINVAL;
    by[i++] = '\0';

    target = source + i;
  }

  char path[0x200];
  snprintf(path, sizeof(path), "/dev/disk/by-%s/%s", by, target);

  if (mount(path, mountpoint, "btrfs", MS_NOATIME, flags))
    return -errno;

  return 0;
}

#define MOUNTPOINT_TEMPLATE "/btrfs_root-XXXXXX"

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

char * match_next(
    char * const str,
    const char * const target,
    const size_t len)
{
  if (!strncmp(str, target, len))
    return str + len;
  return NULL;
}

char * get_root_subvol_path(
    const char * const mountpoint,
    const char * const flags)
{
  static const size_t PATH_LEN = 0x1000;
  const size_t flags_len = strlen(flags);

  uint64_t id;
  char *token, *path;
  char * flags_tmp;
    
  flags_tmp = malloc(flags_len+1);
  memcpy(flags_tmp, flags, flags_len+1);

  id = 0; // not a valid subvolume ID in BTRFS
  path = NULL;
  token = strtok(flags_tmp, ",");
  do {
    char *value;
    if ((value = match_next(token, str_and_len("subvol=")))) {
      path = value;
    } else if ((value = match_next(token, str_and_len("subvolid=")))) {
      char *endptr;
      id = strtoull(value, &endptr, 10);
      if (!*token || *endptr)
        id = 0; // invalid ID string
    }
  } while ((token = strtok(NULL, ",")) != NULL);

  if (path) {
    const size_t len = strlen(path);
    char *tmp = malloc(len+1);
    memcpy(tmp, path, len+1);
    path = tmp;
  } else {
    if (!id)
      if (btrfs_util_get_default_subvolume(mountpoint, &id) != BTRFS_UTIL_OK)
        return NULL;

    if (btrfs_util_subvolume_path(mountpoint, id, &path) != BTRFS_UTIL_OK)
      return NULL;
  }

  free(flags_tmp);
  return path;
}

void snapshot_menu(dialog_backend_t *dialog) {
  char root[0x1000], flags[0x1000];

  if (get_root(root, sizeof(root), flags, sizeof(flags))) {
    dialog_ok(dialog, "Error",
        "Failed to get root device info from /proc/cmdline: %s",
        strerror(errno));
    return;
  }

  char mountpoint[] = MOUNTPOINT_TEMPLATE;
  if (!mkdtemp(mountpoint)) {
    dialog_ok(dialog, "Error",
        "Failed to create a temporary directory for mounting: %s",
        strerror(errno));
    return;
  }

  // TODO: sanitize flags, deleting subvol{,id} but retaining others
  if (mount_root(root, mountpoint, "")) {
    dialog_ok(dialog, "Error",
        "Failed to mount `%s` to `%s`: %s",
        root, mountpoint, strerror(errno));
    return;
  }

  // TODO: this could be nicer
  char *subvol_path_rel = get_root_subvol_path(mountpoint, flags);
  char *subvol_path_abs = malloc(strlen(mountpoint) + strlen(subvol_path_rel) + 2);
  char *subvol_path_abs_new = malloc(strlen(mountpoint) + strlen(subvol_path_rel) + 4);
  sprintf(subvol_path_abs, "%s%s%s", mountpoint,
          subvol_path_rel[0] == '/' ? "" : "/" , subvol_path_rel);
  sprintf(subvol_path_abs_new, "%s.d/current", subvol_path_abs);
  free(subvol_path_rel);

  struct stat info;
  if (lstat(subvol_path_abs, &info)) {
    dialog_ok(dialog, "Error",
        "Failed to stat subvol path: %s",
        strerror(errno));
    return;
  }

  if (!S_ISLNK(info.st_mode)) {
    if (dialog_confirm(dialog, 0, "Error",
        "Root subvolume is not a symlink. Would you like to provision it?")
          == DIALOG_RESPONSE_YES)
    {
      if (rename(subvol_path_abs, subvol_path_abs_new) < 0) {
        dialog_ok(dialog, "Error",
            "Failed to rename `%s` to `%s`: %s",
            subvol_path_abs, subvol_path_abs_new, strerror(errno));
        return;
      }

      if (symlink(subvol_path_abs_new, subvol_path_abs) < 0) {
        dialog_ok(dialog, "Error",
            "Failed to symlink `%s` to `%s`: %s",
            subvol_path_abs_new, subvol_path_abs, strerror(errno));
        // TODO: revert if failed
        return;
      }

      dialog_ok(dialog, "Success", "Created .d and symlink.");
    }
    return;
  }

  dialog_ok(dialog, "Incomplete", "Incomplete");
}

#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <dialog/cli.h>
#include <dialog/tui.h>
#include <macros.h>
#include <root.h>
#include <run.h>

#define INITRD_RELEASE_PATH "/etc/initrd-release"

void main_menu();
void snapshot_menu();
int provision_subvol(char *path);

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

#define MOUNTPOINT_TEMPLATE "/btrfs_root-XXXXXX"

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
  char *subvol_path_rel = get_btrfs_root_subvol_path(mountpoint, flags);
  char *subvol_path_abs = malloc(strlen(mountpoint) + strlen(subvol_path_rel) + 2);
  char *subvol_path_abs_new = malloc(strlen(mountpoint) + strlen(subvol_path_rel) + 11);
  sprintf(subvol_path_abs, "%s%s%s", mountpoint,
          subvol_path_rel[0] == '/' ? "" : "/" , subvol_path_rel);

  free(subvol_path_rel);

  struct stat info;
  if (lstat(subvol_path_abs, &info)) {
    dialog_ok(dialog, "Error",
        "Failed to stat subvol path: %s",
        strerror(errno));
    return;
  }

  // TODO: Need a stricter check for this
  if (!S_ISLNK(info.st_mode) &&
        dialog_confirm(dialog, 0, "Error",
          "Root subvolume is not a symlink. Would you like to provision it for "
          "use with btrroll?") == DIALOG_RESPONSE_YES && 
        provision_subvol(subvol_path_abs))
      dialog_ok(dialog, "Error", "Failed provision: %s", strerror(errno));

  dialog_ok(dialog, "Incomplete", "Incomplete");
}

#define SUBVOL_DIR_SUFFIX ".d"
#define SUBVOL_CUR_NAME "current"

int provision_subvol(char *path) {
  const size_t path_new_len = strlen(path) + strlen(SUBVOL_DIR_SUFFIX) +
      strlen(SUBVOL_CUR_NAME) + 2;
  char *path_new = malloc(path_new_len);

  strcpy(path_new, path);
  strcat(path_new, SUBVOL_DIR_SUFFIX);

  // Create subvol.d
  if (mkdir(path_new, 0700)) {
    perror("mkdir");
    return -1;
  }

  strcat(path_new, "/" SUBVOL_CUR_NAME);

  // Move subvol to subvol.d/current
  if (rename(path, path_new)) {
    perror("rename");
    return -1;
  }

  // Symlink subvol -> subvol.d/current
  if (symlink(path_new, path)) {
    perror("symlink");
    return -1;
  }

  return 0;
}

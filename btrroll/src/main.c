#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <libgen.h>
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

#define BTRFS_MOUNTPOINT "/btrfs_root"

char *mount_root_subvol(dialog_backend_t *dialog) {
  char root[0x1000], flags[0x1000];

  if (get_root(root, sizeof(root), flags, sizeof(flags))) {
    dialog_ok(dialog, "Error",
        "get_root failed: %s",
        strerror(errno));
    return NULL;
  }

  if (mkdir(BTRFS_MOUNTPOINT, 0700)) {
    perror("mkdir");
    return NULL;
  }

  if (mount_root(root, BTRFS_MOUNTPOINT, "")) { // TODO: flags -subvol{,id}
    dialog_ok(dialog, "Error",
        "mount_root with `%s` failed: %s",
        root,
        strerror(errno));
    return NULL; // TODO: unmount root?
  }

  char *subvol_path_rel = get_btrfs_root_subvol_path(BTRFS_MOUNTPOINT, flags);
  if (!subvol_path_rel) {
    dialog_ok(dialog, "Error",
        "get_btrfs_root failed: %s",
        strerror(errno));
    return NULL;
  }

  // Compute the absolute path relative to the initramfs root
  char *subvol_path = malloc(strlen(BTRFS_MOUNTPOINT) + strlen(subvol_path_rel) + 2);
  sprintf(subvol_path, "%s%s%s", BTRFS_MOUNTPOINT,
          subvol_path_rel[0] == '/' ? "" : "/" , subvol_path_rel);
  free(subvol_path_rel);

  return subvol_path;
}

#define SUBVOL_DIR_SUFFIX ".d"
#define SUBVOL_CUR_NAME "current"
#define SUBVOL_SNAP_NAME "snapshots"

void snapshot_menu(dialog_backend_t *dialog) {
  // Get the root subvolume path relative to the BTRFS partition root
  char *subvol_path = mount_root_subvol(dialog);
  if (!subvol_path) {
    dialog_ok(dialog, "Error",
        "Failed to mount root subvolume: %s",
        strerror(errno));
    return;
  }

  // Check if the root subvol is already set up for use with btrroll
  struct stat info;
  if (lstat(subvol_path, &info)) {
    dialog_ok(dialog, "Error",
        "Failed to stat subvol path: %s",
        strerror(errno));
    return;
  }

  // TODO: May want a stricter check than ISLNK
  if (!S_ISLNK(info.st_mode) &&
        dialog_confirm(dialog, 0, "Error",
          "Root subvolume is not a symlink. Would you like to provision it for "
          "use with btrroll?") == DIALOG_RESPONSE_YES && 
        provision_subvol(subvol_path))
      dialog_ok(dialog, "Error", "Failed provision: %s", strerror(errno));

  const size_t snapshots_path_len = strlen(subvol_path) + strlen(SUBVOL_DIR_SUFFIX) +
      strlen(SUBVOL_SNAP_NAME) + 2;
  char * const snapshots_path = malloc(snapshots_path_len);
  sprintf(snapshots_path, "%s" SUBVOL_DIR_SUFFIX "/" SUBVOL_SNAP_NAME, subvol_path);

  DIR * const snapshots_dir = opendir(snapshots_path);
  if (!snapshots_dir) {
    // TODO: no snapshots dir/subvol
    dialog_ok(dialog, "Error", "Failed to open `%s`: %s", snapshots_path, strerror(errno));
    return;
  }

  // TODO: mem size limit
  char **snapshots = malloc(0x100 * sizeof(char*));
  size_t snapshots_len;

  {
    struct dirent *ep;
    char **p = snapshots;
    errno = 0;
    while (ep = readdir(snapshots_dir))
      if (ep->d_type == DT_DIR && ep->d_name[0] != '.')
        *p++ = strcpy(malloc(strlen(ep->d_name)+1), ep->d_name);
    snapshots_len = p - snapshots;

    if (errno) {
      // TODO: dir read error
      return;
    }

    closedir(snapshots_dir); // TODO: error check
  }

  int choice = 0;
  while (true) {
    dialog_ok(dialog, "Choice", "Choice is %d", choice);
    choice = dialog_choose(dialog,
        (const char**)snapshots, snapshots_len, choice,
        "Snapshots", "Select a snapshot from the list below.");

    if (choice == DIALOG_RESPONSE_CANCEL)
      return;
    else if (choice > snapshots_len)
      return; // TODO: error

    const char * const snapshot_name = snapshots[choice];
    char * const snapshot_path = malloc(snapshots_path_len + strlen(snapshot_name) + 2);
    sprintf(snapshot_path, "%s/%s", snapshots_path, snapshot_name);

    dialog_ok(dialog, "Selected", "Selected %s", snapshot_path);
  }
}

int provision_subvol(char *path) {
  char * tmp = malloc(strlen(path) + strlen(SUBVOL_DIR_SUFFIX) +
      strlen(SUBVOL_CUR_NAME) + 2);

  // Create subvol.d
  sprintf(tmp, "%s" SUBVOL_DIR_SUFFIX, path);
  if (mkdir(tmp, 0700)) {
    perror("mkdir");
    return -1;
  }

  // Move subvol to /.../subvol.d/current (absolute)
  strcat(tmp, "/" SUBVOL_CUR_NAME);
  if (rename(path, tmp)) {
    perror("rename");
    return -1;
  }

  // Symlink subvol -> subvol.d/current (relative)
  sprintf(tmp, "%s" SUBVOL_DIR_SUFFIX "/" SUBVOL_CUR_NAME, basename(path));
  if (symlink(tmp, path)) {
    perror("symlink");
    return -1;
  }

  return 0;
}

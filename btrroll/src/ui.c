#include <btrfsutil.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <libgen.h>
#include <linux/magic.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/vfs.h>

#include <boot.h>
#include <constants.h>
#include <dialog.h>
#include <macros.h>
#include <path.h>
#include <root.h>
#include <run.h>
#include <snapshot.h>
#include <subvol.h>
#include <ui.h>

int main_menu(dialog_t *dialog, char *root_subvol) {
  __label__ CLEANUP;
  int ret = 0;

  static const char *ITEMS[] = {
    "Boot/restore a snapshot",
    "Launch a shell",
    "Reboot",
    "Shutdown",
    // "Exit (continue booting)"
  };

  int is_provisioned = is_subvol_provisioned(root_subvol);
  char * root_subvol_dir = get_subvol_dir_path(root_subvol);

  if (is_provisioned < 0) {
    dialog_ok(dialog, "Error", "Could not determine whether or not the root " \
        "subvolume `%s` is provisioned: %s", root_subvol, strerror(errno));
    is_provisioned = 0;
  }

  size_t choice = 0;
  while (true) {
    ret = dialog_choose(dialog,
        ITEMS, lenof(ITEMS), &choice,
        "Main Menu", "What would you like to do?");

    dialog_reset(dialog);

    if (ret == DIALOG_RESPONSE_CANCEL) {
      ret = 0;
      goto CLEANUP;
    }

    switch (choice) {
      case 0: // Boot or restore from a snapshot
        if (!is_provisioned) {
          const int answer = dialog_confirm(dialog, 0,
              "Error",
              "The root subvolume is not provisioned for use with btrroll. " \
              "Would you like to provision it?");

          if (answer != DIALOG_RESPONSE_YES)
            break;

          if (provision_subvol(root_subvol)) {
            dialog_ok(dialog, "Error", "Failed to provision: %s", strerror(errno));
          } else {
            is_provisioned = 1;
            break;
          }
        }
        if (chdir(root_subvol_dir)) {
          dialog_ok(dialog, "Error", "Failed to chdir to `%s`: %s",
              root_subvol_dir, strerror(errno));
        } else {
          snapshot_menu(dialog, root_subvol_dir);
          chdir("/");
        }
        break;
      case 1: // Launch a shell
        dialog_clear(dialog);
        run("sh", NULL);
        break;
      case 2: // Reboot
        restart();
        ret = 0;
        goto CLEANUP;
      case 3: // Shut down
        shutdown();
        ret = 0;
        goto CLEANUP;
      default:
        goto CLEANUP;
    }
  }

CLEANUP:
  free(root_subvol_dir);
  return ret;
}

void snapshot_menu(dialog_t *dialog, char *root_subvol_dir) {
  // Change to the "snapshots" directory. This is much easier than staying in
  // place and constructing relative paths for each snapshot.
  if (chdir(SUBVOL_SNAP_NAME)) {
    if (errno != ENOENT) {
      dialog_ok(dialog, "Error",
          "Could not chdir to `" SUBVOL_SNAP_NAME "`: %s", strerror(errno));
      return;
    }
    return;
  }

  DIR * const snapshots_dir = opendir(".");
  if (!snapshots_dir) {
    dialog_ok(dialog, "Error", "Failed to open `.`: %s", strerror(errno));
    return;
  }

  const size_t SNAPSHOTS_LEN_MAX = 0x400;
  char *snapshots[SNAPSHOTS_LEN_MAX];
  {
    struct dirent *ep;
    char ** const snapshots_end = snapshots + SNAPSHOTS_LEN_MAX;
    char **p = snapshots;
    errno = 0;
    while ((ep = readdir(snapshots_dir)) && p != snapshots_end) {
      if (errno) {
        dialog_ok(dialog, "Error (readdir)",
            "Failed to read snapshots directory: %s", strerror(errno));
        break;
      }
      errno = 0;

      // Record the names of all applicable directories in snapshots/
      if (ep->d_type == DT_DIR &&                               // Is a directory
          ep->d_name[0] != '.' &&                               // Is not hidden
          btrfs_util_is_subvolume(ep->d_name) == BTRFS_UTIL_OK) // Is a subvol
        *p++ = strcpy(malloc(strlen(ep->d_name)+1), ep->d_name);
    }
    *p = NULL;

    if (closedir(snapshots_dir))
      perror("closedir");
  }

  if (*snapshots == NULL) {
    dialog_ok(dialog, "Snapshots", "There are no snapshots to display.");
    return;
  }

  size_t choice = 0;
  while (true) {
    int ret = dialog_choose(dialog,
        (const char**)snapshots, 0, &choice,
        "Snapshots", "Select a snapshot from the list below.");

    if (ret == DIALOG_RESPONSE_CANCEL || ret < 0)
      break;

    // This function repurposes the extra/help buttons as boot/restore
    char * snapshot = snapshots[choice];
    ret = snapshot_detail_menu(dialog, snapshot);

    if (ret == DIALOG_RESPONSE_EXTRA) {
      if (snapshot_boot(root_subvol_dir, snapshot))
        dialog_ok(dialog, "Error", "Failed to set up snapshot for booting: %s", strerror(errno));
      break;
    } else if (ret == DIALOG_RESPONSE_HELP) {
      snapshot_restore(root_subvol_dir, snapshot);
      break;
    }
  }

  for (char **p = snapshots; *p; p++)
    free(*p);

  // Return to the parent directory
  if (chdir(".."))
    perror("chdir");
}

int snapshot_detail_menu(dialog_t *dialog, char *snapshot) {
  char *info_file_path = pathcat(snapshot, INFO_FILE);

  dialog->buttons.extra = true;
  dialog->buttons.help = true;

  dialog->labels.ok = "Cancel";
  dialog->labels.extra = "Boot";
  dialog->labels.help = "Restore";

  int ret;
  if (access(info_file_path, R_OK))
    ret = dialog_ok(dialog, snapshot, "This snapshot has no info file.");
  else
    ret = dialog_view_file(dialog, snapshot, info_file_path);

  dialog_reset(dialog);
  free(info_file_path);

  return ret;
}

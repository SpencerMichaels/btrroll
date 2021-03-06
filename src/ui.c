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
#include <time.h>

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
    "Boot/restore from a snapshot",
    "Launch a shell",
    "Reboot",
    "Shutdown",
    // "Exit (continue booting)"
  };

  int is_provisioned = root_subvol ? is_subvol_provisioned(root_subvol) : 0;
  int is_toplevel = root_subvol ? is_subvol_toplevel(root_subvol) : 0;
  char * root_subvol_dir = root_subvol ? get_subvol_dir_path(root_subvol) : NULL;

  if (is_provisioned < 0) {
    dialog_ok(dialog, "Error", "Could not determine whether or not the root " \
        "subvolume `%s` is provisioned: %s", root_subvol, strerror(errno));
    is_provisioned = 0; // assume the worst case
  }
  if (is_toplevel < 0) {
    dialog_ok(dialog, "Error", "Could not determine whether or not the root " \
        "subvolume `%s` is top-level: %s", root_subvol, strerror(errno));
    is_toplevel = 1; // assume the worst case
  }

  size_t choice = 0;
  while (true) {
    ret = dialog_choose(dialog,
        ITEMS, NULL, lenof(ITEMS), &choice,
        "Main Menu", "What would you like to do?");

    dialog_reset(dialog);

    if (ret == DIALOG_RESPONSE_CANCEL) {
      ret = 0;
      goto CLEANUP;
    }

    switch (choice) {
      case 0: // Boot or restore from a snapshot
        if (!is_provisioned) {
          if (!root_subvol) {
            dialog_ok(dialog, "Error", "The root subvolume is not mounted.");
            break;
          }
          if (is_toplevel) {
            dialog_ok(dialog, "Error", "The system root resides directly on "
                "the top-level subvolume. btrroll's rollback functionality is "
                "not compatible with this configuration.");
            break;
          }

          int answer = dialog_confirm(dialog, 0,
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
    if (errno == ENOENT) {
      dialog_ok(dialog, "Warning",
          "There is no " SUBVOL_SNAP_NAME " directory in `%s`. Add one and "
          "populate it with snapshots in order to use this functionality.");
    } else {
      dialog_ok(dialog, "Error",
          "Failed to chdir to `" SUBVOL_SNAP_NAME "`: %s", strerror(errno));
    }
    return;
  }

  DIR * const snapshots_dir = opendir(".");
  if (!snapshots_dir) {
    dialog_ok(dialog, "Error", "Failed to open `.`: %s", strerror(errno));
    return;
  }

  // Collect a list of snapshots and their descriptions.
  char *snapshots[0x400];
  char *descriptions[0x100];
  {
    struct dirent *ep;
    char ** const snapshots_end = snapshots + lenof(snapshots) - 1;
    char **p = snapshots, **q = descriptions;
    errno = 0;
    while ((ep = readdir(snapshots_dir)) && p != snapshots_end) {
      if (errno) {
        dialog_ok(dialog, "Error (readdir)",
            "Failed to read snapshots directory: %s", strerror(errno));
        break;
      }

      // Record the names of all applicable directories in snapshots/
      if (ep->d_type == DT_DIR &&                               // Is a directory
          ep->d_name[0] != '.' &&                               // Is not hidden
          btrfs_util_is_subvolume(ep->d_name) == BTRFS_UTIL_OK) // Is a subvol
      {
        // Note the snapshot name
        char *snapshot = ep->d_name;
        *p++ = strdup(snapshot);

        // Get the last-modified time for the snapshot
        struct stat sb;
        char mtime[0x100];
        if (stat(snapshot, &sb)) {
          perror("stat");
          strcpy(mtime, "unknown");
        } else {
          strftime(mtime, sizeof(mtime), "%c", localtime(&sb.st_mtime));
        }

        // Get the kernel versions supported by the snapshot
        char *versions[0x100];
        char versions_str[0x1000];
        int num_versions = get_kernel_versions(snapshot, versions, lenof(versions));
        if (num_versions < 0) {
          snprintf(versions_str, sizeof(versions_str), "unknown (%s)", strerror(errno));
        } else {
          char *dest = versions_str;
          for (int i = 0; i < num_versions; ++i) {
            char *v = versions[i];
            dest += snprintf(dest,
                sizeof(versions_str) - (dest - versions_str),
                i == (num_versions-1) ? "%s" : "%s, ", v);
            free(v);
          }
        }

        // Compile the final description
        static const size_t DESC_LEN = 0x1000;
        char *desc = malloc(DESC_LEN);
        snprintf(desc, DESC_LEN, "Last modified: %s. Kernel version(s): %s.",
            mtime, versions_str);

        *q++ = desc;
      }

      errno = 0;
    }
    *p = NULL;

    if (closedir(snapshots_dir))
      perror("closedir");
  }

  if (*snapshots == NULL) {
    dialog_ok(dialog, "Snapshots", "There are no snapshots to display.");
    return;
  }

  // Allow the user to choose between the collated snapshots
  size_t choice = 0;
  while (true) {
    int ret = dialog_choose(dialog,
        (const char**)snapshots, (const char**)descriptions, 0, &choice,
        "Snapshots", "Select a snapshot from the list below.");

    if (ret == DIALOG_RESPONSE_CANCEL || ret < 0)
      break;

    // This function repurposes the extra/help buttons as boot/restore
    char *snapshot = snapshots[choice];
    ret = snapshot_detail_menu(dialog, snapshot);

    static const char *esp_path = "/esp"; // TODO

    // `Boot` selected
    if (ret == DIALOG_RESPONSE_EXTRA) {
      char *boot_entry = boot_entry_menu(dialog, snapshot, esp_path);
      if (!boot_entry || bootctl_set_oneshot(esp_path, boot_entry) && errno)
        dialog_ok(dialog, "Error", "Failed to set oneshot boot entry: %s", strerror(errno));
      //else
      //  if (snapshot_boot(root_subvol_dir, snapshot))
      //    dialog_ok(dialog, "Error", "Failed to set up snapshot for booting: %s", strerror(errno));
      free(boot_entry);
      break;
    }

    // `Restore` selected
    else if (ret == DIALOG_RESPONSE_HELP) {
      char *backup = NULL;

      // Offer to back up the current root subvol
      bool cancelled = false;
      if (dialog_confirm(dialog, 0, "Restore",
          "Would you like to save a snapshot of the current root subvolume?")
          == DIALOG_RESPONSE_YES)
      {
        static const size_t BACKUP_LEN = 0x400;
        backup = malloc(BACKUP_LEN);

        // Allow the user to set a name
        while (true) {
          snprintf(backup, BACKUP_LEN, "%s.pre-restore", snapshot);
          if (dialog_input(dialog, backup, backup, BACKUP_LEN, "Backup Name",
                "What would you like to name the backup?") != DIALOG_RESPONSE_OK)
          {
            backup = NULL;
            cancelled = true; // don't continue with the restore process
            break;
          }

          // Validate the snapshot name
          if (strchr(backup, '/'))
            dialog_ok(dialog, "Error", "The backup name cannot contain slashes.", backup);
          else if (backup[0] == '.')
            dialog_ok(dialog, "Error", "The snapshot name cannot start with a dot.", backup);
          else if (!access(backup, R_OK))
            dialog_ok(dialog, "Error", "A snapshot already exists with the name `%s`. "
                "Please choose a different name.", backup);
          else
            break;
        }
      }

      if (!cancelled) {
        char *boot_entry = boot_entry_menu(dialog, snapshot, esp_path);
        if (!boot_entry || !bootctl_set_default(esp_path, boot_entry) && errno)
          dialog_ok(dialog, "Error", "Failed to set default boot entry: %s", strerror(errno));
        else
          snapshot_restore(root_subvol_dir, snapshot, backup);
        free(boot_entry);
      }

      free(backup);
      break;
    }
  }

  for (char **p = snapshots; *p; p++)
    free(*p);
  for (char **p = descriptions; *p; p++)
    free(*p);

  // Return to the parent directory
  if (chdir(".."))
    perror("chdir");
}

int snapshot_detail_menu(dialog_t *dialog, const char *snapshot) {
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

char * boot_entry_menu(dialog_t *dialog, const char *snapshot, const char *esp_path) {
  __label__ CLEANUP;
  char *ret = NULL;

  bootctl_entry_t entries[32];
  int num_entries = get_compatible_boot_entries(snapshot, esp_path, entries, lenof(entries));
  if (num_entries < 0) {
    perror("get_compatible_boot_entries");
    goto CLEANUP;
  }
  else if (num_entries == 0) {
    dialog_ok(dialog, "Error", "There are no boot entries compatible with this snapshot.");
    goto CLEANUP;
  }
  else if (num_entries == 1) {
    // Only one compatible entry; no need to select
    ret = strdup(entries[0].id);
    goto CLEANUP;
  }

  // TODO: descs for .efi and .conf files (process will be different)
  // Possibly offer an "Info" button to view .confs directly
  char *items[sizeof(entries)];
  char *descs[sizeof(entries)];
  for (int i = 0; i < num_entries; ++i) {
    bootctl_entry_t *e = entries + i;
    items[i] = strdup(e->id ? e->id : "");
    descs[i] = strdup(e->options ? e->options : "");
  }

  size_t choice = 0;
  int err = dialog_choose(dialog, (const char**)items, (const char**)descs,
        num_entries, &choice, "Choose a Boot Entry",
        "Multiple available boot entries are compatible with this snapshot. "
        "Please choose one from the list below.");

  for (int i = 0; i < num_entries; ++i) {
    free(items[i]);
    free(descs[i]);
  }

  if  (err == DIALOG_RESPONSE_OK) {
    ret = strdup(entries[choice].id);
  }
  else if (err == DIALOG_RESPONSE_CANCEL) {
    errno = 0;
  }

CLEANUP:
  for (int i = 0; i < num_entries; ++i) {
    bootctl_entry_free(entries + i);
  }

  return ret;
}

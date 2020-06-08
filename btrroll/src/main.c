#include <btrfsutil.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <libgen.h>
#include <linux/magic.h>
#include <linux/reboot.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/reboot.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/vfs.h>

#include <dialog.h>
#include <macros.h>
#include <root.h>
#include <run.h>

#define INITRD_RELEASE_PATH "/etc/initrd-release"

void main_menu();
void snapshot_menu();
void snapshot_detail_menu(dialog_t *dialog, char *snapshot);
int provision_subvol(char *path);

int wait_for_input(time_t seconds) {
  fd_set set;
  struct timeval timeout;

  FD_ZERO(&set);
  FD_SET(STDIN_FILENO, &set);

  timeout.tv_sec = seconds;
  timeout.tv_usec = 0;

  int ret;
  while ((ret = select(FD_SETSIZE, &set, NULL, NULL, &timeout)) < 0 && errno == EAGAIN);

  return ret;
}

int main(int argc, char **argv) {
  /*
  if (!access(INITRD_RELEASE_PATH, F_OK)) {
    eprintf("btrroll: error: could not find " INITRD_RELEASE_PATH "! " \
            "btrroll must be run from within the initial ramdisk.\n");
    return EXIT_FAILURE;
  }
  */

  // TODO: Due to terminal input buffering, I can only wait for Enter unless
  // I actually use ncurses. Make this as ergonomic as I can given that limitation..
  // Consider dialog_timeout (dialog --timeout) to give a better indication
  if (wait_for_input(1) == 0)
    return 0;

  dialog_t dialog;
  dialog_init(&dialog);

  main_menu(&dialog);

  dialog_free(&dialog);

  return EXIT_SUCCESS;
}

void main_menu(dialog_t *dialog) {
  static const char *ITEMS[] = {
    "Boot/restore a snapshot",
    "Launch a shell",
    "Reboot",
    "Shutdown",
    "Exit (continue booting)"
  };

  size_t choice = 0;
  while (true) {
    dialog->buttons.ok = false;
    dialog->buttons.cancel = false;

    int ret = dialog_choose(dialog,
        ITEMS, lenof(ITEMS), &choice,
        "Main Menu", "What would you like to do?");

    dialog_reset(dialog);

    if (ret == DIALOG_RESPONSE_CANCEL)
      return;

    switch (choice) {
      case 0:
        // TODO: make sure mounts/unmounts, or at least chdirs, are reverted
        snapshot_menu(dialog);
        break;
      case 1:
        // TODO: cd to /; see above
        dialog_clear(dialog);
        run("sh", NULL);
        break;
      case 2:
        // TODO: Make sure this does not cause any data loss
        sync();
        reboot(LINUX_REBOOT_CMD_RESTART);
        break;
      case 3:
        // TODO: Make sure this does not cause any data loss
        sync();
        reboot(LINUX_REBOOT_CMD_POWER_OFF);
        break;
      default:
        dialog_ok(dialog, "Error", "Encountered an error."); // TODO
        return;
    }
  }
}

#define BTRFS_MOUNTPOINT "/btrfs_root"

char *mount_root_subvol(dialog_t *dialog) {
  char root[0x1000], flags[0x1000];

  if (get_root(root, sizeof(root), flags, sizeof(flags))) {
    dialog_ok(dialog, "Error",
        "get_root failed: %s",
        strerror(errno));
    return NULL;
  }

  struct stat sb;
  bool needs_mount;
  if (stat(BTRFS_MOUNTPOINT, &sb)) {
    // The mountpoint directory does not exist; create it
    needs_mount = true;
    if (mkdir(BTRFS_MOUNTPOINT, 0700)) {
      perror("mkdir");
      return NULL;
    }
  } else {
    // The mountpoint directory already exists
    struct stat sbp;
    if (stat(BTRFS_MOUNTPOINT "/..", &sbp)) {
      perror("stat");
      return NULL;
    }
    // Only mount over it if it's not already a mountpoint
    needs_mount = sb.st_dev == sbp.st_dev;
  }
  
  // Mount if not mounted already
  // TODO: flags -= subvol{,id}
  if (needs_mount && mount_root(root, BTRFS_MOUNTPOINT, "")) {
    dialog_ok(dialog, "Error",
        "mount_root with `%s` failed: %s",
        root, strerror(errno));
    return NULL; // TODO: unmount root?
  }

  // Check that the mounted filesystem is actually BTRFS
  struct statfs sfb;
  if (statfs(BTRFS_MOUNTPOINT, &sfb)) {
    perror("statfs");
    return NULL;
  }

  if (sfb.f_type != BTRFS_SUPER_MAGIC) {
    dialog_ok(dialog, "Error",
        "Root device `%s` is not a BTRFS filesystem. %x", root, sfb.f_type);
    errno = EINVAL;
    return NULL;
  }

  char *subvol_path = get_btrfs_root_subvol_path(BTRFS_MOUNTPOINT, flags);
  if (!subvol_path) {
    dialog_ok(dialog, "Error",
        "get_btrfs_root failed: %s",
        strerror(errno));
    return NULL;
  }

  return subvol_path;
}

#define SUBVOL_DIR_SUFFIX ".d"
#define SUBVOL_CUR_NAME "current"
#define SUBVOL_SNAP_NAME "snapshots"

void snapshot_menu(dialog_t *dialog) {
  // Get the root subvolume path relative to the BTRFS partition root
  // This path is relative in this case, but sometimes start with a /
  char *subvol_path = mount_root_subvol(dialog);
  if (!subvol_path) {
    dialog_ok(dialog, "Error",
        "Failed to mount root subvolume: %s",
        strerror(errno));
    return;
  }

  // Make subvol_path absolute
  {
    char *tmp = malloc(strlen(BTRFS_MOUNTPOINT) + strlen(subvol_path) + 2);
    sprintf(tmp, BTRFS_MOUNTPOINT "%s%s", subvol_path[0] == '/' ? "" : "/", subvol_path);
    free(subvol_path);
    subvol_path = tmp;
  }

  // Check if the root subvol is already set up for use with btrroll
  struct stat info;
  if (lstat(subvol_path, &info)) {
    dialog_ok(dialog, "Error",
        "Failed to stat subvol path `%s`: %s",
        subvol_path, strerror(errno));
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
  char * snapshots_path = malloc(snapshots_path_len);
  sprintf(snapshots_path, "%s" SUBVOL_DIR_SUFFIX "/" SUBVOL_SNAP_NAME, subvol_path);
  free(subvol_path);

  DIR * const snapshots_dir = opendir(snapshots_path);
  if (!snapshots_dir) {
    if (errno == ENOENT)
      dialog_ok(dialog, "Warning",
          "There is no `snapshots` directory at `%s`. Create one and populate "
          "it with snapshots of your root subvolume in order to use this menu.",
          snapshots_path);
    else
      dialog_ok(dialog, "Error", "Failed to open `%s`: %s",
          snapshots_path, strerror(errno));
    free(snapshots_path);
    return;
  }

  // Switch to the snapshots base dir to make paths more convenient.
  // TODO: switch back afterward?
  chdir(snapshots_path);
  free(snapshots_path);

  const size_t SNAPSHOTS_LEN_MAX = 0x100;
  size_t snapshots_len;
  char **snapshots = malloc(SNAPSHOTS_LEN_MAX * sizeof(char*));
  {
    struct dirent *ep;
    char ** const snapshots_end = snapshots + SNAPSHOTS_LEN_MAX;
    char **p = snapshots;
    errno = 0;
    while ((ep = readdir(snapshots_dir)) && p != snapshots_end) {
      if (errno) {
        // TODO: dir read error, cleanup
        return;
      }
      errno = 0;

      // Record the names of all applicable directories in snapshots/
      if (ep->d_type == DT_DIR &&                   // Is a directory
          ep->d_name[0] != '.' &&                   // Does not start with '.'
          btrfs_util_is_subvolume(ep->d_name) == BTRFS_UTIL_OK) // Is a subvol
        *p++ = strcpy(malloc(strlen(ep->d_name)+1), ep->d_name);
    }
    snapshots_len = p - snapshots;
    *p = NULL;

    if (closedir(snapshots_dir))
      perror("closedir");
  }

  int ret;
  size_t choice = 0;

  while (true) {
    ret = dialog_choose(dialog,
        (const char**)snapshots, snapshots_len, &choice,
        "Snapshots", "Select a snapshot from the list below.");

    if (ret == DIALOG_RESPONSE_CANCEL || ret < 0)
      break;

    snapshot_detail_menu(dialog, snapshots[choice]);
  }

  for (size_t i = 0; i < snapshots_len; ++i)
    free(snapshots[i]);
  free(snapshots);

  //if (ret > 0)
  //  return ret;
  //return 0;
}

#define BTRROLL_INFO_FILE ".btrroll.info"

void snapshot_detail_menu(dialog_t *dialog, char *snapshot) {
  char *info_file_path = malloc(strlen(snapshot) +
      strlen(BTRROLL_INFO_FILE) + 2);
  sprintf(info_file_path, "%s/%s", snapshot, BTRROLL_INFO_FILE);

  dialog->buttons.extra = true;
  dialog->buttons.help = true;

  dialog->labels.ok = "Cancel";
  dialog->labels.extra = "Boot";
  dialog->labels.help = "Restore";

  int ret = dialog_view_file(dialog, snapshot, info_file_path);
  dialog_reset(dialog);
  free(info_file_path);

  if (ret == DIALOG_RESPONSE_EXTRA) {
    //boot_snapshot(snapshot);
  } else if (ret == DIALOG_RESPONSE_HELP) {
    //restore_snapshot(snapshot);
  }
}

int provision_subvol(char *path) {
  CLEANUP_DECLARE(ret);

  char *tmp = malloc(strlen(path) + strlen(SUBVOL_DIR_SUFFIX) +
       strlen(SUBVOL_CUR_NAME) + 2);

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

CLEANUP:
  free(tmp);
  return ret;
}

/* Restore from subvolume
 * (Ask to) Move subvol.d/current to subvol.d/backups and and set it to readonly
 * Make an RW copy of the subvolume to restore in subvol.d/current
 * Set the default boot entry based on the kernel version in the new current
 * Reboot
 */

void snapshot_restore(const char *snapshot) {
  // btrfs_util_set_subvolume_read_only
}

/* Boot from subvolume
 * Make an RW copy of the subvolume to boot in subvol.d/temp
 * Create a file subvol.d/temp.count with content "0"
 * Set the subvol symlink to point to temp
 * Set the next boot entry based on the kernel version in temp
 * Reboot
 *
 * At next boot (stateless)
 * If subvol symlink points to subvol.d/temp and count = ...
 *  0, increment temp.count to "1" and boot normally w/o interface
 *  1, set symlink back to current, delete temp, and remove temp.count
 *
 */

void snapshot_boot(const char *snapshot) {
  // btrfs_util_create_snapshot
}

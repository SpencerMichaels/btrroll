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
#include <sys/types.h>
#include <sys/vfs.h>

#include <dialog/cli.h>
#include <dialog/tui.h>
#include <macros.h>
#include <root.h>
#include <run.h>

#define INITRD_RELEASE_PATH "/etc/initrd-release"

void main_menu();
void snapshot_menu();
void snapshot_detail_menu(dialog_backend_t *dialog, char *snapshot);
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

void snapshot_menu(dialog_backend_t *dialog) {
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

  fprintf(stderr, "1\n");

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

  fprintf(stderr, "2\n");

  // Switch to the snapshots base dir to make paths more convenient.
  // TODO: switch back afterward?
  chdir(snapshots_path);
  free(snapshots_path);

  fprintf(stderr, "3\n");

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

  for (char **x = snapshots; *x; ++x)
    fprintf(stderr, "%s", *x);

  int choice = 0;
  while (true) {
    choice = dialog_choose(dialog,
        (const char**)snapshots, snapshots_len, choice,
        "Snapshots", "Select a snapshot from the list below.");

    if (choice == DIALOG_RESPONSE_CANCEL)
      break;
    if (choice < 0)
      break; // error

    snapshot_detail_menu(dialog, snapshots[choice]);
  }

  fprintf(stderr, "5\n");

  for (size_t i = 0; i < snapshots_len; ++i)
    free(snapshots[i]);
  free(snapshots);
}

#define BTRROLL_INFO_FILE ".btrroll.info"

void snapshot_detail_menu(dialog_backend_t *dialog, char *snapshot) {
  char *info_file_path = malloc(strlen(snapshot) +
      strlen(BTRROLL_INFO_FILE) + 2);
  sprintf(info_file_path, "%s/%s", snapshot, BTRROLL_INFO_FILE);

  FILE *info_file = fopen(info_file_path, "r");
  if (!info_file)
    return; // TODO error, cleanup

  fseek(info_file, 0, SEEK_END);
  const long content_len = ftell(info_file);
  rewind(info_file);

  char *content = malloc(content_len+1);
  fread(content, 1, content_len, info_file);
  fclose(info_file);

  content[content_len] = '\0';

  dialog_ok(dialog, snapshot, "%s", content);
  free(info_file_path);
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

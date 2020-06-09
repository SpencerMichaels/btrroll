#include <errno.h>
#include <libgen.h>
#include <linux/magic.h>
#include <sys/mount.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/vfs.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <constants.h>
#include <dialog.h>
#include <macros.h>
#include <path.h>
#include <root.h>
#include <run.h>
#include <subvol.h>
#include <ui.h>

static const char *btrfs_root_mountpoint = NULL;

int wait_for_input(time_t seconds);
int btrfs_root_mount(const char *mountpoint, char *root, char *flags);
void btrfs_root_unmount();

void snapshot_restore(const char *snapshot);
void snapshot_boot(const char *snapshot);

int main(int argc, char **argv) {
  CLEANUP_DECLARE(did_mount_fail);

  // Check that we're in initramfs; otherwise, undefined behavior may occur
  if (access(INITRD_RELEASE_PATH, F_OK)) {
    eprintf("btrroll: error: could not access " INITRD_RELEASE_PATH "! " \
            "btrroll must be run from within the initial ramdisk.\n");
    return EXIT_FAILURE;
  }

  // Get the root device and its mount flags from the kernel command line
  char root[0x1000], flags[0x1000];
  if (get_root(arr_and_size(root), arr_and_size(flags))) {
    perror("get_root");
    FAIL(did_mount_fail);
  }

  // TODO: there is a race condition where /dev/disk/by-* is not yet available.
  // Retry mounting a couple of times, or adjust the systemd service.
  wait_for_input(3);

  // Mount the root device, and unmount automatically on exit
  const char *mountpoint = BTRFS_MOUNTPOINT; // TODO: load from config file
  if (btrfs_root_mount(mountpoint, root, flags)) {
    perror("btrfs_root_mount");
    FAIL(did_mount_fail);
  }
  atexit(btrfs_root_unmount);

  // Get the path to the root subvolume (distinct from the root _device_)
  char *root_subvol = get_btrfs_root_subvol_path(mountpoint, flags);
  if (!root_subvol) {
    perror("get_btrfs_root_subvol_path");
    FAIL(did_mount_fail);
  }

  { // Make the root_subvol absolute
    char *tmp = pathcat(mountpoint, root_subvol);
    free(root_subvol);
    root_subvol = tmp;
  }

  {
    char *subvol_dir_path = get_subvol_dir_path(root_subvol);
    // TODO: check .btrroll-state and act on it if necessary
    free(subvol_dir_path);
  }

  // TODO: Due to terminal input buffering, I can only wait for Enter unless
  // I use ncurses. Make this as ergonomic as can given that limitation.
  // Consider dialog_timeout (dialog --timeout) to give a better indication
  //if (wait_for_input(1) == 0)
  //  return EXIT_SUCCESS;

  dialog_t dialog;
CLEANUP:
  dialog_init(&dialog);

  if (did_mount_fail)
    dialog_ok(&dialog, "Error", "Mounting the root subvolume failed!");

  int ret = main_menu(&dialog, root_subvol);

  dialog_free(&dialog);
  free(root_subvol);

  if (!ret)
    return EXIT_SUCCESS;
  return ret;
}

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

int btrfs_root_mount(const char *mountpoint, char *root, char *flags) {
  if (!mountpoint) {
    errno = EINVAL;
    return -1;
  }

  // Create the mountpoint directory if it does not already exist
  if (mkdir(mountpoint, 0700) && errno != EEXIST) {
    perror("mkdir");
    return -1;
  }

  // TODO: flags -= subvol{,id}
  if (mount_root(mountpoint, "btrfs", root, "")) {
    perror("mount_root");
    return -1;
  }

  { // Check that the mounted filesystem is actually BTRFS
    struct statfs sfb;
    if (statfs(BTRFS_MOUNTPOINT, &sfb)) {
      perror("statfs");
      return -1;
    }

    if (sfb.f_type != BTRFS_SUPER_MAGIC) {
      eprintf("error: not a btrfs partition: %s\n", root);
      umount(mountpoint);
      errno = EINVAL;
      return -1;
    }
  }

  btrfs_root_mountpoint = mountpoint;

  return 0;
}

void btrfs_root_unmount() {
  if (btrfs_root_mountpoint && umount(btrfs_root_mountpoint))
      perror("umount");
  btrfs_root_mountpoint = NULL;
}

#include <errno.h>
#include <sys/select.h>
#include <stdlib.h>
#include <unistd.h>

#include <dialog.h>
#include <macros.h>
#include <root.h>
#include <run.h>
#include <ui.h>

#define INITRD_RELEASE_PATH "/etc/initrd-release"

int wait_for_input(time_t seconds);
void snapshot_restore(const char *snapshot);
void snapshot_boot(const char *snapshot);

int main(int argc, char **argv) {
  if (access(INITRD_RELEASE_PATH, F_OK)) {
    eprintf("btrroll: error: could not access " INITRD_RELEASE_PATH "! " \
            "btrroll must be run from within the initial ramdisk.\n");
    return EXIT_FAILURE;
  }

  // TODO: Due to terminal input buffering, I can only wait for Enter unless
  // I actually use ncurses. Make this as ergonomic as I can given that limitation.
  // Consider dialog_timeout (dialog --timeout) to give a better indication
  if (wait_for_input(1) == 0)
    return 0;

  dialog_t dialog;
  dialog_init(&dialog);

  int ret = main_menu(&dialog);

  dialog_free(&dialog);

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

#include <btrfsutil.h>
#include <errno.h>
#include <linux/magic.h>
#include <linux/reboot.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/reboot.h>
#include <unistd.h>

#include <constants.h>
#include <dialog.h>
#include <macros.h>
#include <path.h>
#include <snapshot.h>

/* Restore from subvolume
 * (Ask to) Move subvol.d/current to subvol.d/backups and and set it to readonly
 * Make an RW copy of the subvolume to restore in subvol.d/current
 * Set the default boot entry based on the kernel version in the new current
 * Reboot
 */

int snapshot_restore(char *root_subvol_dir, const char *snapshot) {
  // btrfs_util_set_subvolume_read_only
}

int snapshot_boot(char *root_subvol_dir, const char *snapshot) {
  CLEANUP_DECLARE(ret);
  char *tmp_path = NULL, *state_path = NULL;
  FILE *state_file = NULL;

  // Make an RW copy of the subvolume to boot in subvol.d/temp
  tmp_path = pathcat(root_subvol_dir, SUBVOL_TMP_NAME);

  enum btrfs_util_error err = btrfs_util_create_snapshot(
      snapshot, tmp_path, 0, NULL, NULL);
  if (err != BTRFS_UTIL_OK) {
    eprintf("error: %s\n", btrfs_util_strerror(err));
    FAIL(ret);
  }
  
  // Write state file so btrroll knows what to do upon reboot
  state_path = pathcat(root_subvol_dir, STATE_FILE);
  state_file = fopen(state_path, "w");
  if (!state_file) {
    perror("fopen");
    FAIL(ret);
  }

  if (fputs(STATE_BOOT_TEMP, state_file) < 0) {
    perror("fputs");
    FAIL(ret);
  }

CLEANUP:
  if (state_file)
    fclose(state_file);
  free(state_path);
  free(tmp_path);

  if (!ret) {
    sync();
    reboot(LINUX_REBOOT_CMD_RESTART);
  }

  return ret;
}

int handle_state(char *root_subvol_dir) {
  CLEANUP_DECLARE(ret);
  char *state_path;
  FILE *state_file;

  state_path = pathcat(root_subvol_dir, STATE_FILE);
  state_file = fopen(state_path, "w");
  if (!state_file) {
    if (errno == ENOENT)
      goto CLEANUP; // no problem
    perror("fopen");
    FAIL(ret);
  }

  char buf[0x100];
  if (fgets(buf, sizeof(buf), state_file) < 0) {
    perror("fread");
    FAIL(ret);
  }
  buf[sizeof(buf)-1] = 0;

  if (strncmp(buf, str_and_len(STATE_BOOT_TEMP))) {
    // Set the subvol symlink to point to temp
    // Set the next boot entry based on the kernel version in temp
    // Reboot
  }
  else if (strncmp(buf, str_and_len(STATE_BOOT_TEMP_CLEANUP))) {
    // Set symlink back to current
    // Delete temp
    // Remove state file
  }

CLEANUP:
  if (state_file)
    fclose(state_file);
  free(state_path);

  return ret;
}

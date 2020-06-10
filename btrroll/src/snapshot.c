#include <btrfsutil.h>
#include <errno.h>
#include <libgen.h>
#include <linux/magic.h>
#include <linux/reboot.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/reboot.h>
#include <unistd.h>

#include <boot.h>
#include <constants.h>
#include <dialog.h>
#include <macros.h>
#include <path.h>
#include <snapshot.h>
#include <subvol.h>

static int swap_symlink(
    const char *path,
    const char *src_new,
    const char* src_fallback);

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

  if (fputs(STATE_BOOT_TEMP, state_file) == EOF && ferror(state_file)) {
    perror("fputs");
    FAIL(ret);
  }

CLEANUP:
  if (state_file)
    if (fclose(state_file))
      perror("fclose");
  free(state_path);
  free(tmp_path);

  if (!ret)
    restart();

  return ret;
}

int snapshot_continue(char *root_subvol) {
  CLEANUP_DECLARE(ret);

  char *root_subvol_dir = get_subvol_dir_path(root_subvol);
  char *state_path = pathcat(root_subvol_dir, STATE_FILE);
  char *tmp_path_rel = pathcat(basename(root_subvol_dir), SUBVOL_TMP_NAME);
  char *cur_path_rel = pathcat(basename(root_subvol_dir), SUBVOL_CUR_NAME);
  FILE *state_file = fopen(state_path, "r");

  if (!state_file) {
    if (errno == ENOENT)
      goto CLEANUP; // no problem
    perror("fopen");
    FAIL(ret);
  }

  // Read the state value
  char state[0x100];
  if (!fgets(state, sizeof(state), state_file) && ferror(state_file)) {
    perror("fread");
    FAIL(ret);
  }
  state[sizeof(state)-1] = 0;

  eprintf("state is: %s\n", state);

  // Done reading the state file
  if (fclose(state_file)) {
    perror("fclose");
    FAIL(ret);
  }
  state_file = NULL;

  // boot: Boot into the `temp` subvolume
  if (!strcmp(STATE_BOOT_TEMP, state)) {
    // Swap the symlink from `current` to `temp`
    if (swap_symlink(root_subvol, tmp_path_rel, cur_path_rel)) {
      perror("swap_symlink");
      FAIL(ret);
    }

    // TODO: Set the oneshot boot entry based on the kernel version in temp

    // Set the post-boot cleanup state for the next reboot
    state_file = fopen(state_path, "w");
    if (!state_file) {
      perror("fopen");
      FAIL(ret);
    }

    if (fputs(STATE_BOOT_TEMP_CLEANUP, state_file) == EOF && ferror(state_file)) {
      perror("fputs");
      FAIL(ret);
    }

    goto CLEANUP;
  }

  // boot-cleanup: Cleanup after having booted into the `temp` subvolume
  else if (!strcmp(STATE_BOOT_TEMP_CLEANUP, state)) {
    // Swap the symlink back from `temp` to `current`
    if (swap_symlink(root_subvol, cur_path_rel, tmp_path_rel)) {
      perror("swap_symlink");
      FAIL(ret);
    }

    // Delete the `temp` subvolume
    char *tmp_path = pathcat(root_subvol_dir, SUBVOL_TMP_NAME);
    enum btrfs_util_error err = btrfs_util_delete_subvolume(tmp_path, 0);
    free(tmp_path);

    if (err != BTRFS_UTIL_OK) {
      eprintf("error: %s\n", btrfs_util_strerror(err));
      FAIL(ret);
    }

    // Remove the state file
    if (remove(state_path))
      perror("remove");

    goto CLEANUP;
  }

CLEANUP:
  if (state_file)
    if (fclose(state_file))
      perror("fclose");
  free(state_path);
  free(tmp_path_rel);
  free(cur_path_rel);
  free(root_subvol_dir);

  return ret;
}

static int swap_symlink(const char *path, const char *src_new, const char* src_fallback) {
  // Remove the original symlink
  if (unlink(path)) {
    perror("unlink");
    return -1;
  }

  if (symlink(src_new, path)) {
    perror("symlink");
    if (symlink(src_fallback, path))
      perror("symlink");
    return -1;
  }

  return 0;
}

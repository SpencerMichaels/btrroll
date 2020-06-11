#include <btrfsutil.h>
#include <dirent.h>
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
#include <kver.h>
#include <macros.h>
#include <path.h>
#include <snapshot.h>
#include <subvol.h>

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

int snapshot_restore(char *root_subvol_dir, const char *snapshot, const char *backup) {
  CLEANUP_DECLARE(ret);

  char *current = pathcat(root_subvol_dir, SUBVOL_CUR_NAME);
  int err;

  // Back up the current subvolume if desired
  if (backup) {
    // Move subvol.d/current to subvol.d/snapshots/backup
    if (rename(current, backup)) {
      perror("rename");
      FAIL(ret);
    }
    // Make it read-only
    err = btrfs_util_set_subvolume_read_only(backup, 1);
    if (err != BTRFS_UTIL_OK) {
      eprintf("error: %s\n", btrfs_util_strerror(err));
      FAIL(ret);
    }
  }
  // otherwise just delete subvol.d/current
  else if ((err = btrfs_util_delete_subvolume(current, 0)) != BTRFS_UTIL_OK) {
    eprintf("error: %s\n", btrfs_util_strerror(err));
    FAIL(ret);
  }

  // Create an RW copy of the snapshot in place of the old `current` subvolume
  err = btrfs_util_create_snapshot(snapshot, current, 0, NULL, NULL);
  if (err != BTRFS_UTIL_OK) {
    eprintf("error: %s\n", btrfs_util_strerror(err));
    FAIL(ret);
  }

  restart();

CLEANUP:
  free(current);
  return ret;
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
    perror("fgets");
    FAIL(ret);
  }
  state[sizeof(state)-1] = 0;

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

    ret = 1; // continue without interactive console
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

    if (err != BTRFS_UTIL_OK && err != BTRFS_UTIL_ERROR_SUBVOLUME_NOT_FOUND) {
      eprintf("error %d: %s\n", err, btrfs_util_strerror(err));
      FAIL(ret);
    }

    // Remove the state file
    if (remove(state_path))
      perror("remove");

    // continue as normal
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

int get_kernel_versions(
    const char *snapshot,
    char **versions,
    size_t versions_len)
{
  CLEANUP_DECLARE(ret);

  char *path = pathcat(snapshot, "usr/lib/modules");
  DIR * const modules = opendir(path);

  if (!modules) {
    perror("opendir");
    FAIL(ret);
  }

  struct dirent *ep;
  char **p = versions;
  errno = 0;
  while ((ep = readdir(modules)) && versions_len) {
    if (errno) {
      perror("readdir");
      FAIL(ret);
    }
    
    if (ep->d_type == DT_DIR && // Is a directory
        ep->d_name[0] != '.')   // Is not hidden
    {
      *p++ = strdup(ep->d_name);
      --versions_len;
    }

    errno = 0;
  }
  ret = p - versions;
  *p = NULL;

CLEANUP:
  if (closedir(modules))
    perror("closedir");
  free(path);
  return ret;
}

int get_compatible_boot_entries(
    const char *snapshot, const char *esp_path,
    struct bootctl_entry *entries, const size_t entries_len)
{
  // Get the list of all available boot entries from systemd-boot
  bootctl_entry_t all_entries[32];
  int num_entries = bootctl_list(esp_path, all_entries, lenof(all_entries));
  if (num_entries < 0) {
    perror("bootctl_list");
    return -1;
  }

  // Get the list of all kernel versions supported by the snapshot
  char *versions[32];
  int num_versions = get_kernel_versions(snapshot, versions, sizeof(versions));
  if (num_versions < 0) {
    perror("get_kernel_versions");
    return -1;
  }

  bootctl_entry_t *e = entries;
  for (int i = 0; i < num_entries; ++i) {
    bootctl_entry_t *entry = all_entries + i;
    eprintf("entry: %s\n", entry->id);

    // Skip entries sourced from outside of the ESP (usually autogenerated)
    const size_t esp_path_len = strlen(esp_path);
    if (!strncmp(entry->id, "auto-", 5) || strncmp(entry->source, esp_path, esp_path_len))
      continue;

    char *ext = strrchr(entry->id, '.');
    if (!ext)
      continue;

    int err;
    char version[0x100];
    if (!strcmp(".efi", ext))
      err = kver_pe(entry->source, version, sizeof(version));
    else if (!strcmp(".conf", ext)) {
      char *kernel_path = pathcat(esp_path, entry->kernel);
      err = kver(kernel_path, version, sizeof(version));
      eprintf("kver for `%s`: %d: %s\n", kernel_path, err, version);
      free(kernel_path);
    }

    if (err) {
      perror("kver");
      continue;
    }

    // Add it to the output list if it matches a supported version
    for (int j = 0; j < num_versions; ++j) {
      eprintf("%s / %s\n", versions[j], version);
      if (!strncmp(versions[j], version, sizeof(version))) {
        memcpy(e++, entry, sizeof(struct bootctl_entry));
        break;
      }
    }
  }

  return e - entries;
}

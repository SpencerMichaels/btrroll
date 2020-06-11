#ifndef __SNAPSHOT_H__
#define __SNAPSHOT_H__

struct bootctl_entry;

int snapshot_restore(char *root_subvol_dir, const char *snapshot, const char *backup);
int snapshot_boot(char *root_subvol_dir, const char *snapshot);
int snapshot_continue(char *root_subvol);
int get_kernel_versions(const char *snapshot, char **versions, size_t versions_len);

int get_compatible_boot_entries(
    const char *snapshot, const char *esp_path,
    struct bootctl_entry *entries, const size_t entries_len);

#endif

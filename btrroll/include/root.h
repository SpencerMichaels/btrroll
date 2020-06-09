#ifndef __ROOT_H__
#define __ROOT_H__

// Get the root subvolume specifier (path, label, etc.) and mount flags
int get_root(
    char * const root, const size_t root_len,
    char * const flags, const size_t flags_len);

// Mount the root subvolume
int mount_root(
    const char * const mountpoint,
    const char * const fs_type,
    const char * const root,
    const char * const flags);

/* Get the path of the root subvolume for the BTRFS FS mounted at `mountpoint`.
 * The root subvol may be specified explicitly in the flags, implicitly by the
 * filesystem's defalut subvolume setting, or just fall back to / otherwise.
 *
 * Notes to keep in mind:
 * - The path returned is relative to `mountpoint`, so e.g. /dir would
 *   correspond to `/mountpoint/dir`.
 * - The path string returned by this function must be freed by the caller.
 * - This function mutates `flags`; copy this value first if you need it.
 */
char * get_btrfs_root_subvol_path(
    const char * const mountpoint,
    char * flags);

#endif

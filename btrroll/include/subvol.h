#ifndef __SUBVOL_H__
#define __SUBVOL_H__

char * get_btrfs_root_subvol_path(
    const char * const mountpoint,
    char *flags);

char * get_subvol_dir_path(char *subvol_path);

int is_subvol_toplevel(char *path);
int is_subvol_provisioned(char *path);
int provision_subvol(char *path);

#endif

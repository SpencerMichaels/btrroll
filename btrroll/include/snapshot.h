#ifndef __SNAPSHOT_H__
#define __SNAPSHOT_H__

int snapshot_restore(char *root_subvol_dir, const char *snapshot, const char *backup);
int snapshot_boot(char *root_subvol_dir, const char *snapshot);
int snapshot_continue(char *root_subvol);

#endif

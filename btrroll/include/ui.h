#ifndef __UI_H__
#define __UI_H__

#include <dialog.h>
#include <stdbool.h>

int main_menu(dialog_t *dialog, char *root_subvol);
void snapshot_menu(dialog_t *dialog, char *root_subvol_dir);
int snapshot_detail_menu(dialog_t *dialog, char *snapshot);
int provision_subvol(char *path);

#endif

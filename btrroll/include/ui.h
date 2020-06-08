#ifndef __UI_H__
#define __UI_H__

#include <dialog.h>

int main_menu(dialog_t *dialog);
void snapshot_menu(dialog_t *dialog);
void snapshot_detail_menu(dialog_t *dialog, char *snapshot);
int provision_subvol(char *path);

#endif

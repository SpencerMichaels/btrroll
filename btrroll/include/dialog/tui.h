#ifndef __DIALOG_TUI__
#define __DIALOG_TUI__

#include <stdbool.h>

#include "backend.h"

bool dialog_tui_available();
void dialog_tui_init(dialog_backend_t * const backend);

int dialog_tui_choose(
    void * const data,
    const char **items, const size_t items_len,
    const size_t pos,
    const char *title, const char *msg);

int dialog_tui_confirm(
    void * const data,
    bool default_,
    const char *title, const char *msg);

int dialog_tui_ok(
    void * const data,
    const char *title, const char *msg);

int dialog_tui_view_file(
    void * const data,
    const char * title,
    const char * const filepath);

int dialog_tui_clear(void * const data);

#endif

#ifndef __DIALOG_CLI__
#define __DIALOG_CLI__

#include <stdbool.h>

#include "backend.h"

void dialog_cli_init(dialog_backend_t * const backend);

int dialog_cli_choose(
    void * const data,
    const char **items, const size_t items_len,
    const size_t pos,
    const char *title, const char *msg);

int dialog_cli_confirm(
    void * const data,
    bool default_,
    const char *title, const char *msg);

int dialog_cli_ok(
    void * const data,
    const char *title, const char *msg);

int dialog_cli_view_file(
    void * const data,
    const char * title,
    const char * const filepath);

int dialog_cli_clear(void * const data);

#endif

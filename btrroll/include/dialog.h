#ifndef __DIALOG_TUI__
#define __DIALOG_TUI__

#include <limits.h>
#include <stdbool.h>

#define DIALOG_RESPONSE_NO 0
#define DIALOG_RESPONSE_YES 1
#define DIALOG_RESPONSE_CANCEL INT_MIN

typedef struct dialog_statuses {
  int cancel,
      error,
      esc,
      extra,
      help,
      item_help,
      ok;
} dialog_statuses_t;

typedef struct dialog {
  dialog_statuses_t statuses;
} dialog_t;

void dialog_init(dialog_t * const dialog);
void dialog_free(dialog_t * const dialog);

int dialog_choose(
    dialog_t * const dialog,
    const char **items, const size_t items_len,
    const size_t pos,
    const char *title, const char *format, ...);

int dialog_confirm(
    dialog_t * const dialog,
    bool default_,
    const char *title, const char *format, ...);

int dialog_ok(
    dialog_t * const dialog,
    const char *title, const char *format, ...);

int dialog_view_file(
    dialog_t * const dialog,
    const char * title,
    const char * const filepath);

int dialog_clear(dialog_t * const dialog);

#endif


#ifndef __DIALOG_H__
#define __DIALOG_H__

#include <limits.h>
#include <stdbool.h>

typedef enum dialog_response {
  DIALOG_RESPONSE_OK = 0,
  DIALOG_RESPONSE_YES = 0,
  DIALOG_RESPONSE_CANCEL = 1,
  DIALOG_RESPONSE_NO = 1,
  DIALOG_RESPONSE_HELP = 2,
  DIALOG_RESPONSE_ITEM_HELP = 2,
  DIALOG_RESPONSE_EXTRA = 3,
} dialog_response_t;

typedef struct dialog_labels {
  const char *cancel,
             *exit,
             *extra,
             *help,
             *no,
             *ok,
             *yes;
} dialog_labels_t;

typedef struct dialog_buttons {
  bool ok,
       cancel,
       extra,
       help;
} dialog_buttons_t;

typedef struct dialog {
  dialog_labels_t labels;
  dialog_buttons_t buttons;
} dialog_t;

void dialog_init(dialog_t * const dialog);
void dialog_reset(dialog_t * const dialog);
void dialog_free(dialog_t * const dialog);

int dialog_choose(
    dialog_t * const dialog,
    const char **items, size_t items_len,
    size_t *choice,
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


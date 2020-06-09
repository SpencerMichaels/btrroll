#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include <dialog.h>
#include <macros.h>
#include <run.h>

#define BACKTITLE "btrroll 1.0.0"

// Default return codes as defined by the `dialog` binary
#define DIALOG_CANCEL 1
#define DIALOG_ERROR -1
#define DIALOG_ESC 255
#define DIALOG_EXTRA 3
#define DIALOG_HELP 2
#define DIALOG_ITEM_HELP 2
#define DIALOG_OK 0

#define LABEL_ARGS \
  "--cancel-label", dialog->labels.cancel ? dialog->labels.cancel : "Cancel", \
  "--exit-label", dialog->labels.exit ? dialog->labels.exit : "Exit", \
  "--extra-label", dialog->labels.extra ? dialog->labels.extra : "Extra", \
  "--help-label", dialog->labels.help ? dialog->labels.help : "Help", \
  "--no-label", dialog->labels.no ? dialog->labels.no : "No", \
  "--ok-label", dialog->labels.ok ? dialog->labels.ok : "OK", \
  "--yes-label", dialog->labels.yes ? dialog->labels.yes : "Yes"

// --clear here is used as a no-op
#define BUTTON_ARGS \
  dialog->buttons.ok ? "--clear" : "--nook", \
  dialog->buttons.cancel ? "--clear" : "--nocancel", \
  dialog->buttons.extra ? "--extra-button" : "--clear", \
  dialog->buttons.help ? "--help-button" : "--clear"

typedef struct dialog_statuses {
  int cancel,
      error,
      esc,
      extra,
      help,
      item_help,
      ok;
} dialog_statuses_t;

static dialog_statuses_t dialog_statuses;

static char tmp_buf[0x1000];

// Common code snippet facilitating printf-like semantics in dialog messages.
#define format_msg(buf, format) \
{ \
  int ret; \
  va_list args; \
\
  va_start(args, format); \
  ret = vsprintf(buf, format, args); \
  va_end(args); \
\
  if (ret < 0) \
    return ret; \
}

// Get custom dialog status codes from the env, or fall back to a default.
int get_status_code_(const char *name, int default_) {
  errno = 0;
  const char *value = getenv(name);
  if (value && *value != '\0') {
    char *endptr;
    const long status = strtol(value, &endptr, 10);
    if (*endptr != '\0')
      errno = EINVAL;
    else if (status < INT_MIN && status > INT_MAX)
      errno = ERANGE;
    else
      return status;
  }
  return default_;
}

// Wrapper for the above where the env var and C symbol names are the same
#define get_status_code(name) \
  get_status_code_(#name, name)

// Check the return value of dialog and convert it to a response enum
int check_ret(int ret) {
  if (ret == dialog_statuses.ok)
    return DIALOG_RESPONSE_OK;
  if (ret == dialog_statuses.cancel || ret == dialog_statuses.esc)
    return DIALOG_RESPONSE_CANCEL;
  if (ret == dialog_statuses.help)
    return DIALOG_RESPONSE_HELP;
  if (ret == dialog_statuses.item_help)
    return DIALOG_RESPONSE_ITEM_HELP;
  if (ret == dialog_statuses.extra)
    return DIALOG_RESPONSE_EXTRA;
  return ret;
}

void dialog_init(dialog_t * const dialog) {
  /*
   * dialog allows its return values to be changed by env vars (!?)
   * we have to take this into account when reacting to its status codes
   */
  dialog_statuses.cancel = get_status_code(DIALOG_CANCEL);
  dialog_statuses.error = get_status_code(DIALOG_ERROR);
  dialog_statuses.esc = get_status_code(DIALOG_ESC);
  dialog_statuses.extra = get_status_code(DIALOG_EXTRA);
  dialog_statuses.help = get_status_code(DIALOG_HELP);
  dialog_statuses.item_help = get_status_code(DIALOG_ITEM_HELP);
  dialog_statuses.ok = get_status_code(DIALOG_OK);

  dialog_reset(dialog);
}

void dialog_reset(dialog_t * const dialog) {
  memset((void*)&dialog->labels, 0, sizeof(dialog_labels_t));

  dialog->buttons.ok = true;
  dialog->buttons.cancel = true;
  dialog->buttons.extra = false;
  dialog->buttons.help = false;
}

void dialog_free(dialog_t * const dialog) {
}

// Choose an item from the given list
int dialog_choose(
    dialog_t * const dialog,
    const char **items, size_t items_len,
    size_t *choice,
    const char *title, const char *format, ...)
{
  char pos_str[8];
  snprintf(pos_str, sizeof(pos_str), "%zu", (choice ? *choice : 0) + 1);

  format_msg(tmp_buf, format);
  const char * const args_prefix[] = {
      "dialog",
      "--backtitle", BACKTITLE,
      "--title", title,
      LABEL_ARGS,
      BUTTON_ARGS,
      "--default-item", pos_str,
      "--menu", tmp_buf, "0", "0", "10"
  };

  // Calculate items_len (if zero) by looking for a NULL item
  if (!items_len)
    for (const char **p = items; *p; p++)
      ++items_len;
  if (!items_len)
    return -EINVAL;

  const size_t args_len = 2*items_len + lenof(args_prefix) + 1;
  const char **args = malloc(sizeof(char *) * args_len);

  memcpy(args, args_prefix, sizeof(args_prefix));

  const size_t tag_len = 8; // TODO
  for (size_t i = 0; i < items_len; i += 1) {
    char *tag = malloc(tag_len);
    snprintf(tag, tag_len, "%zu", i+1);
    args[lenof(args_prefix) + i*2] = tag;
    args[lenof(args_prefix) + i*2 + 1] = items[i];
  }

  args[args_len - 1] = NULL;

  static const size_t BUF_LEN = 1024;
  char buf[BUF_LEN];

  const int ret = run_pipe("dialog", args, NULL, 0, buf, BUF_LEN);

  // Only some elements of `args` are heap-allocated
  for (size_t i = 0; i < items_len; i += 1)
    free((char*)args[lenof(args_prefix) + i*2]);
  free(args);

  *choice = strtol(buf, NULL, 10) - 1;
  return check_ret(ret);
}

// Choose YES or NO, defaulting to one or the other on cancellation
int dialog_confirm(
    dialog_t * const dialog,
    bool default_,
    const char *title, const char *format, ...)
{
  // The use of --clear here is a hack to avoid dynamic arg allocation
  // It is basically a no-op that can stand in when --defaultno is not needed
  format_msg(tmp_buf, format);
  const char * args[] = {
      "dialog",
      "--backtitle", BACKTITLE,
      "--title", title,
      LABEL_ARGS,
      BUTTON_ARGS,
      default_ ? "--clear" : "--defaultno",
      "--yesno", tmp_buf, "0", "0",
      NULL
  };

  return check_ret(run("dialog", args));
}

int dialog_ok(
    dialog_t * const dialog,
    const char *title, const char *format, ...)
{
  format_msg(tmp_buf, format);
  const char * args[] = {
      "dialog",
      "--backtitle", BACKTITLE,
      "--title", title,
      LABEL_ARGS,
      BUTTON_ARGS,
      "--msgbox", tmp_buf, "0", "0",
      NULL
  };

  return check_ret(run("dialog", args));
}

// Display the contents of a file
int dialog_view_file(
    dialog_t * const dialog,
    const char * title,
    const char * filepath)
{
  const char * args[] = {
      "dialog",
      "--backtitle", BACKTITLE,
      "--title", title,
      LABEL_ARGS,
      BUTTON_ARGS,
      "--tab-correct",
      "--scrollbar",
      "--textbox", filepath, "0", "0",
      NULL
  };

  return check_ret(run("dialog", args));
}

// Clear the screen
int dialog_clear(dialog_t * const dialog) {
  static const char * args[] = {
      "dialog",
      "--clear",
      NULL
  };

  return check_ret(run("dialog", args));
}

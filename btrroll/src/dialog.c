#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include <dialog.h>
#include <macros.h>
#include <run.h>

#define BACKTITLE "btrroll 1.0.0"

#define DIALOG_CANCEL 1
#define DIALOG_ERROR -1
#define DIALOG_ESC 255
#define DIALOG_EXTRA 3
#define DIALOG_HELP 2
#define DIALOG_ITEM_HELP 2
#define DIALOG_OK 0

static char tmp_buf[0x1000];

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

#define get_status_code(name) \
  get_status_code_(#name, name)

int check_ret(int ret, dialog_statuses_t statuses) {
  if (ret == statuses.ok)
    return DIALOG_RESPONSE_YES;
  if (ret == statuses.cancel || ret == statuses.esc)
    return DIALOG_RESPONSE_NO;
  return ret;
}

void dialog_init(dialog_t * const dialog) {
  /*
   * dialog allows its return values to be changed by env vars (!?)
   * we have to take this into account when reacting to its status codes
   */
  dialog->statuses.cancel = get_status_code(DIALOG_CANCEL);
  dialog->statuses.error = get_status_code(DIALOG_ERROR);
  dialog->statuses.esc = get_status_code(DIALOG_ESC);
  dialog->statuses.extra = get_status_code(DIALOG_EXTRA);
  dialog->statuses.help = get_status_code(DIALOG_HELP);
  dialog->statuses.item_help = get_status_code(DIALOG_ITEM_HELP);
  dialog->statuses.ok = get_status_code(DIALOG_OK);
}

void dialog_free(dialog_t * const dialog) {
}

int dialog_choose(
    dialog_t * const dialog,
    const char **items, const size_t items_len,
    const size_t pos,
    const char *title, const char *format, ...)
{
  char pos_str[8];
  snprintf(pos_str, sizeof(pos_str), "%zu", pos+1);

  format_msg(tmp_buf, format);
  const char * const args_prefix[] = {
      "dialog",
      "--backtitle", BACKTITLE,
      "--title", title,
      "--default-item", pos_str,
      "--menu", tmp_buf, "0", "0", "10"
  };
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

  if (ret < 0)
    return ret;
  if (ret == dialog->statuses.cancel || ret == dialog->statuses.esc)
    return DIALOG_RESPONSE_CANCEL;
  if (ret == dialog->statuses.error)
    return -1;

  return strtol(buf, NULL, 10) - 1;
}

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
      default_ ? "--clear" : "--defaultno",
      "--yesno", tmp_buf, "0", "0",
      NULL
  };

  return check_ret(run("dialog", args), dialog->statuses);
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
      "--msgbox", format, "0", "0",
      NULL
  };

  return check_ret(run("dialog", args), dialog->statuses);
}

int dialog_view_file(
    dialog_t * const dialog,
    const char * title,
    const char * filepath)
{
  const char * args[] = {
      "dialog",
      "--backtitle", BACKTITLE,
      "--title", title,
      "--textbox", filepath, "0", "0",
      NULL
  };

  return check_ret(run("dialog", args), dialog->statuses);
}

int dialog_clear(dialog_t * const dialog) {
  static const char * args[] = {
      "dialog",
      "--clear",
      NULL
  };

  return check_ret(run("dialog", args), dialog->statuses);
}

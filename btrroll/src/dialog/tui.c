#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dialog/tui.h>
#include <macros.h>
#include <run.h>

#define BACKTITLE "btrroll 1.0.0"

typedef struct {
  int cancel,
      error,
      esc,
      extra,
      help,
      item_help,
      ok;
} dialog_statuses_t;

#define DIALOG_CANCEL 1
#define DIALOG_ERROR -1
#define DIALOG_ESC 255
#define DIALOG_EXTRA 3
#define DIALOG_HELP 2
#define DIALOG_ITEM_HELP 2
#define DIALOG_OK 0

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

bool dialog_tui_available() {
  const char *args[] = { "which", "dialog", NULL };
  return !run_pipe("which", args, NULL, -1, NULL, -1);
}

void dialog_tui_init(dialog_backend_t * const backend) {
  backend->choose = dialog_tui_choose;
  backend->confirm = dialog_tui_confirm;
  backend->ok = dialog_tui_ok;
  backend->clear = dialog_tui_clear;

  /*
   * dialog allows its return values to be changed by env vars (!?)
   * we have to take this into account when reacting to its status codes
   */
  dialog_statuses_t *statuses = malloc(sizeof(dialog_statuses_t));

  statuses->cancel = get_status_code(DIALOG_CANCEL);
  statuses->error = get_status_code(DIALOG_ERROR);
  statuses->esc = get_status_code(DIALOG_ESC);
  statuses->extra = get_status_code(DIALOG_EXTRA);
  statuses->help = get_status_code(DIALOG_HELP);
  statuses->item_help = get_status_code(DIALOG_ITEM_HELP);
  statuses->ok = get_status_code(DIALOG_OK);

  backend->data = (void*)statuses;
  backend->free = NULL;
}

int dialog_tui_choose(
    void * const data,
    const char **items, const size_t items_len,
    const size_t pos,
    const char *title, const char *msg)
{
  char pos_str[8];
  snprintf(pos_str, sizeof(pos_str), "%zu", pos+1);

  const char * const ARGS_PREFIX[] = {
      "dialog",
      "--backtitle", BACKTITLE,
      "--title", title,
      "--default-item", pos_str,
      "--menu", msg, "0", "0", "10"
  };
  const size_t args_len = 2*items_len + lenof(ARGS_PREFIX) + 1;
  const char **args = malloc(sizeof(char *) * args_len);

  memcpy(args, ARGS_PREFIX, sizeof(ARGS_PREFIX));

  const size_t tag_len = 8; // TODO
  for (size_t i = 0; i < items_len; i += 1) {
    char *tag = malloc(tag_len);
    snprintf(tag, tag_len, "%zu", i+1);
    args[lenof(ARGS_PREFIX) + i*2] = tag;
    args[lenof(ARGS_PREFIX) + i*2 + 1] = items[i];
  }

  args[args_len - 1] = NULL;

  static const size_t BUF_LEN = 1024;
  char buf[BUF_LEN];

  const int ret = run_pipe("dialog", args, NULL, 0, buf, BUF_LEN);

  // Only some elements of `args` are heap-allocated
  for (size_t i = 0; i < items_len; i += 1)
    free((char*)args[lenof(ARGS_PREFIX) + i*2]);
  free(args);

  const dialog_statuses_t * const statuses = (dialog_statuses_t*) data;
  if (ret < 0)
    return ret;
  if (ret == statuses->cancel || ret == statuses->esc)
    return DIALOG_RESPONSE_CANCEL;
  if (ret == statuses->error)
    return -1;

  return strtol(buf, NULL, 10) - 1;
}

int dialog_tui_confirm(
    void * const data,
    bool default_,
    const char *title, const char *msg)
{
  // The use of --clear here is a hack to avoid dynamic arg allocation
  // It is basically a no-op that can stand in when --defaultno is not needed
  const char * ARGS[] = {
      "dialog",
      "--backtitle", BACKTITLE,
      "--title", title,
      default_ ? "--clear" : "--defaultno",
      "--yesno", msg, "0", "0",
      NULL
  };

  const int ret = run("dialog", ARGS);
  const dialog_statuses_t * const statuses = (dialog_statuses_t*) data;

  if (ret == statuses->ok)
    return DIALOG_RESPONSE_YES;
  if (ret == statuses->cancel || ret == statuses->esc)
    return DIALOG_RESPONSE_NO;
  return ret;
}

int dialog_tui_ok(
    void * const data,
    const char *title, const char *msg)
{
  const char * ARGS[] = {
      "dialog",
      "--backtitle", BACKTITLE,
      "--title", title,
      "--msgbox", msg, "0", "0",
      NULL
  };

  const int ret = run("dialog", ARGS);
  const dialog_statuses_t * const statuses = (dialog_statuses_t*) data;

  if (ret == statuses->ok)
    return DIALOG_RESPONSE_YES;
  if (ret == statuses->cancel || ret == statuses->esc)
    return DIALOG_RESPONSE_NO;
  return ret;
}

void dialog_tui_clear(void * const data) {
  static const char * ARGS[] = {
      "dialog",
      "--clear",
      NULL
  };
  run("dialog", ARGS);
}

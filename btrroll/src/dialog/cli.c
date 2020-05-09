#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include <dialog/cli.h>
#include <run.h>

void dialog_cli_init(dialog_backend_t * const backend) {
  backend->choose = dialog_cli_choose;
  backend->confirm = dialog_cli_confirm;
  backend->ok = dialog_cli_ok;
  backend->view_file = dialog_cli_view_file;
  backend->clear = dialog_cli_clear;

  backend->data = NULL;
  backend->free = NULL;
}

int dialog_cli_choose(
    void * const data,
    const char **items, const size_t items_len,
    const size_t pos,
    const char *title, const char *msg)
{
  int ret;
  char *line = NULL;
  size_t n = 0;

  if (!msg || !items || items_len == 0 || items_len > INT_MAX)
    return -EINVAL;

  for (;;) {
    printf("%s\n\n", msg);

    for (size_t i = 0; i < items_len; ++i) {
      printf("%zu.\t%s\n", i+1, items[i]);
    }

    printf("\nYour choice: ");

    ret = getline(&line, &n, stdin);

    if (ret < 0) {
      break;
    } else if (ret == 1) {
      puts("Input cannot be empty.");
      continue;
    }

    char *endptr;
    long choice = strtol(line, &endptr, 10);
    choice -= 1;

    if (*endptr != '\n' || choice < 0) {
      puts("Input must be a positive integer.");
    } else if (choice > INT_MAX || choice >= items_len) {
      puts("Input value is too large.");
    } else {
      ret = choice;
      break;
    }
  }

  free(line);
  return ret;
}

int dialog_cli_confirm(
    void * const data,
    bool default_,
    const char *title, const char *msg)
{
  int ret;
  char *line = NULL;
  size_t n = 0;
  const char * const yn = default_ ? "Y/n" : "y/N";

  for (;;) {
    printf("%s (%s)\n", msg, yn);

    ret = getline(&line, &n, stdin);

    if (ret < 0) {
      break;
    } else if (ret == 1) {
      ret = default_ ? 1 : 0;
      break;
    } else if (ret == 2) {
      const char c = line[0];
      if (c == 'y' || c == 'Y') {
        ret = 1;
        break;
      } else if (c == 'n' || c == 'N') {
        ret = 0;
        break;
      }
    }

    puts("Invalid input!");
  }

  free(line);
  return ret;
}

int dialog_cli_ok(
    void * const data,
    const char *title, const char *msg)
{
  int ret;
  char *line = NULL;
  size_t n = 0;

  puts(msg);
  printf("\nPress any key to continue.");

  ret = getline(&line, &n, stdin);
  free(line);

  return (ret < 0) ? ret : 0;
}

int dialog_cli_view_file(
    void * const data,
    const char * title,
    const char * filepath)
{
  const char * args[] = {
      "less", filepath,
      NULL
  };

  return run("less", args);
}

int dialog_cli_clear(void * const data) {
  return 0;
}

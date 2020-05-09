#ifndef __DIALOG_BACKEND_H__
#define __DIALOG_BACKEND_H__

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>

#define DIALOG_RESPONSE_NO 0
#define DIALOG_RESPONSE_YES 1
#define DIALOG_RESPONSE_CANCEL INT_MIN

struct dialog_backend;
typedef struct dialog_backend dialog_backend_t;

struct dialog_backend {
  /* Choose one item from a list. The list can have at most INT_MAX elements,
   * which should be _way_ more than enough.
   *
   * Returns:
   *   n (0-indexed) if the n-th item was selected
   *   DIALOG_CANCELLED if the user cancelled (if applicable)
   *   -1 on error
   */
  int (*choose)(void * const data,
                const char **items, const size_t items_len,
                const size_t pos,
                const char *title, const char *msg);

  /* Yes/no prompt
   *
   * Returns:
   *   1 on yes
   *   0 on no
   *   default_ on empty response
   *  -1 on error
   */
  int (*confirm)(void * const data,
                 bool default_,
                 const char *title, const char *msg);

  /* Info prompt with an OK button
   *
   * Returns 0, or -1 on error
   */
  int (*ok)(void * const data, const char *title, const char *msg);

  /* Display the contents of a file in a less-like interface
   *
   * Returns 0, or -1 on error
   */
  int (*view_file)(void * const data, const char *title, const char *filepath);

  /* Clears the screen
   */
  int (*clear)(void * const data);

  /* Deallocates memory
   */
  void (*free)();

  /* User data */
  void *data;
};

int dialog_choose(
    const dialog_backend_t * const backend,
    const char **items, const size_t items_len,
    const size_t pos,
    const char *title, const char *format, ...);

int dialog_confirm(
    const dialog_backend_t * const backend,
    bool default_,
    const char *title, const char *format, ...);

int dialog_ok(
    const dialog_backend_t * const backend,
    const char *title, const char *format, ...);

int dialog_view_file(
    const dialog_backend_t * const backend,
    const char * title,
    const char *filepath);

int dialog_clear(
    const dialog_backend_t * const backend);

void dialog_free(
    const dialog_backend_t * const backend);

#endif

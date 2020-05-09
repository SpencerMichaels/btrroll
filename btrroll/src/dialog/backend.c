#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <dialog/backend.h>

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


int dialog_choose(
    const dialog_backend_t * const backend,
    const char **items, const size_t items_len,
    const size_t pos,
    const char *title, const char *format, ...)
{
  if (!backend || !items || items_len == 0 || items_len > INT_MAX ||
      pos >= items_len || !title || !format)
  {
    errno = EINVAL;
    return -1;
  }

  format_msg(tmp_buf, format);
  return backend->choose(backend->data, items, items_len, pos, title, tmp_buf);
}

int dialog_confirm(
    const dialog_backend_t * const backend,
    bool default_,
    const char *title, const char *format, ...)
{
  if (!backend || !title || !format) {
    errno = EINVAL;
    return -1;
  }

  format_msg(tmp_buf, format);
  return backend->confirm(backend->data, default_, title, tmp_buf);
}

int dialog_ok(
    const dialog_backend_t * const backend,
    const char *title, const char *format, ...)
{
  if (!backend || !title || !format) {
    errno = EINVAL;
    return -1;
  }

  format_msg(tmp_buf, format);
  return backend->ok(backend->data, title, tmp_buf);
}

int dialog_view_file(
    const dialog_backend_t * const backend,
    const char *title,
    const char *filepath)
{
  if (!backend || !title || !filepath) {
    errno = EINVAL;
    return -1;
  }

  return backend->view_file(backend->data, title, filepath);
}

int dialog_clear(
    const dialog_backend_t * const backend)
{
  if (!backend) {
    errno = EINVAL;
    return -1;
  }

  return backend->clear(backend->data);
}

void dialog_free(const dialog_backend_t * const backend) {
  if (!backend)
    return;

  if (backend->free)
    backend->free(backend->data);
  free(backend->data);
}

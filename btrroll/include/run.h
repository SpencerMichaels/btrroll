#ifndef __RUN_H__
#define __RUN_H__

/* Run an external program
 * 
 * If a buf is non-null and its length is non-zero, stderr/out will be
 * redirected into it. If a buf is NULL and its length zero, stderr/out
 * will not be redirected. If a buf is NULL and its length nonzero,
 * stderr/out will be redirected but ignored.
 *
 * Returns:
 *   status of the process, if run sucessfully
 *   -x on failure:
 *     if errno == 0, x is a signal that caused the procss to stop/terminate
 *     if errno != 0, x == -errno
 */
int run_pipe(
    char *program, const char **args,
    char *outbuf, const ssize_t outbuf_len,
    char *errbuf, const ssize_t errbuf_len);

static inline int run(char *program, const char **args) {
  return run_pipe(program, args, NULL, 0, NULL, 0);
}

#endif

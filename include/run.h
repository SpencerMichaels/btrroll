#ifndef __RUN_H__
#define __RUN_H__

/* Run an external program, optionally piping its stdout/err to buffers
 * 
 * If a buf is non-null and its length is non-zero, stdout/err will be
 * redirected into it. If a buf is NULL and its length zero, stderr/out
 * will not be redirected. If a buf is NULL and its length nonzero,
 * stdout/err will be redirected but ignored.
 *
 * Returns:
 *   status of the process, if run sucessfully
 *   -x on failure: the termination/stop signal (TODO: can be either)
 *   -1 on unexpected error; see errno
 */
int run_pipe(
    char *program, const char **args,
    char *outbuf, const ssize_t outbuf_len,
    char *errbuf, const ssize_t errbuf_len);

// The same as above, but with stderr/out left untouched
static inline int run(char *program, const char **args) {
  return run_pipe(program, args, NULL, 0, NULL, 0);
}

#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <run.h>

static int read_into_buf(const int fd, char *buf, size_t len) {
  if (!buf)
    return -EINVAL;

  while (len > 0) {
    ssize_t count = read(fd, buf, len);
    if (count < 0) {
      if (errno == EINTR) {
        continue;
      } else {
        perror("read");
        return -errno;
      }
    } else if (count == 0) {
      break;
    } else {
      buf += count;
      len -= count;
    }
  }
  return 0;
}

int run_pipe(
    char *program, const char **args,
    char *outbuf, const ssize_t outbuf_len,
    char *errbuf, const ssize_t errbuf_len)
{
  int stdout_fd[2],
      stderr_fd[2];

  if (pipe(stdout_fd) < 0 || pipe(stderr_fd) < 0) {
    perror("pipe");
    return -errno;
  }

  pid_t pid = fork();

  if (pid < 0) { // Error
    perror("fork");
    return -errno;
  } else if (pid == 0) { // In child
    // dialog outputs selections to stderr, so we pipe that back to the parent
    if ((outbuf_len && dup2(stdout_fd[1], STDOUT_FILENO) < 0) ||
        (errbuf_len && dup2(stderr_fd[1], STDERR_FILENO) < 0))
    {
      perror("dup2");
      return -errno;
    }
    close(stdout_fd[0]);
    close(stdout_fd[1]);
    close(stderr_fd[0]);
    close(stderr_fd[1]);

    // TODO: "discards qualifiers in nested pointer types"
    const char * empty_args[] = { program, NULL };
    if (execvp(program, (char * const *) (args ? args : empty_args)) < 0) {
      perror("execvp");
      exit(errno);
    }
  }
  
  // In parent
  close(stdout_fd[1]); // Don't need to write to the child
  close(stderr_fd[1]);

  if (outbuf && outbuf_len > 0)
    read_into_buf(stdout_fd[0], outbuf, outbuf_len);
  if (errbuf && errbuf_len > 0)
    read_into_buf(stderr_fd[0], errbuf, errbuf_len);

  int status;
  pid = wait(&status);
  close(stdout_fd[0]); // Child exited; done reading
  close(stderr_fd[0]);

  if (pid < 0) {
    perror("wait");
    return -errno;
  }

  errno = 0;
  if (WIFEXITED(status)) {
    return WEXITSTATUS(status);
  }
  if (WIFSIGNALED(status)) {
    return -WTERMSIG(status);
  }
  if (WIFSTOPPED(status)) {
    return -WSTOPSIG(status);
  }
  return -1;
}

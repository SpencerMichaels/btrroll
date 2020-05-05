#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <run.h>

// Read from an FD into a buffer; used to pipe into the parent proc's buffer
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
        return -1;
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

  // Create pipes for stdout/err
  if (pipe(stdout_fd) < 0 || pipe(stderr_fd) < 0) {
    perror("pipe");
    return -1;
  }

  pid_t pid = fork();

  if (pid < 0) { // Failed to fork
    perror("fork");
    return -1;
  } else if (pid == 0) { // In child
    // Send stdout/err to the previously-established pipes if the corresponding
    // length variable is nonzero. Not checking for a NULL buffer is intentional
    // here; that is handled later.
    if ((outbuf_len && dup2(stdout_fd[1], STDOUT_FILENO) < 0) ||
        (errbuf_len && dup2(stderr_fd[1], STDERR_FILENO) < 0))
    {
      perror("dup2");
      return -errno;
    }

    // The child doesn't need any of these FDs
    close(stdout_fd[0]);
    close(stdout_fd[1]);
    close(stderr_fd[0]);
    close(stderr_fd[1]);

    // Run the original program
    const char * empty_args[] = { program, NULL };
    if (execvp(program, (char * const *) (args ? args : empty_args)) < 0) {
      perror("execvp");
      exit(errno);
    }
  }
  
  // In parent
  // Don't need to write to the child; can close these FDs
  close(stdout_fd[1]);
  close(stderr_fd[1]);

  // Read stdout/err into their buffers, if non-NULL and of nonzero length
  if (outbuf && outbuf_len > 0)
    read_into_buf(stdout_fd[0], outbuf, outbuf_len);
  if (errbuf && errbuf_len > 0)
    read_into_buf(stderr_fd[0], errbuf, errbuf_len);

  int status;
  pid = wait(&status);

  // Child exited; done reading
  close(stdout_fd[0]);
  close(stderr_fd[0]);

  if (pid < 0) {
    perror("wait");
    return -1;
  }

  if (WIFEXITED(status))
    return WEXITSTATUS(status);
  if (WIFSIGNALED(status))
    return -WTERMSIG(status);
  if (WIFSTOPPED(status))
    return -WSTOPSIG(status);
  return -1;
}

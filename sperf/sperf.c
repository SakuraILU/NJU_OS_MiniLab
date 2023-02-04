#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <readline/readline.h>
#include <assert.h>

// int main(int argc, char *argv[])
// {
//   char *exec_argv[] = {
//       "strace",
//       "ls",
//       NULL,
//   };
//   char *exec_envp[] = {
//       "PATH=/bin",
//       NULL,
//   };
//   execve("strace", exec_argv, exec_envp);
//   perror(argv[0]);
//   exit(EXIT_FAILURE);
// }
extern char **environ;

void child(int argc, char *exec_argv[]);
void parent();

int fd[2];

int main(int argc, char *argv[])
{
  pipe(fd);

  int ret = fork();
  if (ret == 0)
  {
    close(fd[0]);
    close(STDERR_FILENO);
    dup(fd[1]);
    close(fd[1]);

    child(argc, argv + 1);
  }
  else if (ret > 0)
  {
    close(fd[1]);
    parent();
  }
  else
  {
    close(fd[0]);
    close(fd[1]);
    perror("fork");
    assert(0);
  }
}

void child(int argc, char *exec_argv[])
{
  char *argv[2 + argc + 1] = {NULL};
  argv[0] = "strace";
  argv[1] = "--syscall-time";
  for (int i = 0; i < ARLEN(exec_argv); ++i)
    argv[i + 2] = exec_argv[i];

  execve(argv[0], argv, environ);
  perror(argv[0]);
  exit(EXIT_FAILURE);
}

void parent()
{
  char *syscall_info;
  while ((syscall_info = readline()) != NULL)
  {
    printf("%s\n", syscall_info);
  }
  printf("END");
}
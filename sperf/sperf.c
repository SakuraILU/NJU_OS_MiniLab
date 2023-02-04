#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
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

void child(int argc, char *exec_argv[]);
void parent();

extern char **environ;
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

    child(argc, argv);
  }
  else if (ret > 0)
  {
    close(fd[1]);
    close(STDIN_FILENO);
    dup(fd[0]);
    close(fd[0]);

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
  char *argv[2 + argc + 1];
  argv[0] = "strace";
  argv[1] = "--syscall-time";
  for (int i = 1; i < argc; ++i)
  {
    argv[i + 1] = exec_argv[i];
  }
  printf("%s %s %s %s\n", argv[0], argv[1], argv[2], argv[3]);

  char **env = {getenv("PATH")};
  execve("strace", argv, env);
  perror(argv[0]);
  exit(EXIT_FAILURE);
}

void parent()
{
  char *sysinfo = NULL;
  size_t len = 0;
  while (getline(&sysinfo, &len, stdin) != -1)
  {
    printf("%s\n", sysinfo);
  }
  free(sysinfo);
  printf("END\n");
}
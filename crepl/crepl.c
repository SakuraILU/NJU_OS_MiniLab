#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <assert.h>

#define CMD_MXSIZE 4096
#define PATH_MXSIZE 4096
#define ERR_MSG_LEN 4096

int src_fd = 0;
char org_tmp_name[PATH_MXSIZE - 38] = "/tmp/src.XXXXXX";
char src[PATH_MXSIZE];
char dst[PATH_MXSIZE];
char ndst = 0;
char *compile_cmd[] = {
    "gcc",
#ifdef __x86_64__
    "-m64",
#else
    "-m32",
#endif
    "--shared",
    "-fPIC",
    "-O2",
    "-W",
    src,
    "-o",
    dst,
};

int cmd_src_fd = 0;
char cmd_src[28] = "/tmp/wrapper_cmd.XXXXXX";
char cmd_dst[28];

typedef enum cmdtype
{
  COMPILE,
  RUN,
} Cmdtype;

void compile_libso(char *code);
char *set_dstname(int ndst);
void wrap_cmd(char *cmd);

static __attribute__((constructor)) void constructor()
{
  src_fd = mkstemp(org_tmp_name);
  sprintf(src, "%s.c", org_tmp_name);
  rename(org_tmp_name, src);

  char cmd_org_tmp_name[] = "/tmp/wrap_cmd.XXXXXX";
  cmd_src_fd = mkstemp(cmd_org_tmp_name);
  sprintf(cmd_dst, "%s.so", cmd_org_tmp_name);
  sprintf(cmd_src, "%s.c", cmd_org_tmp_name);
  rename(cmd_org_tmp_name, cmd_src);
}

static __attribute__((destructor)) void destructor()
{
  unlink(src);
  close(src_fd);
  for (int i = 0; i < ndst; ++i)
  {
    set_dstname(i);
    unlink(dst);
  }

  unlink(cmd_src);
  close(src_fd);
  unlink(cmd_dst);
}

int main(int argc, char *argv[])
{
  static char line[CMD_MXSIZE];
  printf("create tmp file %s", src);

  while (1)
  {
    printf("crepl> ");
    fflush(stdout);
    if (!fgets(line, sizeof(line), stdin))
      break;
    printf("Got %zu chars.\n", strlen(line)); // ??
    char head[4];
    memset(head, 0, 4);
    sscanf(line, " %3c", head);

    Cmdtype cmd_type = COMPILE;
    if (strcmp(head, "int") != 0)
      cmd_type = RUN;

    int fd[2];
    pipe(fd);
    if (fork() == 0)
    {
      close(fd[0]);
      dup2(fd[1], STDERR_FILENO);
      close(fd[1]);

      if (cmd_type == RUN)
        wrap_cmd(line);

      compile_libso(line);
    }
    else
    {
      close(fd[1]);

      int wstatus = 0;
      wait(&wstatus);

      bool compile_success = WIFEXITED(wstatus) && (WEXITSTATUS(wstatus) == 0);
      if (!compile_success)
      {
        char err_msg[ERR_MSG_LEN];
        printf("Compile Error:\n");
        read(fd[0], err_msg, ERR_MSG_LEN);
        printf("%s\n", err_msg);
      }
    }

    ndst++;
  }
}

void compile_libso(char *code)
{
  lseek(src_fd, 0, SEEK_SET);
  int remain = strlen(code);
  while (remain > 0)
  {
    int cnt = write(src_fd, code, remain);
    assert(cnt != -1);
    remain -= cnt;
  }

  set_dstname(ndst);

  execvp(compile_cmd[0], compile_cmd);
  perror(compile_cmd[0]);
  exit(EXIT_FAILURE);
}

char *set_dstname(int ndst)
{
  sprintf(dst, "%s_dst_%d.so", org_tmp_name, ndst);
  return dst;
}

void wrap_cmd(char *cmd)
{
  sprintf(cmd, "int wrap_fun(){return %s}", cmd);
}
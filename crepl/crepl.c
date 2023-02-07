#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <dlfcn.h>
#include <signal.h>
#include <assert.h>

#define CMD_MXSIZE 4096
#define PATH_MXSIZE 128
#define ERR_MSG_LEN 4096

typedef int (*wrap_fun_t)();

FILE *src_f = NULL;
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
    "-shared",
    "-fPIC",
    "-O2",
    "-W",
    src,
    "-o",
    dst,
};

typedef enum cmdtype
{
  COMPILE,
  RUN,
} Cmdtype;

void compile_libso(char *code);
void set_dstname(int ndst);
void wrap_cmd(char *cmd);

void sig_handler(int sig)
{
  exit(EXIT_SUCCESS);
}

static __attribute__((constructor)) void constructor()
{
  int src_fd = mkstemp(org_tmp_name);
  sprintf(src, "%s.c", org_tmp_name);
  rename(org_tmp_name, src);
  close(src_fd);

  signal(SIGINT, sig_handler);
}

static __attribute__((destructor)) void destructor()
{
  unlink(src);
  for (int i = 0; i < ndst; ++i)
  {
    set_dstname(i);
    unlink(dst);
  }
}

int main(int argc, char *argv[])
{
  static char line[CMD_MXSIZE];
  // printf("create tmp file %s", src);

  while (1)
  {
    src_f = fopen(src, "w+");

    printf("crepl> ");
    fflush(stdout);
    if (!fgets(line, sizeof(line), stdin))
      break;
    line[strlen(line) - 1] = 0;
    // printf("Got %zu chars.\n", strlen(line)); // ??

    set_dstname(ndst);

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
      if (compile_success)
      {
        if (fork() == 0)
        {
          // printf("load %s\n", dst);
          if (cmd_type == RUN)
          {
            void *dl_handler = dlopen(dst, RTLD_LAZY | RTLD_LOCAL);
            wrap_fun_t wrap_fun = dlsym(dl_handler, "wrap_fun");
            // printf("solve success %p\n", wrap_fun);
            printf("( %s ) == %d\n", line, wrap_fun());
            dlclose(dl_handler);
            unlink(dst);
            ndst--;
          }
          else
          {
            dlopen(dst, RTLD_NOW | RTLD_GLOBAL);
          }
        }

        int wstatus = 0;
        wait(&wstatus);
      }
      else
      {
        char err_msg[ERR_MSG_LEN];
        printf("Compile Error:\n");
        read(fd[0], err_msg, ERR_MSG_LEN);
        printf("%s\n", err_msg);
      }
    }

    fclose(src_f);
    ndst++;
  }
}

void compile_libso(char *code)
{
  fwrite(code, 1, strlen(code), src_f);
  fread(code, 1, strlen(code), src_f);

  set_dstname(ndst);
  // printf("%s\n", code);
  execvp(compile_cmd[0], compile_cmd);
  perror(compile_cmd[0]);
  exit(EXIT_FAILURE);
}

void set_dstname(int ndst)
{
  sprintf(dst, "%s_dst_%d.so", org_tmp_name, ndst);
}

void wrap_cmd(char *cmd)
{
  char tmp[CMD_MXSIZE];
  strcpy(tmp, cmd);
  sprintf(cmd, "int wrap_fun(){ return %s; }", tmp);
}
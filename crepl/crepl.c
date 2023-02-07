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
#define NFUN (1e+25 - 1)

typedef int (*wrap_fun_t)();

FILE *src_f = NULL;
// 24 stands for 24 digits num and 8 stands for "_dst_.so" in the end
char org_tmp_name[PATH_MXSIZE - 24 - 8] = "/tmp/src.XXXXXX";
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
    "-W", // ignore all warnings
    src,
    "-o",
    dst,
};

typedef enum cmdtype
{
  COMPILE,
  RUN,
} Cmdtype;

int fd[2];

void child(char *cmd, Cmdtype cmd_type);
void parent(char *cmd, Cmdtype cmd_type);
void compile_libso(char *code);
void wrap_code(char *cmd);
void set_dstname(int ndst);
Cmdtype get_cmdtype(char *cmd);

void sig_handler(int sig)
{
  // ctrl+c的默认中断函数导致的退出不会出发destructor,
  // 而exit退出是会触发的，所以默认的可能不是采用的exit(),
  // 这里覆盖一下，为了保证ctrl+c时也能删除创建的临时文件
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
  static char cmd[CMD_MXSIZE];

  while (1)
  {
    src_f = fopen(src, "w+");

    printf("crepl> ");
    fflush(stdout);
    if (!fgets(cmd, sizeof(cmd), stdin))
      break;
    cmd[strlen(cmd) - 3] = 0; // 取出末尾的'\n'

    set_dstname(ndst);

    Cmdtype cmd_type = get_cmdtype(cmd);

    pipe(fd);
    if (fork() == 0)
    {
      close(fd[0]);
      dup2(fd[1], STDERR_FILENO);
      close(fd[1]);

      child(cmd, cmd_type);
    }
    else
    {
      close(fd[1]);

      parent(cmd, cmd_type);

      close(fd[0]);
    }

    fclose(src_f);
    ndst++;
  }
}

void child(char *cmd, Cmdtype cmd_type)
{
  if (cmd_type == RUN)
    wrap_code(cmd);

  compile_libso(cmd);
}

void parent(char *cmd, Cmdtype cmd_type)
{
  int wstatus = 0;
  wait(&wstatus);

  bool compile_success = WIFEXITED(wstatus) && (WEXITSTATUS(wstatus) == 0);
  if (compile_success)
  {

    if (cmd_type == RUN)
    {
      void *dl_handler = dlopen(dst, RTLD_LAZY | RTLD_LOCAL);
      wrap_fun_t wrap_fun = dlsym(dl_handler, "wrap_fun");
      if (wrap_fun == NULL)
        return;

      printf("( %s ) == %d\n", cmd, wrap_fun());
      dlclose(dl_handler);
      unlink(dst);
      ndst--;
    }
    else
    {
      dlopen(dst, RTLD_LAZY | RTLD_GLOBAL);
    }
  }
  else
  {
    char err_msg[ERR_MSG_LEN];
    printf("Compile Error:\n");
    read(fd[0], err_msg, ERR_MSG_LEN);
    printf("%s\n", err_msg);
  }
}

void compile_libso(char *code)
{
  fwrite(code, 1, strlen(code), src_f);
  fread(code, 1, strlen(code), src_f);

  set_dstname(ndst);
  execvp(compile_cmd[0], compile_cmd);
  perror(compile_cmd[0]);
  exit(EXIT_FAILURE);
}

void wrap_code(char *cmd)
{
  char tmp[CMD_MXSIZE];
  strcpy(tmp, cmd);
  sprintf(cmd, "int wrap_fun(){ return %s; }", tmp);
}

void set_dstname(int ndst)
{
  assert(ndst < NFUN);
  sprintf(dst, "%s_dst_%d.so", org_tmp_name, ndst);
}

Cmdtype get_cmdtype(char *cmd)
{
  char head[4];
  memset(head, 0, 4);
  sscanf(cmd, " %3c", head);

  Cmdtype cmd_type = COMPILE;
  if (strcmp(head, "int") != 0)
    cmd_type = RUN;

  return cmd_type;
}
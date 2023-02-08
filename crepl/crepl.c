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

#define PATH_MXSIZE 128
#define CMD_MXSIZE 4096
#define ERR_MSG_LEN 4096
#define NLIBSO (1e+25 - 1)

typedef int (*wrap_fun_t)();

FILE *src_f = NULL;
// 24 stands for 24 digits num and 8 stands for "_dst_.so" in the end
char org_tmp_name[PATH_MXSIZE - 24 - 8] = "/tmp/src.XXXXXX";
char src[PATH_MXSIZE]; // append ".c" behind org_tmp_name
char dst[PATH_MXSIZE]; // append "_dst_%d.so" beind org_tmp_name, %d is ndst
char ndst = 0;         // the num of dst (libso)
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

int fd[2]; // file desriptors of pipe

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
    // 用"w"或者"w+"打开会Truncate  file  to zero length or create text file for writing.
    // 如果直接用write的话，上一次写入的内容不会清空，所以多次写入src.c的话会让代码变成乱七八糟
    // 的。。。无法编译，所以每次写入前必须清空文件！
    src_f = fopen(src, "w+");

    printf("crepl> ");
    fflush(stdout);
    if (!fgets(cmd, sizeof(cmd), stdin))
      break;
    cmd[strlen(cmd) - 1] = 0; // 去掉cmd末尾带的'\n'，fgets是会读入\n的

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
      // 有点离谱的是dlsym解析符号失败后会直接退出程序然后扔一个error: [undefined symbol:xxx]
      // 为了容忍输入失误，把符号解析和运行的任务扔给一个子进程去做好了，而且这部分和父进程之间
      // 也不需要什么通信
      if (fork() == 0)
      {
        // 运行一次之后就不用了，所以用RTLD_LOCAL方式加载
        void *dl_handler = dlopen(dst, RTLD_LAZY | RTLD_LOCAL);
        wrap_fun_t wrap_fun = dlsym(dl_handler, "wrap_fun");
        if (wrap_fun == NULL)
          return;

        printf("( %s ) == %d\n", cmd, wrap_fun());

        dlclose(dl_handler);

        // 该子进程没有被execve()覆盖，exit()的话会调用destructor...提前删除掉文件
        // 可能会出现bug，例如删除掉了src.xxxxxx.c之后，mkstemp又可以分配一个同名的
        // src.xxxxxx咯，要是哪儿个进程被分配到生成的这个temp文件，还恰好rename了个
        // 后缀.c，可能就会有些问题，当然最好只是rename失败hhhh，而不是相互覆写
        //
        // 而采用_exit()的话，不会执行用户层面注册的一些析构函数(ateixt()注册或者__attribute__((destructor))等等 )，
        // 直接进内核让OS终止进程。其实相当与exit()就是对_exit()包装了一下，先执行一些用户的析构函数
        _exit(EXIT_SUCCESS);
      }
      wait(&wstatus);

      // 运行一次之后就不用了，删掉该动态库
      unlink(dst);
      ndst--;
    }
    else
    {
      // flag RTLD_GLOBAL 非常重要，必须GLOBAL才能被其他动态库解析到
      // 也就是动态加载器只会解析当前动态库以及其他用RTLD_GLOBAL方式加载的动态库
      // 如果不标记GLOBAL的话，后面的int wrap_fun()将无法解析之前加载进来的动态库了
      void *dl_handler = dlopen(dst, RTLD_LAZY | RTLD_GLOBAL);
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

  set_dstname(ndst);
  execvp(compile_cmd[0], compile_cmd);
  perror(compile_cmd[0]);
  _exit(EXIT_FAILURE);
}

void wrap_code(char *cmd)
{
  char tmp[CMD_MXSIZE];
  strcpy(tmp, cmd);
  sprintf(cmd, "int wrap_fun(){ return %s; }", tmp);
}

void set_dstname(int ndst)
{
  assert(ndst < NLIBSO);
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
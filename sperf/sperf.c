#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <regex.h>
#include <assert.h>
#include <stdbool.h>

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

#define SYSNAME_MSIZE 64
#define SYSTIME_MSIZE 32

typedef struct sysinfo
{
  char name[SYSNAME_MSIZE];
  float total_time;
  struct sysinfo *next;
} Sysinfo;

Sysinfo *head, *tail;

static __attribute__((constructor)) void init()
{
  tail = head = (Sysinfo *)malloc(sizeof(Sysinfo));
  memset(head, 0, sizeof(Sysinfo));
}

void add_sysinfo(char *sys_name, float sys_time)
{
  // printf("add sys %s time %f \n", sys_name, sys_time);
  Sysinfo *itr = head->next;
  while (itr != NULL)
  {
    if (strcmp(itr->name, sys_name) == 0)
    {
      itr->total_time += sys_time;
      return;
    }
    itr = itr->next;
  }

  tail->next = (Sysinfo *)malloc(sizeof(Sysinfo));
  tail = tail->next;
  strcpy(tail->name, sys_name);
  tail->total_time = sys_time;
  tail->next = NULL;
}

static void child(int argc, char *exec_argv[]);
static void parent();
void parse_sysinfo();

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

static void child(int argc, char *exec_argv[])
{
  char *argv[2 + argc + 1];
  argv[0] = "strace";
  argv[1] = "--syscall-time";
  for (int i = 1; i < argc; ++i)
  {
    argv[i + 1] = exec_argv[i];
  }

  char *envp[] = {
      "PATH=/bin:/usr/bin",
      NULL,
  };

  execve("/usr/bin/strace", argv, envp);
  perror(argv[0]);
  exit(EXIT_FAILURE);
}

static void parent()
{
  parse_sysinfo();
}

void parse_sysinfo()
{
  regex_t reg;                                        // 定义一个正则实例
  const char *pat = "(^[^\\(]+)|(<[0-9]+\\.[0-9]+)>"; // 定义模式串
  regcomp(&reg, pat, REG_EXTENDED);                   // 编译正则模式串

  char *sysinfo = NULL;
  size_t len = 0;
  while (getline(&sysinfo, &len, stdin) != -1)
  {
    char sysname[SYSNAME_MSIZE] = {0};
    char systime_str[SYSTIME_MSIZE] = {0};
    float systime = 0;

    const size_t nmatch = 1; // 定义匹配结果最大允许数
    regmatch_t pmatch[1];    // 定义匹配结果在待匹配串中的下标范围

    int status = regexec(&reg, sysinfo, nmatch, pmatch, 0); // 匹配他
    if (status == REG_NOMATCH)
    { // 如果没匹配上
      assert(false);
    }
    else if (status == 0)
    { // 如果匹配上了
      strncpy(sysname, sysinfo + pmatch[0].rm_so, pmatch[0].rm_eo - pmatch[0].rm_so);
    }

    sysinfo = sysinfo + pmatch[0].rm_eo;
    status = regexec(&reg, sysinfo, nmatch, pmatch, 0); // 匹配他
    if (status == REG_NOMATCH)
    { // 如果没匹配上
      if (getline(&sysinfo, &len, stdin) != -1)
        break;
      else
        status = regexec(&reg, sysinfo, nmatch, pmatch, 0); // 匹配他
    }

    if (status == 0)
    { // 如果匹配上了
      systime = atof(strncpy(systime_str, sysinfo + pmatch[0].rm_so + 1, pmatch[0].rm_eo - pmatch[0].rm_so - 2));
    }
    else
      assert(false);

    add_sysinfo(sysname, systime);
  }
}

void sort_sysinfo()
{
}
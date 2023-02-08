#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <regex.h>
#include <time.h>
#include <assert.h>

#define SYSNAME_MSIZE 24
#define SYSTIME_MSIZE 24
#define PATH_MSIZE 128
#define INTERVAL 2

#define eprintf(...) fprintf(stderr, ##__VA_ARGS__)

extern char **environ;

typedef struct sysinfo
{
  char name[SYSNAME_MSIZE];
  float total_time;
  struct sysinfo *next;
} Sysinfo;
Sysinfo *dummy, *tail;
float sys_total_time = 0;

regex_t name_reg, time_reg;
const char *name_pat = "^[a-z_][a-z0-9_]*"; // 定义模式串
const char *time_pat = "<[0-9]+\\.[0-9]+>"; // 定义模式串

static __attribute__((constructor)) void constructor()
{
  tail = dummy = (Sysinfo *)malloc(sizeof(Sysinfo));
  memset(dummy, 0, sizeof(Sysinfo));

  regcomp(&name_reg, name_pat, REG_EXTENDED); // 编译正则模式串
  regcomp(&time_reg, time_pat, REG_EXTENDED); // 编译正则模式串
}

static __attribute__((destructor)) void destuctor()
{
  regfree(&name_reg);
  regfree(&time_reg);
}

static void add_sysinfo(char *sys_name, float sys_time)
{
  // printf("add sys %s time %f \n", sys_name, sys_time);
  sys_total_time += sys_time;
  Sysinfo *itr = dummy->next;
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

static void print_sysinfo()
{
  eprintf("===SYSCALL USAGE PERCENT===\n");
  printf("TIME: %d\n", INTERVAL);
  Sysinfo *itr = dummy->next;
  while (itr != NULL)
  {
    eprintf("%-24s %7.3f%%\n", itr->name, itr->total_time / sys_total_time * 100);
    itr = itr->next;
  }
}

static void child(int argc, char *exec_argv[]);
static void parent();
void parse_sysinfo();
void sort_sysinfo();
static Sysinfo *quick_sort(Sysinfo *dummy);

int fd[2];
bool sperf_over = false;

int main(int argc, char *argv[])
{
  pipe(fd);

  int ret = fork();
  if (ret == 0)
  {
    close(fd[0]);

    dup2(fd[1], STDERR_FILENO);
    close(fd[1]);

    close(STDOUT_FILENO);
    fopen("/dev/null", "w");

    child(argc, argv);
  }
  else if (ret > 0)
  {
    close(fd[1]);
    dup2(fd[0], STDIN_FILENO);
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

static void my_execvp(char *cmd, char *argv[])
{
  char *paths = getenv("PATH");
  printf("%s\n", paths);
  char *path = strtok(paths, ":");
  printf("%s\n", path);
  while (true)
  {
    char real_path[PATH_MSIZE];
    strcat(real_path, path);
    strcat(real_path, "/");
    strcat(real_path, cmd);
    strcpy(argv[0], real_path);
    execve(real_path, argv, environ);
    path = strtok(NULL, ":");
  }
}

// 很离谱的是-O1优化时，编译器会把argv初始化或者赋NULL值都给删掉，
// 可能是因为在这段函数里没有使用arg[argc+2]？但是argv[0,1..]也没
// 使用过却没有被优化...有点无语。
// 定位问题的过程：在读strace top时，tracelog里输出了unknown option未知参数，strace echo good
// 时输出了good xxxxxxxxxxxxxxxxxxx（乱码), strace ls时, tracelog里输出了"cannot access 'xxx(乱码)'"....etc
//
// 怀疑argv赋值出了问题，gdb了一下，发现无论是memset(argv, NULL, argc+3) 还是
// argv[argc+2] = NULL均无效，说明这两个东西很可能被编译优化掉了，然后把-O1改成-O0就
// 完全正常。。。volatile只作用与数组名，而不保证里面的数组元素...所以干脆直接
// 就把这段函数给禁止优化了，改成了O0级，终于正常了T^T
#pragma GCC push_options
#pragma GCC optimize("O0")
static void child(int argc, char *exec_argv[])
{
  char *argv[2 + argc + 1];
  argv[0] = "strace";
  argv[1] = "--syscall-time";
  for (int i = 1; i < argc; ++i)
  {
    argv[i + 1] = exec_argv[i];
  }
  argv[argc + 2] = NULL;
  printf("here\n");
  my_execvp("strace", argv);
  perror(argv[0]);
  exit(EXIT_FAILURE);
}
#pragma GCC pop_options

static void parent()
{
  while (!sperf_over)
  {
    parse_sysinfo();
    sort_sysinfo();
    print_sysinfo();
  }
}

void parse_sysinfo()
{
  char *sysinfo = NULL;
  size_t len = 0;

  time_t stime = time(NULL), interval = 0;
  while (getline(&sysinfo, &len, stdin) != -1)
  {
    char sysname[SYSNAME_MSIZE] = {0};
    char systime_str[SYSTIME_MSIZE] = {0};
    float systime = 0;

    const size_t nmatch = 1; // 定义匹配结果最大允许数
    regmatch_t pmatch[1];    // 定义匹配结果在待匹配串中的下标范围

    // printf("%s", sysinfo);
    int status = regexec(&name_reg, sysinfo, nmatch, pmatch, 0); // 匹配他
    if (status == REG_NOMATCH)
      // 如果没匹配上
      continue;
    else if (status == 0)
      // 如果匹配上了
      strncpy(sysname, sysinfo + pmatch[0].rm_so, pmatch[0].rm_eo - pmatch[0].rm_so);

    int offset = pmatch[0].rm_eo;
    status = regexec(&time_reg, sysinfo + offset, nmatch, pmatch, 0); // 匹配他
    if (status == 0)
      // 如果匹配上了
      systime = atof(strncpy(systime_str, sysinfo + offset + pmatch[0].rm_so + 1, pmatch[0].rm_eo - pmatch[0].rm_so - 2));
    else
    {
      while (getline(&sysinfo, &len, stdin) != -1)
      {
        status = regexec(&time_reg, sysinfo, nmatch, pmatch, 0); // 匹配他
        if (status == REG_NOMATCH)
          continue;

        else if (status == 0)
        { // 如果匹配上了
          systime = atof(strncpy(systime_str, sysinfo + pmatch[0].rm_so + 1, pmatch[0].rm_eo - pmatch[0].rm_so - 2));
          break;
        }
        else
          assert(false);
      }
    }

    add_sysinfo(sysname, systime);

    interval = time(NULL) - stime;
    if (interval >= INTERVAL)
      break;
  }

  if (interval < INTERVAL)
    sperf_over = true;

  free(sysinfo);
}

void sort_sysinfo()
{
  dummy->next = quick_sort(dummy->next);
}

static Sysinfo *quick_sort(Sysinfo *head)
{
  assert(head != NULL);

  Sysinfo *mark = head;
  if (mark->next == NULL)
    return mark;

  Sysinfo *head1 = NULL, *head2 = NULL, *itr1 = NULL, *itr2 = NULL;
  while (mark->next != NULL)
  {
    Sysinfo *tmp = mark->next;
    mark->next = tmp->next;
    if (tmp->total_time >= mark->total_time)
    {
      if (head1 == NULL)
        head1 = itr1 = tmp;
      else
      {
        itr1->next = tmp;
        itr1 = itr1->next;
      }
    }
    else
    {
      if (head2 == NULL)
        head2 = itr2 = tmp;
      else
      {
        itr2->next = tmp;
        itr2 = itr2->next;
      }
    }
    tmp->next = NULL;
  }

  if (head1 == NULL)
  {
    assert(head2 != NULL);
    mark->next = quick_sort(head2);
    return mark;
  }

  head1 = quick_sort(head1);
  itr1 = head1;
  while (itr1->next != NULL)
  {
    itr1 = itr1->next;
  }
  itr1->next = mark;

  if (head2 == NULL)
    return head1;
  else
    mark->next = quick_sort(head2);

  return head1;
}
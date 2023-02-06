#include <stdbool.h>
#include <stdio.h>  /* for printf */
#include <stdlib.h> /* for exit */
#include <getopt.h>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>
#include <assert.h>

#define debug(...)                  \
  do                                \
  {                                 \
    fprintf(stderr, ##__VA_ARGS__); \
    assert(0);                      \
  } while (0)

#define eprintf(...) fprintf(stderr, ##__VA_ARGS__);
#define MAX(a, b) (((a) >= (b)) ? (a) : (b))

#define PROCNAME_LEN 64
#define PATH_LEN 128
#define MAXDEPTH 24

char version_info[] = "pstree (PSmisc) 23.4\n\
Copyright (C) 1993-2020 Werner Almesberger and Craig Small\n\
\n\
PSmisc comes with ABSOLUTELY NO WARRANTY.\n\
This is free software, and you are welcome to redistribute it under\n\
the terms of the GNU General Public License.\n\
For more information about these matters, see the files named COPYING.";

typedef struct proc
{
  uint pid;
  struct childptr *childs_head;
  char name[PROCNAME_LEN];

  struct proc *next;
} Proc;

typedef struct childptr
{
  struct proc *child;
  struct childptr *next;
} Childptr;

Proc *dummy = NULL, *tail = NULL;

int idents[MAXDEPTH];
int depth = 0;

static __attribute__((constructor)) void constructor()
{
  tail = dummy = malloc(sizeof(Proc));
  memset(dummy, 0, sizeof(Proc));
}

void add_proc(const char *name, uint pid, uint ppid)
{
  tail->next = malloc(sizeof(Proc));
  tail = tail->next;
  memset(tail, 0, sizeof(Proc));
  tail->pid = pid;
  strcpy(tail->name, name);

  if (ppid == 0)
    return;

  Proc *itr = dummy->next;
  Proc *child = tail, *parent = NULL;
  while (itr != NULL)
  {
    if (itr->pid == ppid)
      parent = itr;
    itr = itr->next;
  }
  assert(child != NULL && parent != NULL);

  Childptr *nchild = malloc(sizeof(Childptr));
  nchild->child = child;
  nchild->next = NULL;

  Childptr *child_itr = parent->childs_head;
  if (child_itr == NULL)
  {
    parent->childs_head = nchild;
    return;
  }

  while (child_itr->next != NULL)
  {
    child_itr = child_itr->next;
  }
  child_itr->next = nchild;
}

void traverse_proc()
{
  Proc *itr = dummy->next;
  while (itr != NULL)
  {
    printf("%-20s  %d\n", itr->name, itr->pid);
    Childptr *child_itr = itr->childs_head;
    while (child_itr != NULL)
    {
      printf("\t %-20s  %d\n", child_itr->child->name, child_itr->child->pid);
      child_itr = child_itr->next;
    }
    itr = itr->next;
    /* code */
  }
}

bool show_version = false;
bool need_sort = false;
bool show_pids = false;

void parse_args(int argc, char *argv[]);
void print_tree(Proc *proc);
void print_ident();

int main(int argc, char *argv[])
{
  parse_args(argc, argv);

  if (show_version)
  {
    eprintf("%s\n", version_info);
    return 0;
  }

  DIR *proc_dir = opendir("/proc");
  struct dirent *dir_itr;
  while ((dir_itr = readdir(proc_dir)) != NULL)
  {
    int pid = 0;
    if ((pid = atoi(dir_itr->d_name)) == 0)
      continue;
    // printf("%s\n", dir_itr->d_name);
    char fpath[PATH_LEN];
    sprintf(fpath, "/proc/%d/stat", pid);
    FILE *file = fopen(fpath, "r+");
    // printf("%s\n", fpath);
    assert(file != NULL);

    uint proc_pid = 0, proc_ppid = 0;
    char proc_name[PROCNAME_LEN];
    char proc_status;
    fscanf(file, "%d %s %c %d", &proc_pid, proc_name, &proc_status, &proc_ppid);
    proc_name[strlen(proc_name) - 1] = 0;
    char *proc_name_pure = proc_name + 1;
    assert(pid == proc_pid);
    // printf("proc name %s, proc pid %d, proc status %c, proc ppid %d\n", proc_name_pure, proc_pid, proc_status, proc_ppid);
    add_proc(proc_name_pure, proc_pid, proc_ppid);
  }

  // traverse_proc();
  print_tree(dummy->next);

  return 0;
}

void parse_args(int argc, char *argv[])
{
  while (true)
  {
    int option_index = 0;
    static struct option long_options[] = {
        {"show-pids", no_argument, 0, 'p'},
        {"numeric-sort", no_argument, 0, 'n'},
        {"version", no_argument, 0, 'v'},
        {0, 0, 0, 0}};

    int opt = getopt_long(argc, argv, "pnv", long_options, &option_index);
    if (opt == -1)
      break;

    switch (opt)
    {
    case 'p':
      printf("option p\n");
      break;

    case 'n':
      need_sort = true;
      break;

    case 'v':
      show_version = true;
      break;

    default:
      debug("?? getopt returned character code 0%o ??\n", opt);
    }
  }
}

void print_tree(Proc *proc)
{
  printf("%s-+-", proc->name);
  Childptr *child_itr = proc->childs_head;
  depth++;
  idents[depth] = idents[depth - 1] + strlen(proc->name) + 3;

  while (child_itr != NULL)
  {
    print_tree(child_itr->child);

    child_itr = child_itr->next;
    if (child_itr != NULL)
    {
      printf("\n");
      print_ident();
    }
  }
  depth--;
}

void print_ident()
{
  for (int i = 1; i <= depth; ++i)
  {
    int nspace = idents[i] - idents[i - 1];
    if (i == 1)
      nspace = idents[1] - 2;
    for (int j = 0; j < nspace; ++j)
      printf(" ");
    printf("| ");
  }
}
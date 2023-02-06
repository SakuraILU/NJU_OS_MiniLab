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

struct idents
{
  int pos;
  bool need_print;
} idents[MAXDEPTH];
int depth = 0;

static __attribute__((constructor)) void constructor()
{
  tail = dummy = malloc(sizeof(Proc));
  memset(dummy, 0, sizeof(Proc));

  for (int i = 0; i < MAXDEPTH; ++i)
  {
    idents[i].pos = 0;
    idents[i].need_print = false;
  }
}

static void add_proc(const char *name, uint pid, uint ppid)
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

bool show_version = false;
bool need_sort = false;
bool show_pids = false;

static void parse_args(int argc, char *argv[]);
static void print_tree(Proc *proc);
static void sort_child_by_name();

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
    if (show_pids)
    {
      char pid_append[MAXNAMLEN];
      sprintf(pid_append, "(%d)", proc_pid);
      strcat(proc_name, pid_append);
    }
    assert(pid == proc_pid);
    // printf("proc name %s, proc pid %d, proc status %c, proc ppid %d\n", proc_name_pure, proc_pid, proc_status, proc_ppid);
    add_proc(proc_name_pure, proc_pid, proc_ppid);
  }

  sort_child_by_name();

  print_tree(dummy->next);
  printf("\n");

  return 0;
}

static void parse_args(int argc, char *argv[])
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
      show_pids = true;
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

static void print_ident(bool is_last_child)
{
  for (int i = 1; i <= depth; ++i)
  {
    for (int j = 0; j < idents[i].pos - idents[i - 1].pos - 2; ++j)
    {
      printf(" ");
    }

    if (!idents[i].need_print)
      printf("  ");
    else if (i < depth)
      printf("│ ");
    else
    {
      if (!is_last_child)
        printf("├─");
      else
      {
        printf("└─");
        idents[i].need_print = false;
      }
    }
  }
}

static void print_tree(Proc *proc)
{
  printf("%s", proc->name);

  Childptr *child_itr = proc->childs_head;
  if (child_itr == NULL)
    return;

  depth++;
  idents[depth].pos = idents[depth - 1].pos + strlen(proc->name) + 3;
  if (child_itr->next != NULL)
  {
    printf("─┬─");
    idents[depth].need_print = true;
  }
  else
  {
    printf("───");
    idents[depth].need_print = false;
  }

  while (true)
  {
    print_tree(child_itr->child);

    child_itr = child_itr->next;
    if (child_itr != NULL)
    {
      printf("\n");
      print_ident(child_itr->next == NULL);
    }
    else
    {
      break;
    }
  }
  depth--;
}

static Childptr *quick_sort(Childptr *head)
{
  // assert(head != NULL);
  if (head == NULL)
    return NULL;

  if (head != NULL && head->next == NULL)
    return head;

  Childptr *mark = head;

  Childptr *head1 = NULL, *head2 = NULL, *tail1 = NULL, *tail2 = NULL;
  while (mark->next != NULL)
  {
    Childptr *tmp = mark->next;
    mark->next = tmp->next;
    tmp->next = NULL;
    printf("cmp %s, %s\n", tmp->child->name, mark->child->name);
    if (strcmp(tmp->child->name, mark->child->name) < 0)
    {
      if (head1 == NULL)
        tail1 = head1 = tmp;
      else
      {
        tail1->next = tmp;
        tail1 = tail1->next;
      }
    }
    else
    {
      if (head2 == NULL)
        tail2 = head2 = tmp;
      else
      {
        tail2->next = tmp;
        tail2 = tail2->next;
      }
    }
  }
  assert(head1 != NULL && head2 != NULL);
  printf("here\n");

  if (head1 == NULL)
  {
    mark->next = quick_sort(head2);
    return mark;
  }

  tail1 = head1 = quick_sort(head1);
  while (tail1->next != NULL)
  {
    tail = tail->next;
  }
  tail1->next = mark;
  if (head2 != NULL)
    mark->next = quick_sort(head2);

  return head1;
}

static void sort_child_by_name()
{
  Proc *itr = dummy->next;
  while (itr != NULL)
  {
    // printf("sort...\n");
    itr->childs_head = quick_sort(itr->childs_head);
    itr = itr->next;
  }
}
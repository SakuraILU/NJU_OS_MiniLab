#include <stdbool.h>
#include <stdio.h>  /* for printf */
#include <stdlib.h> /* for exit */
#include <getopt.h>
#include <assert.h>

#define debug(...)                  \
  do                                \
  {                                 \
    fprintf(stderr, ##__VA_ARGS__); \
    assert(0);                      \
  } while (0)

#define eprintf(...) fprintf(stderr, ##__VA_ARGS__);

char version_info[] = "pstree (PSmisc) 23.4\n \
Copyright (C) 1993-2020 Werner Almesberger and Craig Small\n";

void parse_args(int argc, char *argv[]);

bool show_version = false;
bool need_sort = false;
bool show_pids = false;

int main(int argc, char *argv[])
{
  parse_args(argc, argv);

  if (show_version)
    eprintf("%s", version_info);

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
      printf("option n\n");
      break;

    case 'v':
      printf("option v\n");
      break;

    default:
      debug("?? getopt returned character code 0%o ??\n", opt);
    }
  }
}

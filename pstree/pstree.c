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

int main(int argc, char *argv[])
{
  // for (int i = 0; i < argc; i++)
  // {
  //   assert(argv[i]);
  //   printf("argv[%d] = %s\n", i, argv[i]);
  // }
  // assert(!argv[argc]);
  int c;
  while (true)
  {
    int option_index = 0;
    static struct option long_options[] = {
        {"show-pids", no_argument, 0, 'p'},
        {"numeric-sort", no_argument, 0, 'n'},
        {"version", no_argument, 0, 'v'},
        {0, 0, 0, 0}};

    c = getopt_long(argc, argv, "pnv", long_options, &option_index);
    if (c == -1)
      break;

    switch (c)
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
      debug("?? getopt returned character code 0%o ??\n", c);
    }

    return 0;
  }
}

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define CMD_MXSIZE 4096
#define PATH_MXSIZE 4096

int src_fd = 0;
char src[PATH_MXSIZE] = "/tmp/src_code.XXXXXX";
char dst[PATH_MXSIZE];

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
    "-Werror",
    src,
    "-o",
    dst,
};

static __attribute__((constructor)) void constructor()
{
  src_fd = mkstemp(src);
}

static __attribute__((destructor)) void destructor()
{
  int ret = unlink(src);
  close(src_fd);
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
    {
      break;
    }
    printf("Got %zu chars.\n", strlen(line)); // ??
  }
}

void compile_liso()
{
}

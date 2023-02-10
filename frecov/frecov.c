#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;

// Copied from the manual
typedef struct fat32hdr
{
  u8 BS_jmpBoot[3];
  u8 BS_OEMName[8];
  u16 BPB_BytsPerSec;
  u8 BPB_SecPerClus;
  u16 BPB_RsvdSecCnt;
  u8 BPB_NumFATs;
  u16 BPB_RootEntCnt;
  u16 BPB_TotSec16;
  u8 BPB_Media;
  u16 BPB_FATSz16;
  u16 BPB_SecPerTrk;
  u16 BPB_NumHeads;
  u32 BPB_HiddSec;
  u32 BPB_TotSec32;
  u32 BPB_FATSz32;
  u16 BPB_ExtFlags;
  u16 BPB_FSVer;
  u32 BPB_RootClus;
  u16 BPB_FSInfo;
  u16 BPB_BkBootSec;
  u8 BPB_Reserved[12];
  u8 BS_DrvNum;
  u8 BS_Reserved1;
  u8 BS_BootSig;
  u32 BS_VolID;
  u8 BS_VolLab[11];
  u8 BS_FilSysType[8];
  u8 __padding_1[420];
  u16 Signature_word;
} __attribute__((packed)) Fat32hdr;
Fat32hdr *hdr = NULL;

typedef struct fat32shortDent
{
  u8 DIR_Name[11];
  u8 DIR_Attr;
  u8 DIR_NTRes;
  u8 DIR_CrtTimeTenth;
  u16 DIR_CrtTime;
  u16 DIR_CrtDate;
  u16 DIR_LastAccDate;
  u16 DIR_FstClusHI;
  u16 DIR_WrtTime;
  u16 DIR_WrtDate;
  u16 DIR_FstClusLO;
  u32 DIR_FileSize;
} __attribute__((packed)) Fat32shortDent;

#define ATTR_READ_ONLY 0x01
#define ATTR_HIDDEN 0x02
#define ATTR_SYSTEM 0x04
#define ATTR_VOLUME_ID 0x08
#define ATTR_DIRECTORY 0x10
#define ATTR_ARCHIVE 0x20

#define LAST_LONG_ENTRY 0x40 // last long name entry bit indicator,
                             // ldir.Ord == LAST_LONG_ENTRY >> this is the last long name entry

#define DIR_CUR_FOLLOW_FREE 0x0 // the current and all the following directory entry is free (available)
#define DIR_CUR_FREE 0xe5       // the current directory entry is free (available)
#define DIR_INVALID 0x20        // names cannot start with a space character

typedef struct fat32longDent
{
  u8 DIR_Ord;
  u8 DIR_Name1[10];
  u8 Dir_Attr;
  u8 Dir_Type; // Must be set to 0.
  u8 Dir_Chksum;
  u8 Dir_Nmae2[12];
  u16 Dir_FstClusLO; // Must be set to 0.
  u8 Dir_Name3[4];
} __attribute__((packed)) Fat32longDent;

typedef union fat32dent
{
  Fat32shortDent sdir;
  Fat32longDent ldir;
} Fat32dent;

void scan();
void *map_disk(const char *fname);

int main(int argc, char *argv[])
{
  if (argc < 2)
  {
    fprintf(stderr, "Usage: %s fs-image\n", argv[0]);
    exit(1);
  }

  setbuf(stdout, NULL);

  assert(sizeof(Fat32hdr) == 512);      // defensive
  assert(sizeof(Fat32shortDent) == 32); // defensive
  assert(sizeof(Fat32longDent) == 32);  // defensive

  // map disk image to memory
  struct fat32hdr *hdr = map_disk(argv[1]);

  // TODO: frecov
  scan(hdr);

  // file system traversal
  munmap(hdr, hdr->BPB_TotSec32 * hdr->BPB_BytsPerSec);
}

void *map_disk(const char *fname)
{
  int fd = open(fname, O_RDWR);

  if (fd < 0)
  {
    perror(fname);
    goto release;
  }

  off_t size = lseek(fd, 0, SEEK_END);
  if (size == -1)
  {
    perror(fname);
    goto release;
  }

  hdr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
  if (hdr == (void *)-1)
  {
    goto release;
  }

  close(fd);

  if (hdr->Signature_word != 0xaa55 ||
      hdr->BPB_TotSec32 * hdr->BPB_BytsPerSec != size)
  {
    fprintf(stderr, "%s: Not a FAT file image\n", fname);
    goto release;
  }
  return hdr;

release:
  if (fd > 0)
  {
    close(fd);
  }
  exit(1);
}

void *cluster_to_addr(int n)
{
  // RTFM: Sec 3.5 and 4 (TRICKY)
  assert(n >= hdr->BPB_RootClus);
  u32 DataSec = hdr->BPB_RsvdSecCnt + hdr->BPB_NumFATs * hdr->BPB_FATSz32;
  DataSec += (n - hdr->BPB_RootClus) * hdr->BPB_SecPerClus;
  return ((char *)hdr) + DataSec * hdr->BPB_BytsPerSec;
}

bool is_dir(Fat32shortDent *dir)
{
  if (dir->DIR_Name[0] == 0)
    return false;
}

void scan()
{
  u32 byte_per_clus = hdr->BPB_SecPerClus * hdr->BPB_BytsPerSec;
  printf("root clus %d, %x\n", hdr->BPB_RootClus, byte_per_clus);
  // char *itr = cluster_to_addr(hdr->BPB_RootClus);
  // char *itr_end = (char *)hdr + hdr->BPB_TotSec32 * hdr->BPB_BytsPerSec;
  // for (; itr < itr_end; itr += byte_per_clus)
  // {
  //   Fat32shortDent *dir = (Fat32shortDent *)itr;

  //   if (is_dir(dir))
  //   {

  //   }
  // }
}

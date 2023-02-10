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
  u8 DIR_Name[11];     // related macro of Name[0]: DIR_CUR_FOLLOW_FREE, DIR_CUR_FREE, DIR_INVALID
  u8 DIR_Attr;         // related macro: ATTR_xxxx
  u8 DIR_NTRes;        // Reserved. Must be set to 0.
  u8 DIR_CrtTimeTenth; // Component of the file creation time. Count of tenths of a second. Valid range is: 0 <= DIR_CrtTimeTenth <= 199 u16 DIR_CrtTime;
  u16 DIR_CrtTime;
  u16 DIR_CrtDate;
  u16 DIR_LastAccDate;
  u16 DIR_FstClusHI; // High word of first data cluster number for file / directory described by this entry.u16 DIR_WrtTime;
  u16 DIR_WrtTime;
  u16 DIR_WrtDate;
  u16 DIR_FstClusLO; // Low word of first data cluster number for file / directory described by this entry
  u32 DIR_FileSize;  // 32-bit quantity containing size in bytes of file / directory described by this entry.
} __attribute__((packed)) Fat32shortDent;

typedef struct fat32longDent
{
  u8 DIR_Ord; // The order of this entry in the sequence of long name directory entries (each containing components of the long file name)
              // 1. The first member of a set has an LDIR_Ord value of 1.
              // 2. The LDIR_Ord value for each subsequent entry must contain a monotonically increasing value.
              // 3. The Nth (last) member of the set must contain a value of (N | LAST_LONG_ENTRY)
  u8 DIR_Name1[10];
  u8 DIR_Attr; // Must be set to ATTR_LONG_NAME
  u8 DIR_Type; // Must be set to 0.
  u8 DIR_Chksum;
  u8 DIR_Nmae2[12];
  u16 DIR_FstClusLO; // Must be set to 0.
  u8 DIR_Name3[4];
} __attribute__((packed)) Fat32longDent;

#define ATTR_READ_ONLY 0x01
#define ATTR_HIDDEN 0x02
#define ATTR_SYSTEM 0x04
#define ATTR_VOLUME_ID 0x08
#define ATTR_DIRECTORY 0x10
#define ATTR_ARCHIVE 0x20
#define ATTR_LONG_NAME (ATTR_READ_ONLY | ATTR_HIDDEN | ATTR_SYSTEM | ATTR_VOLUME_ID)

#define LAST_LONG_ENTRY 0x40 // last long name entry bit indicator

#define DIR_CUR_FOLLOW_FREE 0x0 // the current and all the following directory entry is free (available)
#define DIR_CUR_FREE 0xe5       // the current directory entry is free (available)
#define DIR_INVALID 0x20        // names cannot start with a space character

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
  printf("%x\n", hdr->BPB_RsvdSecCnt * hdr->BPB_BytsPerSec);
  printf("%x\n", (hdr->BPB_RsvdSecCnt + hdr->BPB_NumFATs * hdr->BPB_FATSz32) * hdr->BPB_BytsPerSec);

  return ((char *)hdr) + DataSec * hdr->BPB_BytsPerSec;
}

bool is_dir(Fat32shortDent *dir)
{
  if (dir->DIR_Name[0] == 0)
    return false;

  int ndent = hdr->BPB_BytsPerSec * hdr->BPB_SecPerClus / sizeof(Fat32shortDent);

  for (int i = 0; i < ndent; ++i)
  {
    if (dir[i].DIR_Name[0] == DIR_INVALID)
      return false;

    if (dir[i].DIR_Name[0] == DIR_CUR_FOLLOW_FREE)
    {
      for (int j = i; j < ndent; ++j)
        if (dir[j].DIR_Name[0] != DIR_CUR_FOLLOW_FREE)
          return false;
      return true;
    }

    if (dir[i].DIR_Attr == ATTR_LONG_NAME)
    {
      Fat32longDent *ldir = (Fat32longDent *)dir;
      if (ldir->DIR_Attr != ATTR_LONG_NAME)
        return false;
      if (ldir->DIR_FstClusLO != 0)
        return false;

      // if (ldir->DIR_Ord != 1)
      //   return false;

      bool is_valid = false;
      int j = i + 1;
      for (; j < ndent; j++)
      {
        if (ldir[j].DIR_Attr != ATTR_LONG_NAME)
          return false;

        if (ldir[j].DIR_Ord <= ldir[j - 1].DIR_Ord)
          return false;

        if (ldir[j].DIR_Ord & LAST_LONG_ENTRY)
        {
          i = j;
          is_valid = true;
          break;
        }
      }

      if (j == ndent)
        return true;

      if (is_valid)
        continue;
      else
        return false;
    }
    else
    {
      if (dir[i].DIR_NTRes != 0)
        return false;

      if (dir[i].DIR_CrtTimeTenth >= 200)
        return false;

      switch (dir[i].DIR_Attr)
      {
      case ATTR_READ_ONLY:
      case ATTR_HIDDEN:
      case ATTR_SYSTEM:
      case ATTR_VOLUME_ID:
      case ATTR_DIRECTORY:
      case ATTR_ARCHIVE:
        break;

      default:
        return false;
      }
    }
  }
  return true;
}

void scan()
{
  // printf("%p\n", (void *)((uintptr_t)cluster_to_addr(hdr->BPB_RootClus) - (uintptr_t)hdr));
  // printf("%p\n", (void *)((uintptr_t)cluster_to_addr(3) - (uintptr_t)hdr));
  char *itr = cluster_to_addr(hdr->BPB_RootClus);
  char *itr_end = (char *)hdr + hdr->BPB_TotSec32 * hdr->BPB_BytsPerSec;
  u32 byte_per_clus = hdr->BPB_SecPerClus * hdr->BPB_BytsPerSec;
  for (; itr < itr_end; itr += byte_per_clus)
  {
    Fat32shortDent *dir = (Fat32shortDent *)itr;

    if (is_dir(dir))
    {
    }
  }
}

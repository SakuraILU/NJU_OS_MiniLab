#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <dirent.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;

#define PATH_MXSIZE 256
#define SHA1SUM_SIZE 64

// Copied from the manual
typedef struct fat32hdr
{
  u8 BS_jmpBoot[3];
  u8 BS_OEMName[8];
  u16 BPB_BytsPerSec;
  u8 BPB_SecPerClus;  // Number of sectors per allocation unit. (Data segment)
  u16 BPB_RsvdSecCnt; // Number of reserved sectors in the reserved region of the volume starting at the first sector of the volume.
  u8 BPB_NumFATs;     // The count of file allocation tables (FATs)
  u16 BPB_RootEntCnt; // For FAT32 volumes, this field must be setto 0.
  u16 BPB_TotSec16;
  u8 BPB_Media;
  u16 BPB_FATSz16;
  u16 BPB_SecPerTrk;
  u16 BPB_NumHeads;
  u32 BPB_HiddSec;
  u32 BPB_TotSec32;
  u32 BPB_FATSz32; // This field is the FAT32 32-bit count of sectors occupiedby one FAT.
  u16 BPB_ExtFlags;
  u16 BPB_FSVer;
  u32 BPB_RootClus; // This is set to the cluster number of the first cluster of the root directory. This value should be 2 or the first usable(not bad) cluster available thereafter.u16 BPB_FSInfo;
  u16 BPB_BkBootSec;
  u8 BPB_Reserved[12];
  u8 BS_DrvNum;
  u8 BS_Reserved1;
  u8 BS_BootSig;
  u32 BS_VolID;
  u8 BS_VolLab[11];
  u8 BS_FilSysType[8];
  u8 __padding_1[420];
  u16 Signature_word; // 0xaa55
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

typedef struct bitmapHdr
{
  u16 Signature_word; // 0x4d42
  u32 BMP_Size;
  u16 BMP_Reserved1;
  u16 BMP_Reserved2;
  u32 BMP_MapOffset;
} __attribute__((packed)) BitmapHdr;

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

char recov_dir[PATH_MXSIZE];

void *ptr_offset(void *ptr);
void scan();
void *map_disk(const char *fname);

int main(int argc, char *argv[])
{
  if (argc < 2)
  {
    fprintf(stderr, "Usage: %s fs-image dst_directory\n", argv[0]);
    exit(1);
  }

  setbuf(stdout, NULL);

  assert(sizeof(Fat32hdr) == 512);      // defensive
  assert(sizeof(Fat32shortDent) == 32); // defensive
  assert(sizeof(Fat32longDent) == 32);  // defensive

  // map disk image to memory
  struct fat32hdr *hdr = map_disk(argv[1]);

  // check directory
  DIR *dir = opendir(argv[2]);
  if (dir == NULL)
  {
    fprintf(stderr, "Erro: dirctory is not exist.\n");
    exit(1);
  }
  closedir(dir);
  strcpy(recov_dir, argv[2]);

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

/**
 * @brief get the address (pointer) of cluster[n]
 *
 * @param n the cluster num
 * @return the adress of cluster[n]
 */
void *cluster_to_addr(int n)
{
  // RTFM: Sec 3.5 and 4 (TRICKY)
  assert(n >= hdr->BPB_RootClus);

  //    clus[0]    clus[1]
  // | Metadata  |  Fats  |       Data      |
  //                      ^
  //                    RootClus (usually 2)
  // Metadata is a special cluster and fat is a special cluster which are not equal to SecPerClus
  // while all the cluster of Data has SecPerClus sectors
  //
  // Metadata has hdr->BPB_RsvdSecCnt sectors
  // Fats has hdr->BPB_NumFATs * hdr->BPB_FATSz32 sectors
  u32 DataSec = hdr->BPB_RsvdSecCnt + hdr->BPB_NumFATs * hdr->BPB_FATSz32;
  DataSec += (n - hdr->BPB_RootClus) * hdr->BPB_SecPerClus;

  return ((char *)hdr) + DataSec * hdr->BPB_BytsPerSec;
}

bool is_dir(Fat32shortDent *dir)
{
  // RTFM: sec6 and sec7

  // if Dir_Name[0] == 0, current and following entries are all free...
  // so, if the first one is Dir_Name[0] == 0.. if the cluster is a dir, it is a totally empty dir
  // useless for recovery
  if (dir->DIR_Name[0] == 0)
    return false;

  int ndent = hdr->BPB_BytsPerSec * hdr->BPB_SecPerClus / sizeof(Fat32shortDent);

  for (int i = 0; i < ndent; ++i)
  {
    if (dir[i].DIR_Name[0] == DIR_INVALID)
      return false;

    if (dir[i].DIR_Name[0] == DIR_CUR_FREE)
      continue;

    if (dir[i].DIR_Name[0] == DIR_CUR_FOLLOW_FREE)
    {
      // check the following entries, they all should be free
      for (int j = i; j < ndent; ++j)
        if (dir[j].DIR_Name[0] != DIR_CUR_FOLLOW_FREE)
          return false;
      return true;
    }

    // a long dir entry:
    // | order N   ...| --|
    // | order N-1 ...| -----> store the long name (each long name entry stores part of the long name)
    // | order 1   ...| --|
    // | short entry  | store short name and many metadata, like DIR_Attr, FstClus, Filesz
    if (dir[i].DIR_Attr == ATTR_LONG_NAME)
    {
      Fat32longDent *ldir = (Fat32longDent *)dir;

      if (ldir[i].DIR_FstClusLO != 0)
        return false;

      // check following long_name_entries
      for (; i < ndent; i++)
      {
        if (ldir[i].DIR_Attr != ATTR_LONG_NAME)
          return false;

        u8 ord = ldir[i].DIR_Ord & (~LAST_LONG_ENTRY);
        // if order descends to 1, it is the last long_name_entry, next one must be its normal(short) entry
        if (ord == 1)
        {
          if (i + 1 < ndent && dir[i + 1].DIR_Attr == ATTR_LONG_NAME)
            return false;
          break;
        }

        // the order is descent in our forward traverse order
        if (i + 1 < ndent && ord <= ldir[i + 1].DIR_Ord)
          return false;
      }
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

/**
 * @brief parse a directory entry
 *
 * @param dir
 * @param remain_dent
 * @param name
 * @param fst_cluse
 * @param filesize
 * @return int
 */
int parse_dir_entry(Fat32shortDent *dir, int remain_dent, char *name, u32 *fst_cluse, u32 *filesize)
{
  if (dir->DIR_Name[0] == DIR_CUR_FOLLOW_FREE)
    return 0;

  if (dir->DIR_Name[0] == DIR_CUR_FREE)
    return 1;

  if (dir->DIR_Attr == ATTR_LONG_NAME)
  {
    Fat32longDent *ldir = (Fat32longDent *)dir;
    u8 len = ldir->DIR_Ord & (~LAST_LONG_ENTRY);

    if (len >= remain_dent)
      return 0;

    if ((ldir->DIR_Ord & LAST_LONG_ENTRY) == 0)
      return len + 1;
    if (dir[len].DIR_Attr == ATTR_DIRECTORY || dir[len].DIR_Attr == ATTR_HIDDEN)
      return len + 1;

    int cur = 0;
    for (int i = len - 1; i >= 0; --i)
    {
      assert(cur < PATH_MXSIZE);

      for (int j = 0; j < 10; j += 2)
        name[cur++] = ldir[i].DIR_Name1[j];
      for (int j = 0; j < 12; j += 2)
        name[cur++] = ldir[i].DIR_Nmae2[j];
      for (int j = 0; j < 4; j += 2)
        name[cur++] = ldir[i].DIR_Name3[j];
    }

    *fst_cluse = (dir[len].DIR_FstClusHI << 16) | dir[len].DIR_FstClusLO;
    *filesize = dir[len].DIR_FileSize;

    return len + 1;
  }
  else
  {
    if (dir->DIR_Attr == ATTR_DIRECTORY || dir->DIR_Attr == ATTR_HIDDEN)
      return 1;

    strcpy(name, (char *)dir->DIR_Name);
    *fst_cluse = (dir->DIR_FstClusHI << 16) | dir->DIR_FstClusLO;
    *filesize = dir->DIR_FileSize;

    return 1;
  }
}

bool is_bmp(BitmapHdr *bmp_hdr)
{
  if (bmp_hdr->Signature_word != 0x4d42)
    return false;

  if (bmp_hdr->BMP_Reserved1 != 0 || bmp_hdr->BMP_Reserved2 != 0)
    return false;

  return true;
}

void parse_bmp(BitmapHdr *bmp_hdr, u32 filesz, char *filename)
{
  if (!is_bmp(bmp_hdr))
    return;

  char recov_path[PATH_MXSIZE];
  sprintf(recov_path, "%s", recov_dir);
  strcat(recov_path, "/");
  strcat(recov_path, filename);
  FILE *f = fopen(recov_path, "w+");

  fwrite(bmp_hdr, 1, filesz, f);
  fclose(f);

  char cmd[PATH_MXSIZE + 8];
  sprintf(cmd, "sha1sum %s", recov_path);
  FILE *pf = popen(cmd, "r");
  if (pf == NULL)
    assert(false);

  char sha1sum_res[SHA1SUM_SIZE];
  fread(sha1sum_res, 1, SHA1SUM_SIZE, pf);
  pclose(pf);
  printf("%s\n", sha1sum_res);
}

void scan()
{
  char *itr = cluster_to_addr(hdr->BPB_RootClus);
  char *itr_end = (char *)hdr + hdr->BPB_TotSec32 * hdr->BPB_BytsPerSec;
  u32 byte_per_clus = hdr->BPB_SecPerClus * hdr->BPB_BytsPerSec;
  for (; itr < itr_end; itr += byte_per_clus)
  {
    Fat32shortDent *dir = (Fat32shortDent *)itr;

    if (is_dir(dir))
    {
      // printf("%p is a dir\n", ptr_offset(dir));
      int remain_dent = byte_per_clus / sizeof(Fat32shortDent);
      char name[PATH_MXSIZE];
      u32 fst_cluse;
      u32 filesz;
      while (remain_dent > 0)
      {
        memset(name, 0, PATH_MXSIZE);
        fst_cluse = filesz = 0;
        int pace = parse_dir_entry(dir, remain_dent, name, &fst_cluse, &filesz);
        if (pace == 0)
          break;

        if (name[0] != 0 && fst_cluse != 0 && filesz != 0)
        {
          // printf("get name %s, fst cluse %d, filesz %d\n", name, fst_cluse, filesz);
          void *bmp_addr = cluster_to_addr(fst_cluse);
          parse_bmp(bmp_addr, filesz, name);
        }

        dir += pace;
        remain_dent -= pace;
      }
    }
  }
}

void *ptr_offset(void *ptr)
{
  return (void *)((uintptr_t)ptr - (uintptr_t)hdr);
}

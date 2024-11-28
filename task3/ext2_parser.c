#include "ext2_parser.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "ext2_fs.h"
#define INODE_TYPE_MASK 0xf000
#define INODE_TYPE_DIR 0x4000

typedef struct {
  int type;
  int inode;
  int fd;
  long long size;
  size_t idx;
  int block_ptrs[3];
  int dir_block[12];
  size_t dire;
  struct ext2_super_block *sb;
} Inode;

typedef struct {
  int num;
  char *name;
} DirEntry;

// function to fill super block by file discriptor from image
int get_super_block(struct ext2_super_block *sb, int fd) {
  const off_t kSBoffset = 1024;
  if (pread(fd, sb, sizeof(*sb), kSBoffset) != sizeof(*sb)) {
    perror("Ошибка чтения суперблока");
    return 1;
  }
  return 0;
}

// SUPER BLOCK GETTERS
int get_block_size(struct ext2_super_block *sb) { return EXT2_BLOCK_SIZE(sb); }

int get_inode_per_group(struct ext2_super_block *sb) {
  return sb->s_inodes_per_group;
}

int get_block_per_group(struct ext2_super_block *sb) {
  return sb->s_blocks_per_group;
}

int get_blocks_count(struct ext2_super_block *sb) { return sb->s_blocks_count; }

int get_major(struct ext2_super_block *sb) { return sb->s_rev_level; }

int get_inode_size(struct ext2_super_block *sb) {
  return get_major(sb) ? sb->s_inode_size : 128;
}

int get_inodes_count(struct ext2_super_block *sb) { return sb->s_inodes_count; }

int get_dir_entry_type(struct ext2_super_block *sb) {
  return 0x002 & sb->s_feature_incompat;
}

int get_num_of_block_groups(struct ext2_super_block *sb) {
  return (get_inodes_count(sb) + get_inode_per_group(sb) - 1) /
         get_inode_per_group(sb);
}

int get_block_group_descriptor_table_size(struct ext2_super_block *sb) {
  return get_num_of_block_groups(sb) * 32;
}
// ~SUPER BLOCK GETTERS

// INODE FUNCIONS
Inode *init_inode(const struct ext2_super_block *sb, const int fd,
                  const int inode_num) {
  Inode *inode = (Inode *)malloc(sizeof(Inode));
  inode->inode = inode_num;
  inode->idx = 0;
  inode->dire = 0;
  inode->sb = sb;
  inode->fd = fd;

  const int kBlockGroup = (inode_num - 1) / get_inode_per_group(sb);
  const int kInodeIdx = (inode_num - 1) % get_inode_per_group(sb);
  const int kInodeTableBlockOffset = 2048 + 32 * kBlockGroup + 8;
  const int kInodeSize = get_inode_size(sb);

  int inode_table_block;

  if (pread(fd, &inode_table_block, sizeof(int), kInodeTableBlockOffset) !=
      sizeof(int)) {
    perror("pread: inode table block");
    free(inode);
    exit(1);
  }

  char *inode_buff = (char *)malloc(sizeof(char) * kInodeSize);
  const int kInodeOffset =
      inode_table_block * kInodeSize + kInodeIdx * get_inode_size(sb);

  if (pread(fd, inode_buff, kInodeSize, kInodeOffset) != kInodeSize) {
    perror("pread: inode buff");
    free(inode);
    free(inode_buff);
    exit(1);
  }

  inode->type = *((short *)(inode_buff + 0));

  const int size_lower = *((int *)(inode_buff + 4));
  const int size_higher = *((int *)(inode_buff + 108));

  inode->size = 0;
  inode->size |= size_higher;
  inode->size |= size_lower;

  memcpy(inode->block_ptrs, inode_buff + 88, 12);
  memcpy(inode->dir_block, inode_buff + 40, 48);

  free(inode_buff);

  return inode;
}

void destroy_inode(Inode *inode) { free(inode); }

int inode_get_type(const Inode *inode) { return INODE_TYPE_MASK & inode->type; }

int inode_get_next_block(const Inode *inode) {
  if (inode->idx * get_block_size(inode->sb)) {
    return 0;
  }
  if (inode->idx < 12) {
    return inode->dir_block[inode->idx];
  }
  const int kBlockSize = get_block_size(inode->sb);
  if (inode->idx < 12 + kBlockSize / 4) {
    int res = 0;
    if (pread(inode->fd, &res, sizeof(int),
              inode->block_ptrs[0] * kBlockSize + inode->idx - 12) !=
        sizeof(int)) {
      perror("pread: inode_get_next_block");
      exit(1);
    }
    return res;
  }
  if (inode->idx < 12 + (kBlockSize / 4 + 1) * kBlockSize / 4) {
    const int kTmp =
        inode->idx - 12 - get_block_group_descriptor_table_size(inode->sb) / 4;
    const int idx1 = kTmp / (kBlockSize / 4);
    const int idx2 = kTmp % (kBlockSize / 4);

    int idx3 = 0;

    if (pread(inode->fd, &idx3, sizeof(int),
              inode->block_ptrs[1] * kBlockSize + idx1) != sizeof(int)) {
      perror("pread: inode_get_next_block");
      exit(1);
    }

    int idx4 = 0;

    if (pread(inode->fd, &idx4, sizeof(int), idx3 * kBlockSize + idx2) !=
        sizeof(int)) {
      perror("pread: inode_get_next_block");
      exit(1);
    }
    return idx4;
  }

  return 0;
}

DirEntry *inode_get_next_dir_entry(const Inode *inode) {
  DirEntry *dire = (DirEntry *)malloc(sizeof(DirEntry));
  const int block = inode_get_next_block(inode);

  if (!block) {
    return NULL;
  }

  const int kBlockSize = get_block_size(inode->sb);
  char buff[sizeof(int) + sizeof(short) + sizeof(char) * sizeof(char)];

  if (kBlockSize - inode->dire < sizeof(buff)) {
    // continue;
  }

  return dire;
}

int inode_print_dir_entry(const Inode *inode) {
  DirEntry *dir = inode_get_next_dir_entry(inode);

  if (!dir) {
    return 0;
  }

  return 0;
}

// ~INODE FUNCTIONS

void print_dir(const struct ext2_super_block *sb, int fd, int inode_num) {
  Inode *inode = init_inode(sb, fd, inode_num);

  if (inode_get_type(inode) != INODE_TYPE_DIR) {
    perror("inode: type mismatch");
    exit(1);
  }

  while (inode_print_dir_entry(inode));

  destroy_inode(inode);
}

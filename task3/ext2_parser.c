#include "ext2_parser.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "ext2_fs.h"
#define INODE_TYPE_MASK 0xf000

#define OT_X 0x001
#define OT_W 0x002
#define OT_R 0x004
#define GR_X 0x008
#define GR_W 0x010
#define GR_R 0x020
#define US_X 0x040
#define US_W 0x080
#define US_R 0x100
#define STIKY_BIT 0x200

#define INODE_TYPE_FIFO 0x1000
#define INODE_TYPE_CHAR_DEV 0x2000
#define INODE_TYPE_DIR 0x4000
#define INODE_TYPE_BLOCK_DEV 0x6000
#define INODE_TYPE_REG_FILE 0x8000
#define INODE_TYPE_SYM_LINK 0xa000
#define INODE_TYPE_SOCKET 0xc000

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

typedef struct {
  size_t size;
  size_t capacity;
  char **tockens;
} Tockens;

void set_permission(char *perm, int type) {
  perm[0] = (type & US_X) ? 'x' : '-';
  perm[1] = (type & US_W) ? 'w' : '-';
  perm[2] = (type & US_R) ? 'r' : '-';
  perm[3] = (type & GR_X) ? 'x' : '-';
  perm[4] = (type & GR_W) ? 'w' : '-';
  perm[5] = (type & GR_R) ? 'r' : '-';
  perm[6] = (type & OT_X) ? 'x' : '-';
  perm[7] = (type & OT_W) ? 'w' : '-';
  perm[8] = (type & GR_R) ? 'r' : '-';
  perm[9] = (type & STIKY_BIT) ? 's' : '-';
}

char get_msg_by_inode_type(int inode_type) {
  if (inode_type == INODE_TYPE_BLOCK_DEV) return 'b';
  if (inode_type == INODE_TYPE_DIR) return 'd';
  if (inode_type == INODE_TYPE_CHAR_DEV) return 'c';
  if (inode_type == INODE_TYPE_FIFO) return 'f';
  if (inode_type == INODE_TYPE_REG_FILE) return '-';
  if (inode_type == INODE_TYPE_SOCKET) return 's';
  if (inode_type == INODE_TYPE_SYM_LINK) return 'l';
  perror("unknown type");
  exit(1);
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

void inode_increment_idx(Inode *inode) { ++(inode->idx); }

void inode_decriment_idx(Inode *inode) { --(inode->idx); }

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

DirEntry *inode_get_next_dir_entry(Inode *inode) {
  DirEntry *dire = (DirEntry *)malloc(sizeof(DirEntry));
  int block = inode_get_next_block(inode);

  if (!block) {
    free(dire);
    return NULL;
  }

  dire->name = NULL;

  const int kBlockSize = get_block_size(inode->sb);
  char buff[sizeof(int) + sizeof(short) + sizeof(char) * sizeof(char)];

  if (kBlockSize - inode->dire < sizeof(buff)) {
    if (pread(inode->fd, buff, kBlockSize - inode->dire,
              inode->dire + block * kBlockSize) != kBlockSize - inode->dire) {
      perror("pread: inode_get_next_dir_entry");
      exit(1);
    }
    inode_increment_idx(inode);
    block = inode_get_next_block(inode);
    if (!block) {
      return NULL;
    }
    if (pread(inode->fd, buff + kBlockSize - inode->dire,
              sizeof(buff) - kBlockSize + inode->dire,
              (0 + block) * kBlockSize) !=
        sizeof(buff) - kBlockSize + inode->dire) {
      perror("pread: inode_get_next_dir_entry");
      exit(1);
    }
  } else {
    if (pread(inode->fd, buff, sizeof(buff),
              inode->dire + block * kBlockSize) != sizeof(buff)) {
      perror("pread: inode_get_next_dir_entry");
      exit(1);
    }
  }

  const int dir_entry_inode = *((int *)(buff + 0));
  const short total_size = *((short *)(buff + 4));
  const char lenght_low = *((char *)(buff + 6));
  const char lenght_top = *((char *)(buff + 7));

  const int len = (!get_dir_entry_type(inode->sb)) ? lenght_top : lenght_low;

  char *name_buff = (char *)malloc(sizeof(char) * len);

  block = inode_get_next_block(inode);
  if (!block) {
    perror("inode_get_next_block: inode_get_next_dir_entry");
    exit(1);
  }

  const int kOffsetOfName = inode->dire + block * kBlockSize;
  if (pread(inode->fd, name_buff, len, kOffsetOfName) != len) {
    perror("pread: inode_get_next_dir_entry");
    exit(1);
  }

  if ((inode->dire + total_size) / kBlockSize) {
    inode_increment_idx(inode);
  }

  inode->dire = (inode->dire + total_size) % kBlockSize;

  dire->num = dir_entry_inode;
  dire->name = name_buff;

  return dire;
}

int destroy_dir_entry(DirEntry *dir_entry) {
  if (!dir_entry) return 0;
  if (dir_entry->name) free(dir_entry->name);
  free(dir_entry);
  return 1;
}

void inode_print_meta(Inode *inode) {
  const int kInodeType = inode_get_type(inode);
  char perms[12];
  perms[11] = '\0';
  perms[0] = get_msg_by_inode_type(kInodeType);
  set_permission(perms + 1, kInodeType);
  printf("%s", perms);
}

int inode_print_dir_entry(Inode *inode) {
  DirEntry *dir = inode_get_next_dir_entry(inode);

  if (!dir || dir->num == 0) {
    return 0;
  }

  Inode *inner_inode = init_inode(inode->sb, inode->fd, dir->num);
  inode_print_meta(inner_inode);
  printf(" %s\n", dir->name);
  const int kDirNum = dir->num;
  destroy_dir_entry(dir);
  destroy_inode(inner_inode);

  return kDirNum;
}

int inode_print_data(Inode *inode) {
  const int kBlockidx = inode_get_next_block(inode);
  const int kBlockSize = get_block_size(inode->sb);

  if (!kBlockidx || inode->idx * kBlockidx > inode->size) {
    return 0;
  }

  const int kToReadLen = (inode->size - inode->idx * kBlockSize < kBlockSize)
                             ? inode->size - inode->idx * kBlockSize
                             : kBlockSize;

  char *buff = (char *)malloc(sizeof(char) * kBlockSize);
  const int kReaded =
      pread(inode->fd, buff, kToReadLen, kBlockidx * kBlockSize);
  if (errno) {
    perror("pread: inode_print_data, errno");
    exit(1);
  }
  if (write(STDOUT_FILENO, buff, kReaded) != kReaded || errno) {
    perror("write: inode_print_data");
    exit(1);
  }
  inode_increment_idx(inode);
  free(buff);
  return kReaded;
}

// ~INODE FUNCTIONS

// SOME FUNCTIONS
Tockens *split(const char *str, const char *delimiter) {
  // Проверка на NULL-указатели
  if (str == NULL || delimiter == NULL) {
    return NULL;
  }

  // Выделение памяти для структуры Tockens
  Tockens *tokens = (Tockens *)malloc(sizeof(Tockens));
  if (tokens == NULL) {
    perror("malloc");
    return NULL;
  }
  tokens->capacity = 1;  // Начальная вместимость - 1 токен
  tokens->size = 0;
  tokens->tockens = (char **)malloc(tokens->capacity * sizeof(char *));
  if (tokens->tockens == NULL) {
    perror("malloc");
    free(tokens);
    return NULL;
  }

  char *token;
  char *str_copy = strdup(
      str);  // Создаем копию строки, чтобы избежать модификации оригинала

  if (str_copy == NULL) {
    perror("strdup");
    free(tokens->tockens);
    free(tokens);
    return NULL;
  }

  token = strtok(str_copy, delimiter);  // Разбиваем строку на токены

  while (token != NULL) {
    // Если массив токенов заполнен, увеличиваем его размер
    if (tokens->size >= tokens->capacity) {
      tokens->capacity *= 2;  // Удваиваем вместимость
      tokens->tockens =
          (char **)realloc(tokens->tockens, tokens->capacity * sizeof(char *));
      if (tokens->tockens == NULL) {
        perror("realloc");
        free(str_copy);
        free(tokens);
        return NULL;
      }
    }

    // Выделяем память для токена и копируем его
    tokens->tockens[tokens->size] = strdup(token);
    if (tokens->tockens[tokens->size] == NULL) {
      perror("strdup");
      for (size_t i = 0; i < tokens->size; i++) {
        free(tokens->tockens[i]);
      }
      free(tokens->tockens);
      free(str_copy);
      free(tokens);
      return NULL;
    }

    tokens->size++;
    token = strtok(NULL, delimiter);
  }

  free(str_copy);  // Освобождаем память, выделенную под копию строки

  return tokens;
}

void destroy_tokens(Tockens *tokens) {
  if (tokens != NULL) {
    for (size_t i = 0; i < tokens->size; i++) {
      free(tokens->tockens[i]);
    }
    free(tokens->tockens);
    free(tokens);
  }
}

// ~SOME FUNCTIONS

// function to fill super block by file discriptor from image
int get_super_block(struct ext2_super_block *sb, int fd) {
  const off_t kSBoffset = 1024;
  if (pread(fd, sb, sizeof(*sb), kSBoffset) != sizeof(*sb)) {
    perror("Ошибка чтения суперблока");
    return 1;
  }
  return 0;
}

void print_dir(const struct ext2_super_block *sb, int fd, int inode_num) {
  Inode *inode = init_inode(sb, fd, inode_num);

  if (inode_get_type(inode) != INODE_TYPE_DIR) {
    perror("inode: type mismatch");
    exit(1);
  }

  while (inode_print_dir_entry(inode));

  destroy_inode(inode);
}

void print_file(const struct ext2_super_block *sb, int fd, int inode_num) {
  Inode *inode = init_inode(sb, fd, inode_num);
  const int kBlockSize = get_block_size(sb);

  while (inode_print_data(inode) == kBlockSize);

  destroy_inode(inode);
}

int get_inode_from_path(const struct ext2_super_block *sb, const int fd,
                        const char *path) {
  const char delimiter[] = "/";
  const int kRootInode = 2;
  Tockens *names = split(path, delimiter);

  Inode *inode = init_inode(sb, fd, kRootInode);

  DirEntry *dir;

  for (size_t i = 0; i < names->size; ++i) {
    do {
      dir = inode_get_next_dir_entry(inode);
    } while (dir->num != 0 && strcmp(dir->name, names->tockens[i]) &&
             (destroy_dir_entry(dir) || 1));

    if (!strcmp(dir->name, names->tockens[i])) {
      destroy_inode(inode);
      destroy_dir_entry(dir);
      return -1;
    }

    destroy_inode(inode);
    inode = init_inode(sb, fd, dir->num);
    if (i != names->size && inode_get_type(inode) != INODE_TYPE_DIR) {
      destroy_inode(inode);
      destroy_dir_entry(dir);
      return -1;
    }
    if (i + 1 != names->size) {
      destroy_dir_entry(dir);
    }
  }

  destroy_inode(inode);
  destroy_tokens(names);
  const int res = dir->num;
  destroy_dir_entry(dir);

  return res;
}

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "ext2_fs.h"
#include "ext2_parser.h"

int main() {
  const char path_to_img[] = "floppy.img";

  const int fd = open(path_to_img, O_RDONLY);

  if (fd < 0) {
    perror("open");
    return 1;
  }

  struct ext2_super_block sb;

  if (get_super_block(&sb, fd)) {
    perror("cant read super block");
    close(fd);
    return 1;
  }

  printf("%d %d %d ", sizeof(sb), EXT2_BLOCK_SIZE(&sb),
         sizeof(struct ext2_group_desc));

  struct ext2_group_desc group;

  if (get_group_desc(&group, fd, EXT2_BLOCK_SIZE(&sb))) {
    perror("cant read super block");
    close(fd);
    return 1;
  }

  printf(
      "\nReading first group-descriptor from device \
         :\n \
         Blocks bitmap block: %u\n  \
         Inodes bitmap block: %u\n \
         Inodes table block : %u\n \
         Free blocks count  : %u\n \
         Free inodes count  : %u\n \
         Directories count  : %u\n",
      group.bg_block_bitmap, group.bg_inode_bitmap, group.bg_inode_table,
      group.bg_free_blocks_count, group.bg_free_inodes_count,
      group.bg_used_dirs_count); /* directories count */

  return 0;

  print_dir(&sb, fd, EXT2_ROOT_INO);

  close(fd);
  return 0;
}
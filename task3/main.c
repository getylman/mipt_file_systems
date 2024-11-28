#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "ext2_fs.h"
#include "ext2_parser.h"

int main() {
  const char *image_file =
      "/home/getylman/Desktop/mipt7/mipt_file_systems/task3/floppy.img";

  int fd = open(image_file, O_RDONLY);
  if (fd == -1) {
    perror("Ошибка открытия образа");
    exit(1);
  }

  struct ext2_super_block sb;

  if (get_super_block(&sb, fd)) {
    perror("get_super_block");
    return 1;
  }

  // print_fs_info(&sb);

  printf("%d\n", sizeof(struct ext2_inode));

  printf("%d\t%d\t%d\n", ((char *)(&(sb.s_feature_incompat)) - (char *)(&sb)),
         0x002 & sb.s_feature_incompat, *((unsigned *)(((char *)(&sb)) + 96)));

  close(fd);
  return 0;
}
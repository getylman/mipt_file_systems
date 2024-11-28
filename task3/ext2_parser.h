#include "ext2_fs.h"

// function to fill super block by file discriptor from image
int get_super_block(struct ext2_super_block* sb, int fd);

void print_dir(const struct ext2_super_block* sb, int fd, int inode_num);

void print_file(struct ext2_super_block* sb, int inode_num);
#include "ext2_fs.h"

// function to fill super block by file discriptor from image
int get_super_block(struct ext2_super_block* sb, int fd);
// function to print inside of directory by inode number
void print_dir(const struct ext2_super_block* sb, int fd, int inode_num);
// function to print file by inode number
void print_file(const struct ext2_super_block* sb, int fd, int inode_num);
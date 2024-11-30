#include <assert.h>
#include <fcntl.h>
#include <stdio.h>

#include "chunk_pool.h"

int preporation(const char* filename) {
  int fd = open(filename, O_RDWR, 0666);

  if (fd < 0) {
    perror("open");
    return -1;
  }

  const char text[] = "Test";

  pwrite(fd, text, 5, 0);

  close(fd);

  return 0;
}

int test_chunk_pool(const char* filename) {
  // if (preporation(filename)) {
  //   return 1;
  // }

  char buff[20];

  ChunkPool* pool = create_chunk_pool(filename);

  read_file(pool, buff, 10, 0);

  printf("%s\n", buff);

  char writem_buff[] = "asjhdhsaijdoisdjo";

  write_file(pool, writem_buff, 11, 4096);

  read_file(pool, buff, 11, 4096);

  printf("%s\n", buff);

  delete_chunk_pool(pool);

  return 0;
}

int main() {
  test_chunk_pool("bin_file");
  return 0;
}
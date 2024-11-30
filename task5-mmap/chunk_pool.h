#ifndef CHUNK_POOL
#define CHUNK_POOL

#include <stdlib.h>
// Структура для представления чанка
typedef struct {
  void* addr;
  size_t size;
  off_t offset;
  int fd;
} Chunk;

typedef struct {
  Chunk** chunks;
  size_t num_chunks;
  off_t file_size;
  int fd;
} ChunkPool;

ChunkPool* create_chunk_pool(const char* filename);
void delete_chunk_pool(ChunkPool* pool);

ssize_t write_file(ChunkPool* pool, const void* buf, size_t count,
                   off_t offset);
ssize_t read_file(ChunkPool* pool, void* buf, size_t count, off_t offset);

#endif  // CHUNK_POOL
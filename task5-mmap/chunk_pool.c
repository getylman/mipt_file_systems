#include "chunk_pool.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#define CHUNK_SIZE 4096
#define MIN(a, b) ((a) < (b) ? (a) : (b))

// CHUNK FUNKCTIONS

Chunk* create_chunk(int fd, off_t offset) {
  Chunk* chunk = malloc(sizeof(Chunk));
  if (chunk == NULL) {
    perror("malloc");
    return NULL;
  }

  chunk->addr =
      mmap(NULL, CHUNK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, offset);
  if (chunk->addr == MAP_FAILED) {
    perror("mmap");
    free(chunk);
    return NULL;
  }
  chunk->offset = offset;
  chunk->fd = fd;
  chunk->size = CHUNK_SIZE;
  return chunk;
}

void delete_chunk(Chunk* chunk) {
  if (chunk == NULL) return;
  if (munmap(chunk->addr, chunk->size) == -1) {
    perror("munmap");
  }
  free(chunk);
}

ssize_t read_chunk(Chunk* chunk, void* buf, size_t count,
                   off_t offset_in_chunk) {
  if (chunk == NULL || offset_in_chunk + count > chunk->size) return -1;
  memcpy(buf, (char*)chunk->addr + offset_in_chunk, count);
  return count;
}

ssize_t write_chunk(Chunk* chunk, const void* buf, size_t count,
                    off_t offset_in_chunk) {
  if (chunk == NULL || offset_in_chunk + count > chunk->size) return -1;
  memcpy((char*)chunk->addr + offset_in_chunk, buf, count);
  return msync(chunk->addr + offset_in_chunk, count, MS_SYNC);
}

// ~CHUNK FUNKCTIONS

ChunkPool* create_chunk_pool(const char* filename) {
  int fd = open(filename, O_RDWR | O_CREAT, 0666);
  if (fd == -1) {
    perror("open");
    return NULL;
  }

  struct stat st;
  if (fstat(fd, &st) == -1) {
    perror("fstat");
    close(fd);
    return NULL;
  }

  ChunkPool* pool = malloc(sizeof(ChunkPool));
  if (pool == NULL) {
    perror("malloc");
    close(fd);
    return NULL;
  }

  pool->file_size = st.st_size;
  pool->num_chunks = (pool->file_size + CHUNK_SIZE - 1) / CHUNK_SIZE;
  pool->fd = fd;
  pool->chunks = malloc(pool->num_chunks * sizeof(Chunk*));
  if (pool->chunks == NULL) {
    perror("malloc");
    free(pool);
    close(fd);
    return NULL;
  }

  for (size_t i = 0; i < pool->num_chunks; ++i) {
    pool->chunks[i] = create_chunk(fd, i * CHUNK_SIZE);
    if (pool->chunks[i] == NULL) {
      for (size_t j = 0; j < i; ++j) {
        delete_chunk(pool->chunks[j]);
      }
      free(pool->chunks);
      free(pool);
      close(fd);
      return NULL;
    }
  }

  return pool;
}

void delete_chunk_pool(ChunkPool* pool) {
  if (pool == NULL) return;
  for (size_t i = 0; i < pool->num_chunks; ++i) {
    delete_chunk(pool->chunks[i]);
  }
  free(pool->chunks);
  free(pool);
  close(pool->fd);
}

ssize_t write_file(ChunkPool* pool, const void* buf, size_t count,
                   off_t offset) {
  if (pool == NULL || offset + count > pool->file_size) return -1;

  size_t chunk_index = offset / CHUNK_SIZE;
  off_t offset_in_chunk = offset % CHUNK_SIZE;
  size_t bytes_to_write = count;

  while (bytes_to_write > 0) {
    size_t chunk_offset = chunk_index * CHUNK_SIZE;
    size_t bytes_in_chunk = CHUNK_SIZE - offset_in_chunk;
    size_t bytes_to_write_to_chunk = MIN(bytes_to_write, bytes_in_chunk);

    ssize_t bytes_written =
        write_chunk(pool->chunks[chunk_index], buf, bytes_to_write_to_chunk,
                    offset_in_chunk);
    if (bytes_written == -1) return -1;

    buf += bytes_written;
    offset_in_chunk = 0;
    bytes_to_write -= bytes_written;
    chunk_index++;
  }

  return count;
}

ssize_t read_file(ChunkPool* pool, void* buf, size_t count, off_t offset) {
  if (pool == NULL || offset + count > pool->file_size) return -1;

  size_t chunk_index = offset / CHUNK_SIZE;
  off_t offset_in_chunk = offset % CHUNK_SIZE;
  size_t bytes_to_read = count;

  while (bytes_to_read > 0) {
    size_t chunk_offset = chunk_index * CHUNK_SIZE;
    size_t bytes_in_chunk = CHUNK_SIZE - offset_in_chunk;
    size_t bytes_to_read_from_chunk = MIN(bytes_to_read, bytes_in_chunk);

    ssize_t bytes_read = read_chunk(pool->chunks[chunk_index], buf,
                                    bytes_to_read_from_chunk, offset_in_chunk);
    if (bytes_read == -1) return -1;

    buf += bytes_read;
    offset_in_chunk = 0;
    bytes_to_read -= bytes_read;
    chunk_index++;
  }

  return count;
}
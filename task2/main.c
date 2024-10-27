#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "linux/msdos_fs.h"

typedef struct msdos_dir_entry DirectoryEntry;

typedef struct fat_boot_sector FatBootSector;

FatBootSector cur_boot;

int set_fat_boot(int fd) {
  char dir_entry_struct[sizeof(FatBootSector) + 1];
  size_t size = 0;

  if ((size = read(fd, (char*)dir_entry_struct, sizeof(FatBootSector)) == -1)) {
    perror("read");
    printf("size = %lu %d %lu\n", size, size == sizeof(FatBootSector),
           sizeof(FatBootSector));
    return -1;
  }

  memcpy(&cur_boot, dir_entry_struct, sizeof(FatBootSector));

  return 0;
}

int get_sector_size() {
  const int kByteMaxValue = 256;
  return 0 + cur_boot.sector_size[0] + kByteMaxValue * cur_boot.sector_size[1];
}

int get_root_dir_size() {
  const int kByteMaxValue = 256;
  return 0 + cur_boot.dir_entries[0] + kByteMaxValue * cur_boot.dir_entries[1];
}

int get_dir_entries_offset() {
  return (0 + cur_boot.reserved + (cur_boot.fats * cur_boot.fat_length)) * get_sector_size();
}

int get_reserved() { return cur_boot.reserved; }

int get_cluster_size() { return get_sector_size() * cur_boot.sec_per_clus; }

int get_fat_lenght() { return get_sector_size() * cur_boot.fat_length; }

int get_num_of_sectors() {
  const int kByteMaxValue = 256;
  return 0 + cur_boot.sectors[0] + kByteMaxValue * cur_boot.sectors[1];
}

int get_data_offset() {
  return get_dir_entries_offset() + get_root_dir_size() * sizeof(DirectoryEntry);
}

int print_root_files(int fd) {
  const int kDirListingOffset = get_dir_entries_offset();
  for (int i = 0;; ++i) {
    DirectoryEntry dir_entry = {};
    char dir_entry_struct[sizeof(DirectoryEntry) + 1];
    size_t size = 0;

    if ((size = pread(fd, (char*)dir_entry_struct, sizeof(DirectoryEntry),
                      (size_t)kDirListingOffset + (2 * i + 1) * sizeof(DirectoryEntry))) != 32) {
      perror("pread");
      return -1;
    }

    memcpy(&dir_entry, dir_entry_struct, sizeof(DirectoryEntry));

    if (dir_entry.name[0] == '\0') {
      break;
    }

    printf(
        "file name is: %s attr: %d created time: %d created date: %d last "
        "access: %d\n",
        dir_entry.name, dir_entry.attr, dir_entry.ctime, dir_entry.cdate,
        dir_entry.adate);
  }
  return 0;
}

int cat_file(int fd, char* file_name) {
  if (!file_name) return -1;

  const int kDirListingOffset = get_dir_entries_offset();
  const int kSectorSize = get_sector_size();
  const int kReserved = get_reserved();
  short cur_file_start_id = -1;
  int cur_file_size = -1;

  for (int i = 0; ; ++i) {
    DirectoryEntry dir_entry = {};
    char dir_entry_struct[sizeof(DirectoryEntry) + 1];
    size_t size = 0;

    if ((size = pread(fd, (char*)dir_entry_struct, sizeof(DirectoryEntry),
                      (size_t)kDirListingOffset + (2 * i + 1) * sizeof(DirectoryEntry))) != 32) {
      perror("pread");
      return -1;
    }

    memcpy(&dir_entry, dir_entry_struct, sizeof(DirectoryEntry));

    if (dir_entry.name[0] == '\0') {
      // not found
      return -1;
    }
    if (strncmp(dir_entry.name, file_name, (strlen(file_name) > strlen(dir_entry.name)) ? strlen(dir_entry.name) : strlen(file_name))) {
      continue;
    }

    cur_file_start_id = dir_entry.start;
    cur_file_size = dir_entry.size;
    break;
  }

  if (cur_file_start_id == -1) {
    return -1;
  }

  const int kClusterSize = get_cluster_size();
  const int kDataFATOffset = kReserved * kSectorSize;
  const int kNumOfBytes = 2;
  const int kByteMaxValue = 256;
  unsigned char cluster_id_buff[3];


  char* file_data = (char*)malloc(sizeof(char) * (get_cluster_size() + 1));
  if (!file_data) {
    perror("malloc");
    return -1;
  }

  int cur_cluster_start = cur_file_start_id;

  while (1) {
    const int cur_offset =
        get_data_offset() + (cur_cluster_start - 2) * get_cluster_size();

    if (pread(fd, file_data, get_cluster_size(), cur_offset) != get_cluster_size()) {
      free(file_data);
      perror("pread");
      return -1;
    }
    printf("%s", file_data);

    if (pread(fd, cluster_id_buff, 2,
              kDataFATOffset + (cur_cluster_start * kNumOfBytes)) != 2) {
      perror("pread");
      return -1;
    }

    cur_cluster_start =
        0 + cluster_id_buff[0] + kByteMaxValue * cluster_id_buff[1];

    if (cur_cluster_start == 0xFFFF) {
      break;
    }
  }
  
  free(file_data);
  return 0;
}

int main() {
  const char* file_name =
      "/home/kanat/mipt7/mipt_1/mipt_file_systems/task2/floppy.img";

  int fd = open(file_name, O_RDONLY, S_IRUSR);

  if (fd == -1) {
    perror("open");
    return 1;
  }
  // get the structure
  if (set_fat_boot(fd) != 0) {
    close(fd);
    return 1;
  }
  // get dir contains
  if (print_root_files(fd) != 0) {
    close(fd);
    return 1;
  }
  // get file contains
  if (cat_file(fd, "MAIN    C   ") != 0) {
    close(fd);
    return 1;
  }

  close(fd);

  return 0;
}

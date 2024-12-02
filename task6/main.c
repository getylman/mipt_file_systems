#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define PROC_DIR "/proc"

void get_process_name(pid_t pid, char *name, size_t size) {
  char path[4096];
  snprintf(path, sizeof(path), PROC_DIR "/%d/comm", pid);

  int fd = open(path, O_RDONLY);
  if (fd == -1) {
    perror("open");
    strncpy(name, "Unknown", size);
    return;
  }
  ssize_t bytes_read = read(fd, name, size - 1);
  if (bytes_read == -1) {
    perror("read");
    strncpy(name, "Unknown", size);
    close(fd);
    return;
  }
  name[bytes_read] = '\0';
  name[strcspn(name, "\n")] = '\0';
  close(fd);
}

void list_open_files(pid_t pid) {
  const int kSize = 4096;
  char fd_dir[kSize];
  snprintf(fd_dir, sizeof(fd_dir), PROC_DIR "/%d/fd", pid);

  DIR *dir = opendir(fd_dir);
  if (!dir) {
    perror("opendir");
    return;
  }

  struct dirent *entry;
  while (entry = readdir(dir)) {
    if (entry->d_name[0] == '.') continue;

    char fd_path[kSize];
    char target_path[kSize];
    snprintf(fd_path, sizeof(fd_path), "%s/%s", fd_dir, entry->d_name);

    ssize_t len = readlink(fd_path, target_path, sizeof(target_path) - 1);
    if (len == -1) {
      perror("readlink");
      continue;
    }
    target_path[len] = '\0';
    printf("\tFD: %s -> %s\n", entry->d_name, target_path);
  }

  closedir(dir);
}

int main() {
  DIR *proc_dir = opendir(PROC_DIR);
  if (!proc_dir) {
    perror("opendir");
    return EXIT_FAILURE;
  }

  struct dirent *entry;
  while (entry = readdir(proc_dir)) {
    pid_t pid = atoi(entry->d_name);
    if (entry->d_type == DT_DIR && pid > 0) {
      char process_name[256];
      get_process_name(pid, process_name, sizeof(process_name));

      printf("PID: %d, Process Name: %s\n", pid, process_name);
      list_open_files(pid);
    }
  }

  closedir(proc_dir);
  return EXIT_SUCCESS;
}

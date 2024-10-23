#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define FAT16_SECTOR_SIZE 512

// Структура для записи в каталоге
typedef struct {
  char filename[8];       // Имя файла (8 символов)
  char extension[3];      // Расширение файла (3 символа)
  uint8_t attributes;     // Атрибуты файла
  uint8_t reserved;       // Зарезервировано
  uint8_t createTime[2];  // Время создания (2 байта)
  uint8_t createDate[2];  // Дата создания (2 байта)
  uint8_t lastAccessDate[2];  // Дата последнего доступа (2 байта)
  uint8_t lastWriteTime[2];  // Время последней записи (2 байта)
  uint8_t lastWriteDate[2];  // Дата последней записи (2 байта)
  uint16_t clusterStart;  // Номер начального кластера (2 байта)
  uint16_t fileSize;  // Размер файла (2 байта)
} DirectoryEntry;

// Структура для записи в таблице FAT
typedef struct {
  uint16_t cluster;  // Номер кластера
} FATEntry;

// Функция для вывода атрибутов файла
void printAttributes(uint8_t attributes) {
  printf(" ");
  if (attributes & 0x01) printf("R");  // Только для чтения
  if (attributes & 0x02) printf("H");  // Скрытый
  if (attributes & 0x04) printf("S");  // Системный
  if (attributes & 0x10) printf("A");  // Архивный
  if (attributes & 0x20) printf("D");  // Каталог
  if (attributes & 0x40) printf("V");  // Том
  if (attributes & 0x80) printf("L");  // Метка тома
}

// Функция для вывода даты и времени
void printDateTime(uint8_t *time, uint8_t *date) {
  printf(" %02d.%02d.%04d %02d:%02d", date[1], date[0], 1980 + date[2],
         (time[0] & 0x1f) << 1 | (time[1] & 0x80) >> 7, (time[1] & 0x7f) >> 1);
}

// Функция для чтения сектора из образа FAT16
void readSector(int fd, int sector, char *buffer) {
  lseek(fd, sector * FAT16_SECTOR_SIZE, SEEK_SET);
  if (read(fd, buffer, FAT16_SECTOR_SIZE) == -1) {
    perror("read");
    exit(1);
  }
}

// Функция для чтения записи из таблицы FAT
FATEntry readFATEntry(int fd, int cluster) {
  FATEntry entry;
  char sector[FAT16_SECTOR_SIZE];
  int sectorIndex = cluster / 8;
  int offset = (cluster % 8) * 2;
  readSector(fd, sectorIndex + 2, sector);
  memcpy(&entry, sector + offset, sizeof(FATEntry));
  return entry;
}

// Функция для чтения файла из образа FAT16
void readFile(int fd, int clusterStart, int fileSize) {
  char buffer[FAT16_SECTOR_SIZE];
  int currentCluster = clusterStart;

  while (fileSize > 0) {
    readSector(fd, currentCluster, buffer);
    write(STDOUT_FILENO, buffer,
          FAT16_SECTOR_SIZE);  // Использовать write для вывода
    fileSize -= FAT16_SECTOR_SIZE;

    // Читаем следующий кластер
    FATEntry entry = readFATEntry(fd, currentCluster);
    currentCluster = entry.cluster;
  }
}

int main(int argc, char *argv[]) {
  if (argc != 2) {
    fprintf(stderr, "Usage: %s <image_file>\n", argv[0]);
    return 1;
  }

  int fd = open(argv[1], O_RDONLY);
  if (fd == -1) {
    fprintf(stderr, "Error opening file: %s\n", argv[1]);
    return 1;
  }

  // Читаем корневой каталог
  char directory[FAT16_SECTOR_SIZE * 32];
  readSector(
      fd, 19,
      directory);  // Предположим, что корневой каталог начинается с сектора 19

  // Выводим список файлов
  int i = 0;
  while (i < 32 * FAT16_SECTOR_SIZE) {
    DirectoryEntry entry;
    memcpy(&entry, directory + i, sizeof(DirectoryEntry));
    // Если запись не пуста
    if (entry.filename[0] != 0xe5 && entry.filename[0] != 0x00) {
      // Вывод имени файла
      printf("%s.%s", entry.filename, entry.extension);

      // Вывод атрибутов
      printAttributes(entry.attributes);

      // Вывод даты и времени создания/изменения
      printDateTime(entry.createTime, entry.createDate);
      printf(" ");
      printDateTime(entry.lastWriteTime, entry.lastWriteDate);

      // Вывод размера файла
      printf(" %u", entry.fileSize);

      printf("\n");
    }
    i += sizeof(DirectoryEntry);
  }

  // Чтение файла
  char filename[12];
  printf("Введите имя файла для чтения: ");
  scanf("%s", filename);

  // Поиск записи файла
  int found = 0;
  i = 0;
  while (i < 32 * FAT16_SECTOR_SIZE) {
    DirectoryEntry entry;
    memcpy(&entry, directory + i, sizeof(DirectoryEntry));
    // Если запись не пуста
    if (entry.filename[0] != 0xe5 && entry.filename[0] != 0x00) {
      // Сравнение имени файла
      if (strncmp(filename, entry.filename, 8) == 0 &&
          strncmp(filename + 8, entry.extension, 3) == 0) {
        found = 1;
        readFile(fd, entry.clusterStart, entry.fileSize);
        break;
      }
    }
    i += sizeof(DirectoryEntry);
  }

  if (!found) {
    printf("Файл не найден\n");
  }

  close(fd);
  return 0;
}

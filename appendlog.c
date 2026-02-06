#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#define LOG_FILENAME "appendlog.dat"

#define RECORD_SIZE 32
#define MAX_RECORDS 1048576

#define HEADER_INTS 2
#define HEADER_SIZE ((int)(sizeof(int) * HEADER_INTS))
#define FILE_SIZE ((off_t)HEADER_SIZE + (off_t)MAX_RECORDS * (off_t)RECORD_SIZE)

static void *map_log_file(int fd) {
    void *addr = mmap(NULL, (size_t)FILE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }
    return addr;
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    int fd = open(LOG_FILENAME, O_RDWR | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    if (ftruncate(fd, FILE_SIZE) != 0) {
        perror("ftruncate");
        close(fd);
        return 1;
    }

    void *map = map_log_file(fd);

    if (munmap(map, (size_t)FILE_SIZE) != 0) {
        perror("munmap");
        close(fd);
        return 1;
    }

    if (close(fd) != 0) {
        perror("close");
        return 1;
    }

    return 0;
}
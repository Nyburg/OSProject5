#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
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

static int parse_int(const char *s) {
    char *end = NULL;
    long v = strtol(s, &end, 10);
    if (end == s || *end != '\0' || v < 0 || v > 2147483647L) {
        return -1;
    }
    return (int)v;
}

int main(int argc, char **argv) {
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
    (void)map;

    int argn = argc - 1;
    if (argn < 2 || (argn % 2) != 0) {
        fprintf(stderr, "Usage: %s <count> <prefix> [<count> <prefix> ...]\n", argv[0]);
        munmap(map, (size_t)FILE_SIZE);
        close(fd);
        return 1;
    }

    int pairs = argn / 2;
    if (pairs > 5) {
        fprintf(stderr, "Too many pairs (max 5)\n");
        munmap(map, (size_t)FILE_SIZE);
        close(fd);
        return 1;
    }

    for (int i = 0; i < pairs; i++) {
        int count = parse_int(argv[1 + i * 2]);
        const char *prefix = argv[2 + i * 2];

        if (count < 0) {
            fprintf(stderr, "Bad count: %s\n", argv[1 + i * 2]);
            munmap(map, (size_t)FILE_SIZE);
            close(fd);
            return 1;
        }

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            munmap(map, (size_t)FILE_SIZE);
            close(fd);
            return 1;
        }

        if (pid == 0) {
            (void)count;
            (void)prefix;
            munmap(map, (size_t)FILE_SIZE);
            close(fd);
            _exit(0);
        }
    }

    for (int i = 0; i < pairs; i++) {
        wait(NULL);
    }

    munmap(map, (size_t)FILE_SIZE);
    close(fd);
    return 0;
}
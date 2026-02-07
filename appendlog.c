#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

#define HEADER_OFFSET_INDEX 0
#define HEADER_COUNT_INDEX  1

static void *map_log_file(int fd) {
    void *addr = mmap(NULL, (size_t)FILE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }
    return addr;
}

static void lock_region(int fd, short lock_type, off_t start, off_t len) {
    struct flock fl;
    memset(&fl, 0, sizeof(fl));
    fl.l_type = lock_type;
    fl.l_whence = SEEK_SET;
    fl.l_start = start;
    fl.l_len = len;

    if (fcntl(fd, F_SETLKW, &fl) != 0) {
        perror("fcntl(F_SETLKW)");
        exit(1);
    }
}

static int parse_int(const char *s) {
    char *end = NULL;
    long v = strtol(s, &end, 10);
    if (end == s || *end != '\0' || v < 0 || v > 2147483647L) {
        return -1;
    }
    return (int)v;
}

static void append_one_record(int fd, void *map, const char *text32) {
    int *header = (int *)map;

    lock_region(fd, F_WRLCK, (off_t)0, (off_t)HEADER_SIZE);

    int old_offset = header[HEADER_OFFSET_INDEX];
    int old_count  = header[HEADER_COUNT_INDEX];

    header[HEADER_COUNT_INDEX] = old_count + 1;
    header[HEADER_OFFSET_INDEX] = old_offset + RECORD_SIZE;

    lock_region(fd, F_UNLCK, (off_t)0, (off_t)HEADER_SIZE);

    off_t rec_start = (off_t)HEADER_SIZE + (off_t)old_offset;
    char *rec_ptr = (char *)map + rec_start;

    lock_region(fd, F_WRLCK, rec_start, (off_t)RECORD_SIZE);
    memcpy(rec_ptr, text32, (size_t)RECORD_SIZE);
    lock_region(fd, F_UNLCK, rec_start, (off_t)RECORD_SIZE);
}

static void dump_log(int fd, void *map) {
    int *header = (int *)map;

    lock_region(fd, F_RDLCK, (off_t)0, (off_t)0);

    int count = header[HEADER_COUNT_INDEX];

    for (int i = 0; i < count; i++) {
        off_t rec_start = (off_t)HEADER_SIZE + (off_t)i * (off_t)RECORD_SIZE;
        char *rec_ptr = (char *)map + rec_start;

        char tmp[RECORD_SIZE + 1];
        memcpy(tmp, rec_ptr, (size_t)RECORD_SIZE);
        tmp[RECORD_SIZE] = '\0';

        printf("%d: %s\n", i, tmp);
    }

    lock_region(fd, F_UNLCK, (off_t)0, (off_t)0);
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
            for (int n = 0; n < count; n++) {
                char rec[RECORD_SIZE];
                memset(rec, 0, sizeof(rec));
                (void)snprintf(rec, sizeof(rec), "%s %d", prefix, n);

                append_one_record(fd, map, rec);
            }

            munmap(map, (size_t)FILE_SIZE);
            close(fd);
            _exit(0);
        }
    }

    for (int i = 0; i < pairs; i++) {
        wait(NULL);
    }

    dump_log(fd, map);

    munmap(map, (size_t)FILE_SIZE);
    close(fd);
    return 0;
}

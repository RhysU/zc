/*
 * zc: C version of https://github.com/rupa/z.
 * Licensed under the Apache License, Version 2.0.
 */
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#define die(...) do {                  \
        fprintf (stderr, __VA_ARGS__); \
        fputc('\n', stderr);           \
        exit(EXIT_FAILURE); }          \
    while (0)

long milliseconds() {
    struct timeval now;
    gettimeofday(&now, NULL);
    return 1000*now.tv_sec + now.tv_usec/1000;
}

// --add
// --complete
int main(int argc, char **argv)
{
    int fd = -1;
    int opt;
    while ((opt = getopt(argc, argv, "f:h")) != -1) {
        switch (opt) {
        default:
        case 'h':
            die("Usage: %s -F DATABASE", argv[0]);
        case 'f':
            fd = open(optarg, O_CREAT | O_RDWR, 0600);
            if (fd < 0) {
                die("Failed opening '%s' (%d): %s",
                    optarg, errno, strerror(errno));
            }
            if (flock(fd, LOCK_EX)) {
                die("Failed locking '%s' (%d): %s",
                    optarg, errno, strerror(errno));
            }
            break;
        }
    }
    printf("Hello %d %ld\n", fd, milliseconds());
    exit(EXIT_SUCCESS);
}

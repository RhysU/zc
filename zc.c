/*
 * zc: C version of https://github.com/rupa/z.
 * Licensed under the Apache License, Version 2.0.
 */
#define _POSIX_C_SOURCE 2
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
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

int main(int argc, char **argv)
{
    int opt;
    while ((opt = getopt(argc, argv, "h")) != -1) {
        switch (opt) {
        case 'h':
            die("Unimplemented");
        default:
            die("Usage: %s", argv[0]);
        }
    }
    printf("Hello %ld\n", milliseconds());
    exit(EXIT_SUCCESS);
}

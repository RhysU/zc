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

#define SEPARATOR ('|')
#define DELIMITERS ("|\n")

long milliseconds() {
    struct timeval now;
    gettimeofday(&now, NULL);
    return 1000*now.tv_sec + now.tv_usec/1000;
}

struct row {
    struct row *next;
    char *path;
    long rank;
    long time;
};

struct row *cons(struct row *tail, char *path, long rank, long time) {
    struct row * head = malloc(sizeof(struct row));
    if (!head) {
        die("Failed malloc (%d): %s", errno, strerror(errno));
    }
    head->next = tail;
    head->path = path;
    head->rank = rank;
    head->time = time;
    return head;
}

struct row *reverse(struct row *head)
{
    struct row *prev = NULL, *curr = head, *next;
    while (curr) {
        next = curr->next;
        curr->next = prev;
        prev = curr;
        curr = next;
    }
    return prev;
}

struct row *load(FILE *database) {
    struct row *head = NULL;
    if (database) {
        for (int line = 1; /*NOP*/; errno = 0, ++line) {
            // Load each line of the file into lineptr
            char *lineptr = NULL;
            size_t n = 0;
            ssize_t bytes = getline(&lineptr, &n, database);
            if (errno) {
                die("Failed reading database at line %d (%d): %s",
                    line, errno, strerror(errno));
            }
            if (bytes < 0) {
                break;
            }
            if (bytes == 0) {
                continue;
            }

            // Tokenize each line into (path, rank, time)
            char *path = strtok(lineptr, DELIMITERS);
            if (!path) die("No path in line %d", line);
            char *rank = strtok(NULL, DELIMITERS);
            if (!rank) die("No rank in line %d", line);
            long rankl = strtol(rank, NULL, 10);
            if (errno) die("Invalid rank '%s' in line %d (%d): %s",
                           rank, line, errno, strerror(errno));
            char *time = strtok(NULL, DELIMITERS);
            if (!time) die("No time in line %d", line);
            long timel = strtol(time, NULL, 10);
            if (errno) die("Invalid time '%s' in line %d (%d): %s",
                           time, line, errno, strerror(errno));
            if (strtok(NULL, DELIMITERS)) die("Excess in line %d", line);

            // Built up the reverse of the desired result
            head = cons(head, path, rankl, timel);
        }
    }
    return reverse(head);
}

// --add
// --complete
int main(int argc, char **argv)
{
    // Process arguments with post-condition that database is open
    FILE *database = NULL;
    int option;
    while ((option = getopt(argc, argv, "f:h")) != -1) {
        switch (option) {
        default:
        case 'h':
            die("Usage: %s -F DATABASE", argv[0]);
        case 'f':
            if (!(database = fopen(optarg, "a+"))) {
                die("Failed opening '%s' (%d): %s",
                    optarg, errno, strerror(errno));
            }
            if (flock(fileno(database), LOCK_EX)) {
                die("Failed locking '%s' (%d): %s",
                    optarg, errno, strerror(errno));
            }
            rewind(database);
            break;
        }
    }
    struct row *head = load(database);
    while (head) {
        // FIXME
        printf("%s   %ld   %ld\n", head->path, head->rank, head->time);
        head = head->next;
    }

    exit(EXIT_SUCCESS);
}

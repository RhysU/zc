/*
 * zc: C version of parts of https://github.com/rupa/z.
 * Licensed under the WTFPL (version 2) following rupa/z.
 * No free(3) calls because this process is lightweight and short-lived.
 */
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/time.h>
#include <unistd.h>

#define die(...) do {                 \
        fputs("zc: ", stderr);        \
        fprintf(stderr, __VA_ARGS__); \
        fputc('\n', stderr);          \
        exit(EXIT_FAILURE); }         \
    while (0)

#define SEPARATOR ('|')
#define DELIMITERS ("|\n")
#define AGING_THRESHOLD (9000)
#define AGING_RESCALING (0.99)

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

long milliseconds() {
    struct timeval now;
    gettimeofday(&now, NULL);
    return 1000*now.tv_sec + now.tv_usec/1000;
}

// Unlike rupa/z, here aging cannot exclude the newest addition.
struct row *record(struct row *head, char *path) {
    long now = milliseconds();
    bool found = false;
    long count = 0;
    for (struct row *curr = head; curr; curr = curr->next) {
        if (0 == strcmp(path, curr->path)) {
            found = true;
            ++curr->rank;
            curr->time = now;
        }
        count += curr->rank;
    }
    if (count > AGING_THRESHOLD) {
        for (struct row *curr = head; curr; curr = curr->next) {
            curr->rank *= AGING_RESCALING;
        }
    }
    if (!found) {
        head = cons(head, path, 1L, now);
        ++count;
    }
    return head;
}

int main(int argc, char **argv)
{
    // Process arguments with post-condition that database is loaded.
    bool add = false;
    bool complete = false;
    FILE *database = NULL;
    int option;
    while ((option = getopt(argc, argv, "acf:h")) != -1) {
        switch (option) {
        default:
        case 'a':
            add = true;
            break;
        case 'c':
            complete = true;
            break;
        case 'h':
            fprintf(stdout, "Usage: %s [-ac] -f DATABASE ARG...\n", argv[0]);
            exit(EXIT_SUCCESS);
        case 'f':
            if (!(database = fopen(optarg, "ab+"))) {
                die("Failed opening '%s' (%d): %s",
                    optarg, errno, strerror(errno));
            }
            if (flock(fileno(database), LOCK_EX)) {
                die("Failed locking '%s' (%d): %s",
                    optarg, errno, strerror(errno));
            }
            break;
        }
    }
    if (!database) {
        die("Database must be specified.");
    }
    struct row *head = load(database);

    // Process positional arguments using the loaded database
    if (add) {
        if (complete) die("Cannot both (a)dd and (c)omplete.");

        // Add in reverse as if separate program invocations
        for (int ipos = argc; ipos --> optind;) {
            head = record(head, argv[ipos]);
        }
        // Overwrite database with updated contents for all positive ranks
        if (!freopen(NULL, "w", database)) { // FIXME
            die("Error freopening (%d): %s", errno, strerror(errno));
        }
        for (struct row *curr = head; curr; curr = curr->next) {
            if (curr->rank > 0) {
                fprintf(database, "%s%c%ld%c%ld\n", curr->path,
                        SEPARATOR, curr->rank, SEPARATOR, curr->time);
            }
        }

    } else if (complete) {
        if (add) die("Cannot both (a)dd and (c)omplete.");

        // TODO

    } else { // Match incoming arguments

        // TODO

    }

    exit(EXIT_SUCCESS);
}

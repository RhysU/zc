/*
 * zc: C version of parts of https://github.com/rupa/z.
 * Licensed under the WTFPL (version 2) following rupa/z.
 * No free(3) calls because this process is lightweight and short-lived.
 */
#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/file.h>
#include <sys/time.h>
#include <unistd.h>

// Initialized in main(...)
static long NOW = 0;

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
    long frecency;
};

long frecency(long rank, long time) {
    long age = (time - NOW);
    if (age <   3600000) return rank * 4;
    if (age <  86400000) return rank * 2;
    if (age < 604800000) return rank / 2;
    return rank / 4;
}

struct row *cons(struct row *tail, char *path, long rank, long time) {
    struct row * head = malloc(sizeof(struct row));
    if (!head) {
        die("Failed malloc (%d): %s", errno, strerror(errno));
    }
    head->next = tail;
    head->path = path;
    head->frecency = frecency(rank, time);
    head->rank = rank;
    head->time = time;
    return head;
}

// TODO Likely not needed in the final version
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

            // Build up the reverse of the desired result
            head = cons(head, path, rankl, timel);
        }
    }
    return reverse(head);
}

// Unlike rupa/z, here aging cannot exclude the newest addition.
struct row *add(struct row *head, char *path) {
    bool found = false;
    long count = 0;
    for (struct row *curr = head; curr; curr = curr->next) {
        if (0 == strcmp(path, curr->path)) {
            found = true;
            ++curr->rank;
            curr->time = NOW;
        }
        count += curr->rank;
    }
    if (count > AGING_THRESHOLD) {
        for (struct row *curr = head; curr; curr = curr->next) {
            curr->rank *= AGING_RESCALING;
        }
    }
    if (!found) {
        head = cons(head, path, 1L, NOW);
        ++count;
    }
    return head;
}

// Construct a new list of all entries matching argv[0]...argv[argc-1]
struct row *matches(struct row *head, int argc, char **argv) {

    // Accumulate all entries matching all segments
    struct row * results = NULL;
    for (struct row *curr = head; curr; curr = curr->next) {
        bool all = true;
        char *q = curr->path;
        for (int i = 0; i < argc; ++i) {
            q = strstr(q, argv[i]);
            if (q) {
                q += strlen(argv[i]);
            } else {
                all = false;
                break;
            }
        }
        if (all) {
            results = cons(results, curr->path, curr->rank, curr->time);
        }
    }

    return reverse(results);
}

typedef int (*comparator)(struct row *, struct row *);

int compare_paths(struct row *a, struct row *b) {
    return strcmp(a->path, b->path);
}

int compare_ranks(struct row *a, struct row *b) {
    return a->rank - b->rank;
}

int compare_times(struct row *a, struct row *b) {
    return a->time - b->time;
}

int compare_frecencies(struct row *a, struct row *b) {
    return a->frecency - b->frecency;
}

void fprint_paths(FILE *stream, struct row *tail, char sep) {
    for (struct row *curr = tail; curr; curr = curr->next) {
        fprintf(stream, "%s%c", curr->path, sep);
    }
}

// Move non-NULL head from src onto dest.
void move(struct row **dst, struct row **src) {
    struct row *item = *src;
    *src = (*src)->next;
    item->next = *dst;
    *dst = item;
}

struct row *merge(
        struct row *left, struct row *right, comparator cmp, bool reverse
) {
    int maybe_reverse = reverse ? -1 : 1;
    struct row *tail = NULL;

    fprint_paths(stderr, left, '|');
    fputc('*', stderr);
    fprint_paths(stderr, right, '|');
    fputc('*', stderr);

    while (left) {
        if (right && maybe_reverse * cmp(left, right) <= 0) {
            move(&tail, &right);
        } else {
            move(&tail, &left);
        }
    }
    while (right) {
        move(&tail, &right);
    }

    fprint_paths(stderr, tail, '|');
    fputc('\n', stderr);

    return tail;
}

struct row *sort(struct row *tail, comparator cmp, bool reverse) {
    // Eagerly return when no work to perform
    if (!tail || !tail->next) {
        return tail;
    }

    // Split tail evenly into into left/right
    struct row *left = NULL, *right = NULL;
    while (tail) {
        move(&left, &tail);
        if (tail) {
            move(&right, &tail);
        }
    }

    // Recurse then merge results from left/right.
    // Reverse sort order at each step to accommodate singly-linked data.
    return merge(sort(left, cmp, !reverse),
                 sort(right, cmp, !reverse),
                 cmp,
                 reverse);
}

long milliseconds() {
    struct timeval now;
    gettimeofday(&now, NULL);
    return 1000*now.tv_sec + now.tv_usec/1000;
}

enum mode { ADD, COMPLETE, FRECENT, RANK, TIME };

int main(int argc, char **argv)
{
    NOW = milliseconds();

    struct row *left = NULL;
    left = cons(left, "d", 1, 1);
    left = cons(left, "e", 1, 1);
    left = cons(left, "c", 1, 1);
    left = cons(left, "a", 1, 1);
    left = cons(left, "b", 1, 1);
    fprint_paths(stderr, sort(left, compare_paths, false), '|');
    fputc('\n', stderr);
    exit(EXIT_FAILURE);

    //struct row *left = NULL;
    //left = cons(left, "d", 1, 1);
    //left = cons(left, "c", 1, 1);
    //left = cons(left, "a", 1, 1);
    //struct row *right = NULL;
    //right = cons(right, "f", 1, 1);
    //right = cons(right, "e", 1, 1);
    //right = cons(right, "b", 1, 1);
    //merge(left, right, compare_paths);
    //exit(EXIT_FAILURE);

    // Process arguments with post-condition that database is loaded.
    FILE *database = NULL;
    int option;
    enum mode mode = COMPLETE;
    while ((option = getopt(argc, argv, "ad:fhrt")) != -1) {
        switch (option) {
        default:
        case 'h':
            fprintf(option == 'h' ? stdout : stderr,
                    "Usage: %s -d DATABASE -a PATH...\n"
                    "Usage: %s -d DATABASE [-f] [-r] [-t] NEEDLE...\n",
                    argv[0], argv[0]);
            exit(option == 'h' ? EXIT_SUCCESS : EXIT_FAILURE);
        case 'a':
            mode = ADD;
            break;
        case 'd':
            if (!(database = fopen(optarg, "ab+"))) {
                die("Failed opening '%s' (%d): %s",
                    optarg, errno, strerror(errno));
            }
            if (flock(fileno(database), LOCK_EX)) {
                die("Failed locking '%s' (%d): %s",
                    optarg, errno, strerror(errno));
            }
            break;
        case 'f':
            mode = FRECENT;
            break;
        case 'r':
            mode = RANK;
            break;
        case 't':
            mode = TIME;
            break;
        }
    }
    if (!database) {
        die("Database must be specified.");
    }
    struct row *head = load(database);

    // Now process positional arguments using the loaded database.

    // Possibly add entries to the database...
    if (mode == ADD) {
        // Add in reverse of CLI as if separate program invocations
        for (int ipos = argc; ipos --> optind;) {
            head = add(head, argv[ipos]);
        }
        // Overwrite database with updated contents for all positive ranks
        if (!freopen(NULL, "w", database)) {
            die("Error freopening (%d): %s", errno, strerror(errno));
        }
        for (struct row *curr = head; curr; curr = curr->next) {
            if (curr->rank > 0) {
                fprintf(database, "%s%c%ld%c%ld\n", curr->path,
                        SEPARATOR, curr->rank, SEPARATOR, curr->time);
            }
        }
        exit(EXIT_SUCCESS);
    }

    // Find all entries matching the positional segments
    head = matches(head, argc - optind, &argv[optind]);

    // Sort all matches per the given criterion
    comparator cmp = (mode == FRECENT) ? &compare_frecencies
                   : (mode == RANK)    ? &compare_ranks
                   : (mode == TIME)    ? &compare_times
                   :                     &compare_paths;
    head = sort(head, cmp, false);

    if (mode == COMPLETE) {

        // Either print all matching entries from the database...
        fprint_paths(stdout, head, '\n');

    } else {

        // ...or print only the first result.
        if (head) {
            fprintf(stdout, "%s\n", head->path);
        }

    }

    exit(EXIT_SUCCESS);
}

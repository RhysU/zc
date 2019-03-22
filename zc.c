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

// See rupa/z for background on this portmanteau of frequent and recent
long frecency(long rank, long millis) {
    long age = millis - NOW;
    // Units: D    H    M    S   MS
    if (age <          60 * 60 * 1000) return rank * 4;
    if (age <     24 * 60 * 60 * 1000) return rank * 2;
    if (age < 7 * 24 * 60 * 60 * 1000) return rank / 2;
    return rank / 4;
}

struct row {
    struct row *next;
    char *path;
    long rank;
    long millis;
    long frecency;
};

struct row *cons(struct row *list, char *path, long rank, long millis) {
    struct row * head = malloc(sizeof(struct row));
    if (!head) {
        die("Failed malloc (%d): %s", errno, strerror(errno));
    }
    head->next = list;
    head->path = path;
    head->frecency = frecency(rank, millis);
    head->rank = rank;
    head->millis = millis;
    return head;
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

            // Tokenize each line into (path, rank, millis)
            char *path = strtok(lineptr, DELIMITERS);
            if (!path) die("No path in line %d", line);
            char *rank = strtok(NULL, DELIMITERS);
            if (!rank) die("No rank in line %d", line);
            long rankl = strtol(rank, NULL, 10);
            if (errno) die("Invalid rank '%s' in line %d (%d): %s",
                           rank, line, errno, strerror(errno));
            char *millis = strtok(NULL, DELIMITERS);
            if (!millis) die("No millis in line %d", line);
            long timel = strtol(millis, NULL, 10);
            if (errno) die("Invalid millis '%s' in line %d (%d): %s",
                           millis, line, errno, strerror(errno));
            if (strtok(NULL, DELIMITERS)) die("Excess in line %d", line);

            // Build up the desired result
            head = cons(head, path, rankl, timel);
        }
    }
    return head;
}

// Unlike rupa/z, here aging cannot exclude the newest addition.
struct row *add(struct row *head, char *path) {
    bool found = false;
    long count = 0;
    for (struct row *curr = head; curr; curr = curr->next) {
        if (0 == strcmp(path, curr->path)) {
            found = true;
            ++curr->rank;
            curr->millis = NOW;
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
            results = cons(results, curr->path, curr->rank, curr->millis);
        }
    }

    return results;
}

typedef int (*comparator)(struct row *, struct row *);

int compare_times(struct row *a, struct row *b) {
    int i = b->millis - a->millis;
    return i ? i : strcmp(a->path, b->path);
}

int compare_paths(struct row *a, struct row *b) {
    int i = strcmp(a->path, b->path);
    return i ? i : compare_times(a, b);
}

int compare_ranks(struct row *a, struct row *b) {
    int i = b->rank - a->rank;
    return i ? i : compare_times(a, b);
}

int compare_frecencies(struct row *a, struct row *b) {
    int i = b->frecency - a->frecency;
    return i ? i : compare_times(a, b);
}

void fprint_paths(FILE *stream, struct row *list, int intersep, int aftersep) {
    bool any = false;
    for (; list; list = list->next, any = true) {
        fputs(list->path, stream);
        if (list->next) {
            fputc(intersep, stream);
        }
    }
    if (any && aftersep) {
        fputc(aftersep, stream);
    }
}

void print_paths(struct row *list, int intersep, int aftersep) {
    return fprint_paths(stdout, list, intersep, aftersep);
}

// Move non-NULL head from src onto dest.
void move(struct row **dst, struct row **src) {
    struct row *item = *src;
    *src = (*src)->next;
    item->next = *dst;
    *dst = item;
}

struct row *sort(struct row *list, comparator cmp, bool reverse) {
    // Eagerly return when no work to perform
    if (!list || !list->next) {
        return list;
    }

    // Split list evenly into into a/b
    struct row *a = NULL, *b = NULL;
    while (list) {
        move(&a, &list);
        if (list) {
            move(&b, &list);
        }
    }

    // Recurse on a/b with ordering flip to handle singly-linked data
    a = sort(a, cmp, !reverse);
    b = sort(b, cmp, !reverse);

    // Merge the recursive results onto a fresh list adhering to reverse
    int maybe_reverse = reverse ? -1 : 1;
    while (a) {
        if (b && maybe_reverse * cmp(a, b) <= 0) {
            move(&list, &b);
        } else {
            move(&list, &a);
        }
    }
    while (b) {
        move(&list, &b);
    }

    return list;
}

long milliseconds() {
    struct timeval now;
    gettimeofday(&now, NULL);
    return 1000*now.tv_sec + now.tv_usec/1000;
}

enum mode { ADD, COMPLETE, ONE };

int main(int argc, char **argv)
{
    NOW = milliseconds();

    // Process arguments with post-condition that database is loaded.
    FILE *database = NULL;
    int option;
    enum mode mode = COMPLETE;
    comparator cmp = &compare_paths;
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
            cmp = &compare_frecencies;
            mode = ONE;
            break;
        case 'r':
            cmp = &compare_ranks;
            mode = ONE;
            break;
        case 't':
            cmp = &compare_times;
            mode = ONE;
            break;
        }
    }
    if (!database) {
        die("Database required per help available with -h.");
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
            die("Error freopen-ing (%d): %s", errno, strerror(errno));
        }
        for (struct row *curr = head; curr; curr = curr->next) {
            if (curr->rank > 0) {
                fprintf(database, "%s%c%ld%c%ld\n", curr->path,
                        SEPARATOR, curr->rank, SEPARATOR, curr->millis);
            }
        }
        exit(EXIT_SUCCESS);
    }

    // Find all entries matching the positional segments
    head = matches(head, argc - optind, &argv[optind]);

    // Sort all matches per the given criterion
    head = sort(head, cmp, false);

    // Either print all matching entries from the database...
    // ...or print only the first result.
    if (mode == COMPLETE) {
        print_paths(head, '\n', '\n');
    } else if (head) {
        printf("%s\n", head->path);
    }

    exit(EXIT_SUCCESS);
}

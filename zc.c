/*
 * zc: C version of https://github.com/rupa/z.
 * Licensed under the Apache License, Version 2.0.
 */
#include <stdio.h>
#include <sys/time.h>

long milliseconds() {
    struct timeval now;
    gettimeofday(&now, NULL);
    return 1000*now.tv_sec + now.tv_usec/1000;
}

int main(int argc, char **argv)
{
    printf("Hello %ld\n", milliseconds());
}

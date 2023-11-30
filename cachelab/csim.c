#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cachelab.h"

#define BUF_SIZE 128

void printUsage(char *prog) {
    printf(
        "Usage: %s [-hv] -s <num> -E <num> -b <num> -t <file>\n"
        "Options:\n"
        "  -h         Print this help message.\n"
        "  -v         Optional verbose flag.\n"
        "  -s <num>   Number of set index bits.\n"
        "  -E <num>   Number of lines per set.\n"
        "  -b <num>   Number of block offset bits.\n"
        "  -t <file>  Trace file.\n"
        "\n"
        "Examples:\n"
        "  linux>  %s -s 4 -E 1 -b 4 -t traces/yi.trace\n"
        "  linux>  %s -v -s 8 -E 2 -b 4 -t traces/yi.trace\n",
        prog, prog, prog
    );
}

char *scanArg(char *argv[], int *argi) {
    return argv[*argi][2] ? argv[*argi] + 2 : argv[++(*argi)];
}

int verbose,
    setBits, // Number of set index bits
    lineCnt, // Number of lines per set
    blockBits; // Number of block bits
char *fileName; // Name of the valgrind trace to replay

int hits, misses, evictions;

typedef struct {
    int valid;
    unsigned long tag;
    int time; // Last recent use time
} Line;
Line *cache;

// Alloc memory for data structure
void init() {
    size_t len = sizeof(Line) * lineCnt * (1 << setBits);
    cache = malloc(len);
    memset(cache, 0, len);
}

void touch(unsigned long setI, unsigned long tag, int time) {
    Line *line, *set = cache + setI * lineCnt;
    Line *available = NULL, *lru = NULL;
    for (line = set; line < set + lineCnt; ++line) {
        // cold line
        if (!line->valid) {
            available = line;
            continue;
        }

        // find the least recent used line
        if (!lru || line->time < lru->time) lru = line;

        if (line->tag != tag) continue;

        // hit!
        if (verbose) printf("hit ");
        ++hits;
        line->time = time;
        return;
    }

    // miss
    if (verbose) printf("miss ");
    ++misses;

    if (available) { // have cold line
        available->valid = 1;
        available->tag = tag;
        available->time = time;
    } else { // eviction lru line
        if (verbose) printf("eviction ");
        ++evictions;
        lru->tag = tag;
        lru->time = time;
    }
}

void walk(FILE *f) {
    char buf[BUF_SIZE];
    int time = 0, i;
    unsigned long addr, set, tag;

    while (fgets(buf, BUF_SIZE, f)) {
        if (buf[0] == 'I') continue;

        if (verbose) {
            for (i = 1; buf[i] != '\n'; ++i) putchar(buf[i]);
            putchar(' ');
        }

        // get addr from input
        addr = 0;
        for (i = 3; buf[i] != ','; ++i) {
            addr = (addr << 4) + (buf[i] > '9' ? buf[i] - 'a' + 10 : buf[i] - '0');
        }

        // calculate set index and tag
        set = (addr >> blockBits) & ((1 << setBits) - 1);
        tag = addr >> (blockBits + setBits);

        // three operations
        switch (buf[1]) {
        case 'L': touch(set, tag, ++time); break; // load
        case 'S': touch(set, tag, ++time); break; // store
        case 'M': touch(set, tag, ++time); touch(set, tag, ++time); break; // modify (load & store)
        }

        if (verbose) putchar('\n');
    }
}

int main(int argc, char *argv[]) {
    // Read command line arguments
    int argi;
    for (argi = 1; argi < argc && argv[argi][0] == '-'; ++argi) {
        switch (argv[argi][1]) {
        case 'h': printUsage(argv[0]); return 0;
        case 'v': verbose = 1; break;
        case 's': setBits = atoi(scanArg(argv, &argi)); break;
        case 'E': lineCnt = atoi(scanArg(argv, &argi)); break;
        case 'b': blockBits = atoi(scanArg(argv, &argi)); break;
        case 't': fileName = scanArg(argv, &argi); break;
        default:
            fprintf(stderr, "%s: invalid option -- '%c'\n", argv[0], argv[argi][1]);
            printUsage(argv[0]);
            return EXIT_FAILURE;
        }
    }

    // Check arguments
    if (!setBits || !lineCnt || !blockBits) {
        fprintf(stderr, "%s: Missing required command line argument\n", argv[0]);
        printUsage(argv[0]);
        return EXIT_FAILURE;
    }

    // Open trace file
    FILE *traceFile = fopen(fileName, "r");
    if (traceFile == NULL) {
        fprintf(stderr, "%s: No such file or directory\n", fileName);
        return EXIT_FAILURE;
    }

    init();
    walk(traceFile);
    fclose(traceFile);
    printSummary(hits, misses, evictions);
    free(cache);
    return 0;
}

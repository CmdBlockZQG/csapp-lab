/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "CmdBlock",
    /* First member's full name */
    "CmdBlockZQG",
    /* First member's email address */
    "CmdBlockZQG@github.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};


#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define WORD_SIZE 4
#define CLASS_NUM 13

// read and write a word at address p
#define GET(p) (*(unsigned int *)(p))
#define SET(p, val) (*(unsigned int *)(p) = (val))

#define GET_PTR(p) (*(void **)(p))
#define SET_PTR(p, val) (*(void **)(p) = (val))

// pack size, alloc bit, previous block alloc bit into a single word
#define PACK(size, prev_alloc, alloc) ((size) | ((prev_alloc) << 1) | (alloc))

#define BASE (mem_heap_lo()) // heap low addr
#define TOP (mem_heap_hi() + 1) // heap_brk (top block ptr)
#define CLASS_HEAD_BLK(class) GET_PTR(BASE + (class) * WORD_SIZE) // first block ptr of size class
#define SET_CLASS_HEAD_BLK(class, ptr) SET_PTR(BASE + (class) * WORD_SIZE, ptr)
#define FIRST_BLK (BASE + (CLASS_NUM + 1) * WORD_SIZE) // first block ptr in heap

#define BLK_HEADER_PTR(ptr) ((ptr) - WORD_SIZE)
#define BLK_FOOTER_PTR(ptr) ((ptr) + BLK_SIZE(ptr) - WORD_SIZE)
#define BLK_HEADER(ptr) GET(BLK_HEADER_PTR(ptr))
#define SET_BLK_HEADER(ptr, val) SET(BLK_HEADER_PTR(ptr), val)
#define SET_BLK_FOOTER(ptr, val) SET(BLK_FOOTER_PTR(ptr), val)
#define SET_BLK_ALLOC(ptr, val) SET_BLK_HEADER(ptr, (BLK_HEADER(ptr) & (~0x1)) | val)
#define SET_PREV_ALLOC(ptr, val) SET_BLK_HEADER(ptr, (BLK_HEADER(ptr) & (~0x2)) | (val << 1))

#define NEXT_FREE_BLK(ptr) GET_PTR((ptr) + WORD_SIZE) // next free block in size class link list of a free block
#define PREV_FREE_BLK(ptr) GET_PTR(ptr) // previous free block in size class link list of a free block
#define SET_NEXT_FREE_BLK(ptr, val) SET_PTR((ptr) + WORD_SIZE, val) // next free block in size class link list of a free block
#define SET_PREV_FREE_BLK(ptr, val) SET_PTR(ptr, val) // previous free block in size class link list of a free block
#define BLK_SIZE(ptr) (BLK_HEADER(ptr) & (~0x3)) // block size
#define BLK_PREV_ALLOC(ptr) ((BLK_HEADER(ptr) & (0x2)) >> 1) // is previous block in heap alloc
#define BLK_ALLOC(ptr) (BLK_HEADER(ptr) & 1) // is block alloc

#define NEXT_BLK(ptr) ((ptr) + BLK_SIZE(ptr) + WORD_SIZE) // next block in heap
#define PREV_BLK(ptr) ((ptr) - WORD_SIZE - GET((ptr) - 2 * WORD_SIZE)) // previous block in heap, valid only when !BLK_PREV_ALLOC

// align block size
static inline size_t align(size_t x) {
    if ((x & 0x7) <= 0x4) return MAX((x & (~0x7)) | 0x4, 0xc);
    else return (x & (~0x7)) + 0xc;
}

// get class index (0~12)
static inline int get_class(size_t x) {
    assert((x & 0x7) == 0x4);
    int i;
    x = (x >> 3) | 0x1;
    for (i = -1; x && i < CLASS_NUM - 1; ++i) x >>= 1;
    assert(i < CLASS_NUM);
    return i;
}

static void list_rm(void *blk) {
    int class = get_class(BLK_SIZE(blk));
    void *next_blk = NEXT_FREE_BLK(blk),
         *prev_blk = PREV_FREE_BLK(blk);

    if (prev_blk != NULL) SET_NEXT_FREE_BLK(prev_blk, next_blk);
    else SET_CLASS_HEAD_BLK(class, next_blk);

    if (next_blk != NULL) SET_PREV_FREE_BLK(next_blk, prev_blk);
}

static void list_push(void *blk, int size) {
    int class = get_class(size);
    void *head = CLASS_HEAD_BLK(class);
    if (head) SET_PREV_FREE_BLK(head, blk);

    SET_PREV_FREE_BLK(blk, NULL);
    SET_NEXT_FREE_BLK(blk, head);
    SET_CLASS_HEAD_BLK(class, blk);
}

static void alloc_block(void *blk, size_t size) {
    size_t free_size = BLK_SIZE(blk);
    if (free_size >= size + 4 * WORD_SIZE) {
        free_size -= size + WORD_SIZE;
        list_rm(blk);
        assert(BLK_PREV_ALLOC(blk));
        SET_BLK_HEADER(blk, PACK(size, 1, 1));

        void *free_blk = NEXT_BLK(blk);
        SET_BLK_HEADER(free_blk, PACK(free_size, 1, 0));
        SET_BLK_FOOTER(free_blk, free_size);
        list_push(free_blk, free_size);
    } else {
        SET_PREV_ALLOC(NEXT_BLK(blk), 1);
        SET_BLK_ALLOC(blk, 1);
        list_rm(blk);
    }
}

int compare_ptrs(const void *a, const void *b) {
    void *arg1 = *(void**)a;
    void *arg2 = *(void**)b;
 
    if (arg1 < arg2) return -1;
    if (arg1 > arg2) return 1;
    return 0;
}


void mm_check() {
    void *free_blks[20000];
    int free_blks_cnt = 0;
    for (int class = 0; class < CLASS_NUM; ++class) {
        for (void *blk = CLASS_HEAD_BLK(class); blk != NULL; blk = NEXT_FREE_BLK(blk)) {
            free_blks[free_blks_cnt++] = blk;
        }
    }
    qsort(free_blks, free_blks_cnt, sizeof(void *), compare_ptrs);
    int i = 0;
    for (void *blk = FIRST_BLK; blk < TOP; blk = NEXT_BLK(blk)) {
        assert(BLK_PREV_ALLOC(NEXT_BLK(blk)) == BLK_ALLOC(blk));
        if (BLK_ALLOC(blk)) {
        } else {
            assert(free_blks[i++] == blk);
            assert(BLK_SIZE(blk) == GET(BLK_FOOTER_PTR(blk)));
        }
    }
}


/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void) {
    void *p = mem_sbrk((CLASS_NUM + 1) * WORD_SIZE);
    // head of free list of every size class
    for (int i = 0; i < CLASS_NUM; ++i) {
        SET_PTR(p, NULL);
        p += WORD_SIZE;
    }
    SET(p, PACK(0, 1, 1)); // top block
    mm_check();
    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size) {
    size = align(size);
    void *blk;
    // go over every size class
    for (int class = get_class(size); class < CLASS_NUM; ++class) {
        for (blk = CLASS_HEAD_BLK(class); blk != NULL; blk = NEXT_FREE_BLK(blk)) {
            if (BLK_SIZE(blk) >= size) {
                alloc_block(blk, size);
                mm_check();
                return blk;
            }
        }
    }
    // need more space
    if (BLK_PREV_ALLOC(TOP)) {
        blk = mem_sbrk(size + WORD_SIZE);
    } else {
        blk = PREV_BLK(TOP);
        list_rm(blk);
        mem_sbrk(size - BLK_SIZE(blk));
        assert(BLK_PREV_ALLOC(blk) == 1);
    }
    SET_BLK_HEADER(blk, PACK(size, 1, 1));
    SET_BLK_HEADER(TOP, PACK(0, 1, 1));
    mm_check();
    return blk;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr) {
    void *blk = ptr;
    size_t size = BLK_SIZE(ptr);
    if (!BLK_PREV_ALLOC(ptr)) {
        void *prev_blk = PREV_BLK(ptr);
        assert(BLK_PREV_ALLOC(prev_blk));
        list_rm(prev_blk);
        size += BLK_SIZE(prev_blk) + WORD_SIZE;
        blk = prev_blk;
    }

    void *next_blk = NEXT_BLK(ptr);
    if (BLK_ALLOC(next_blk)) {
        SET_PREV_ALLOC(next_blk, 0);
    } else {
        assert(BLK_ALLOC(NEXT_BLK(next_blk)));
        list_rm(next_blk);
        size += BLK_SIZE(next_blk) + WORD_SIZE;
        // SET_PREV_ALLOC(NEXT_BLK(next_blk), 0);
    }
    SET_BLK_HEADER(blk, PACK(size, 1, 0));
    SET_BLK_FOOTER(blk, size);
    list_push(blk, size);
    mm_check();
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size) {
    size_t orig_size = BLK_SIZE(ptr);
    size = align(size);

    void *next_blk = NEXT_BLK(ptr);

    if (size <= orig_size) { // shrink current block
        // decide not to split current block, do nothing and return original ptr
        if (orig_size < size + 4 * WORD_SIZE) {
            mm_check();
            return ptr;
        }
        // split current block into two parts, the latter is free
        size_t free_size = orig_size - size - WORD_SIZE;
        if (BLK_ALLOC(next_blk)) { // next block is alloc
            SET_PREV_ALLOC(next_blk, 0);
        } else { // next block is free, combine the free space
            assert(BLK_ALLOC(NEXT_BLK(next_blk)));
            list_rm(next_blk);
            free_size += BLK_SIZE(next_blk) + WORD_SIZE;
        }
        SET_BLK_HEADER(ptr, PACK(size, BLK_PREV_ALLOC(ptr), 1));
        void *free_blk = NEXT_BLK(ptr);
        SET_BLK_HEADER(free_blk, PACK(free_size, 1, 0));
        SET_BLK_FOOTER(free_blk, free_size);
        list_push(free_blk, free_size);
        mm_check();
        return ptr;
    }

    // current block needs expansion
    // next block is free and total space is enough for expansion
    if (!BLK_ALLOC(next_blk) && orig_size + BLK_SIZE(next_blk) + WORD_SIZE >= size) {
        alloc_block(next_blk, size - orig_size - WORD_SIZE);
        SET_BLK_HEADER(ptr, PACK(size, BLK_PREV_ALLOC(ptr), 1));
        mm_check();
        return ptr;
    }

    // need to do malloc & copy & free
    void *new_blk = mm_malloc(size);
    memcpy(new_blk, ptr, orig_size);
    mm_free(ptr);
    mm_check();
    return new_blk;
}
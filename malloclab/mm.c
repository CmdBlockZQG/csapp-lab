/*
 * mm-naive.c - segregated fit malloc
 * 
 * the pointer of a block is right after header (&header + 8B)
 * the size of a block is the length of the whole block except the header (tot_len - 8B)
 * the size of every block is aligned to 0b100 (size & 0x7 == 0x4)
 * 
 * 
 * free block structure
 * 4B    header          size | (prev_alloc << 1) | alloc
 *           30b    size of block
 *           1b     if previous block allocated
 *           1b     if this block allocated
 * 4B    prev_pointer    point to the previous block in free list
 * 4B    next_pointer    point to the next block in free list
 * ?B    data & fill
 * 4B    footer          size of block
 * 
 * 
 * allocated block structure
 * 4B    header          size | (prev_alloc << 1) | alloc
 *           30b    size of block
 *           1b     if previous block allocated
 *           1b     if this block allocated
 * ?B    data & fill
 * 
 * 
 * heap structure
 * 4B    pointer to first block in free list of size class 0
 * 4B    pointer to first block in free list of size class 1
 * 4B    pointer to first block in free list of size class 2
 *       ...
 * 4B    pointer to first block in free list of size class 12
 * -- blocks --
 * -- blocks --
 * 4B    virtual last block, only has a header(0x3), size=0
 * 
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
#define CLASS_NUM 13 // number of size classes

// read and write a word at address p
#define GET(p) (*(unsigned int *)(p))
#define SET(p, val) (*(unsigned int *)(p) = (val))

// read and write a pointer at address p
#define GET_PTR(p) (*(void **)(p))
#define SET_PTR(p, val) (*(void **)(p) = (val))

// pack size, alloc bit, previous block alloc bit into a single word
#define PACK(size, prev_alloc, alloc) ((size) | ((prev_alloc) << 1) | (alloc))

#define BASE (mem_heap_lo()) // heap low addr
#define TOP (mem_heap_hi() + 1) // heap_brk (top block ptr)
#define CLASS_HEAD_BLK(class) GET_PTR(BASE + (class) * WORD_SIZE) // first block of size class
#define SET_CLASS_HEAD_BLK(class, ptr) SET_PTR(BASE + (class) * WORD_SIZE, ptr) // set first block of size class
#define FIRST_BLK (BASE + (CLASS_NUM + 1) * WORD_SIZE) // first block ptr in heap

#define BLK_HEADER_PTR(ptr) ((ptr) - WORD_SIZE) // get ptr of block header
#define BLK_FOOTER_PTR(ptr) ((ptr) + BLK_SIZE(ptr) - WORD_SIZE) // get ptr of block footer, valid only when block is free
#define BLK_HEADER(ptr) GET(BLK_HEADER_PTR(ptr)) // get block header word
#define SET_BLK_HEADER(ptr, val) SET(BLK_HEADER_PTR(ptr), val) // set block header
#define SET_BLK_FOOTER(ptr, val) SET(BLK_FOOTER_PTR(ptr), val) // set block footer
#define SET_BLK_ALLOC(ptr, val) SET_BLK_HEADER(ptr, (BLK_HEADER(ptr) & (~0x1)) | val) // mark this block as allocated
// mark in this block's header that previous block is allocated
#define SET_PREV_ALLOC(ptr, val) SET_BLK_HEADER(ptr, (BLK_HEADER(ptr) & (~0x2)) | (val << 1))

#define NEXT_FREE_BLK(ptr) GET_PTR((ptr) + WORD_SIZE) // next free block in size class link list of a free block
#define PREV_FREE_BLK(ptr) GET_PTR(ptr) // previous free block in size class link list of a free block
#define SET_NEXT_FREE_BLK(ptr, val) SET_PTR((ptr) + WORD_SIZE, val) // next free block in size class link list of a free block
#define SET_PREV_FREE_BLK(ptr, val) SET_PTR(ptr, val) // previous free block in size class link list of a free block
#define BLK_SIZE(ptr) (BLK_HEADER(ptr) & (~0x3)) // block size
#define BLK_PREV_ALLOC(ptr) ((BLK_HEADER(ptr) & (0x2)) >> 1) // is previous block in heap allocated?
#define BLK_ALLOC(ptr) (BLK_HEADER(ptr) & 1) // is this block allocated?

#define NEXT_BLK(ptr) ((ptr) + BLK_SIZE(ptr) + WORD_SIZE) // next block in heap
#define PREV_BLK(ptr) ((ptr) - WORD_SIZE - GET((ptr) - 2 * WORD_SIZE)) // previous block in heap, valid only when !BLK_PREV_ALLOC

/*
 * align - align block size 'x' to a number greater than 'x' and has lower bits 0b100
 */
static inline size_t align(size_t x) {
    if ((x & 0x7) <= 0x4) return MAX((x & (~0x7)) | 0x4, 0xc);
    else return (x & (~0x7)) + 0xc;
}

/*
 * get_class - get the index of size class if block size is 'x'
 *     return value 0 ~ CLASS_NUM - 1 
 */
static inline int get_class(size_t x) {
    // assert((x & 0x7) == 0x4);
    int i;
    x = (x >> 3) | 0x1;
    for (i = -1; x && i < CLASS_NUM - 1; ++i) x >>= 1;
    // assert(i < CLASS_NUM);
    return i;
}

/*
 * list_rm - remove a free block from its size class's free list, which is a link list
 */
static void list_rm(void *blk) {
    int class = get_class(BLK_SIZE(blk));
    void *next_blk = NEXT_FREE_BLK(blk),
         *prev_blk = PREV_FREE_BLK(blk);

    // remove an item from a link list
    if (prev_blk != NULL) SET_NEXT_FREE_BLK(prev_blk, next_blk);
    else SET_CLASS_HEAD_BLK(class, next_blk); // remove the first item of the link list, reset list head

    if (next_blk != NULL) SET_PREV_FREE_BLK(next_blk, prev_blk);
}

/*
 * list_push - add a free block to the front of its size class's free list, which is a link list
 */
static void list_push(void *blk, int size) {
    int class = get_class(size);
    void *head = CLASS_HEAD_BLK(class);
    if (head) SET_PREV_FREE_BLK(head, blk); // the original head block becomes the second

    SET_PREV_FREE_BLK(blk, NULL); // 'blk' becomes the first block in the link list
    SET_NEXT_FREE_BLK(blk, head);
    SET_CLASS_HEAD_BLK(class, blk); // reset list head 
}

/*
 * alloc_block - take a free block 'blk', whose size is larger than 'size', mark it as allocated
 *     if the block is large enough, split it into two blocks
 *     the former has size 'size' and the latter is free
 */
static void alloc_block(void *blk, size_t size) {
    size_t free_size = BLK_SIZE(blk); // total free space
    if (free_size >= size + 4 * WORD_SIZE) { // big enough to split
        free_size -= size + WORD_SIZE;
        list_rm(blk);
        // assert(BLK_PREV_ALLOC(blk));
        SET_BLK_HEADER(blk, PACK(size, 1, 1));

        // create new free block
        void *free_blk = NEXT_BLK(blk);
        SET_BLK_HEADER(free_blk, PACK(free_size, 1, 0));
        SET_BLK_FOOTER(free_blk, free_size);
        list_push(free_blk, free_size);
    } else { // not split this block
        // mark the block as allocated
        SET_PREV_ALLOC(NEXT_BLK(blk), 1);
        SET_BLK_ALLOC(blk, 1);
        list_rm(blk);
    }
}

/*
 * compare_ptrs - compare two pointers, used in mm_check
 */
int compare_ptrs(const void *a, const void *b) {
    void *arg1 = *(void**)a;
    void *arg2 = *(void**)b;
 
    if (arg1 < arg2) return -1;
    if (arg1 > arg2) return 1;
    return 0;
}

/*
 * mm_check - use assert to check
 *     1. if all the free blocks in heap are in the free list
 *     2. if all blocks in free list are free 
 *     3. if there are contiguous free blocks that escaped coalescing
 * all the blocks in heap are adjacent, so there's no overlap
 * any invalid pointer will trigger a segmentation fault
 */
void mm_check() {
    void *free_blks[20000]; // store all block pointers in free list
    int free_blks_cnt = 0;
    for (int class = 0; class < CLASS_NUM; ++class) {
        // walk through all the link list
        for (void *blk = CLASS_HEAD_BLK(class); blk != NULL; blk = NEXT_FREE_BLK(blk)) {
            free_blks[free_blks_cnt++] = blk;
        }
    }
    // sort all the pointers by address
    qsort(free_blks, free_blks_cnt, sizeof(void *), compare_ptrs);
    int i = 0;
    // walk through all the blocks, for they're adjacent in heap
    for (void *blk = FIRST_BLK; blk < TOP; blk = NEXT_BLK(blk)) {
        assert(!BLK_ALLOC(blk) || !BLK_ALLOC(NEXT_BLK(blk))); // no two adjacent blocks are both free
        assert(BLK_PREV_ALLOC(NEXT_BLK(blk)) == BLK_ALLOC(blk)); // check for alloc bit consistency
        if (!BLK_ALLOC(blk)) { // as for free blocks ...
            assert(free_blks[i++] == blk); // free blocks in heap and those in free list are birefringence
            assert(BLK_SIZE(blk) == GET(BLK_FOOTER_PTR(blk))); // sizes in header and footer are identical
        }
    }
}

/*
 * print_heap - print all the block in the heap, for debugging
 */
void print_heap() {
    printf("HEAP\n");
    for (void *blk = FIRST_BLK; blk < TOP; blk = NEXT_BLK(blk)) {
        printf("%d %d\n", BLK_ALLOC(blk), BLK_SIZE(blk));
    }
}

/* 
 * mm_init - initialize the malloc package
 *    build head pointers for all 13 size classes
 *    and a 'virtual' block, which marks the end of the whole heap
 */
int mm_init(void) {
    void *p = mem_sbrk((CLASS_NUM + 1) * WORD_SIZE);
    // head of free list of every size class
    for (int i = 0; i < CLASS_NUM; ++i) {
        SET_PTR(p, NULL);
        p += WORD_SIZE;
    }
    SET(p, PACK(0, 1, 1)); // 'virtual' last block
    return 0;
}

/* 
 * mm_malloc - allocate a block
 *     1. start from its own size class, try to find a free block that larger than size
 *        in every size class, just use first-fit
 *     2. if there's no large ehough free block, check if the last block in heap is free
 *        if so, expand the last free block to fit the size demand
 *     3. otherwise, simply expand the heap, create a free block at the end of heap
 */
void *mm_malloc(size_t size) {
    size = align(size);
    void *blk;
    // go over every size class
    for (int class = get_class(size); class < CLASS_NUM; ++class) {
        for (blk = CLASS_HEAD_BLK(class); blk != NULL; blk = NEXT_FREE_BLK(blk)) {
            if (BLK_SIZE(blk) >= size) { // find the first fit block
                alloc_block(blk, size); // allocate the block we found
                return blk;
            }
        }
    }
    // need more space
    size += 0x10; // make some room for future small expansion
    if (BLK_PREV_ALLOC(TOP)) { // the last block is allocated
        blk = mem_sbrk(size + WORD_SIZE); // make a new block
    } else { // expand the last free block
        blk = PREV_BLK(TOP);
        list_rm(blk); // not free any more, remove from free list
        mem_sbrk(size - BLK_SIZE(blk));
        // assert(BLK_PREV_ALLOC(blk) == 1);
    }
    SET_BLK_HEADER(blk, PACK(size, 1, 1)); // create the block header
    SET_BLK_HEADER(TOP, PACK(0, 1, 1)); // recreate the 'virtual' last block 
    return blk;
}

/*
 * mm_free - free a block, and combine it with adjacent blocks if possible
 *     there're no contiguous free blocks, so dealing with previous and next is enough
 */
void mm_free(void *ptr) {
    void *blk = ptr;
    size_t size = BLK_SIZE(ptr);

    // previous block is free
    if (!BLK_PREV_ALLOC(ptr)) {
        void *prev_blk = PREV_BLK(ptr);
        // assert(BLK_PREV_ALLOC(prev_blk));
        list_rm(prev_blk);
        size += BLK_SIZE(prev_blk) + WORD_SIZE;
        blk = prev_blk; // combine with previous block
    }

    void *next_blk = NEXT_BLK(ptr);
    if (BLK_ALLOC(next_blk)) { // next block is allocated
        SET_PREV_ALLOC(next_blk, 0); // mark current block as free
    } else { // next block is free
        // assert(BLK_ALLOC(NEXT_BLK(next_blk)));
        list_rm(next_blk); // combine with next block
        size += BLK_SIZE(next_blk) + WORD_SIZE;
        // SET_PREV_ALLOC(NEXT_BLK(next_blk), 0);
    }
    // create new free block
    SET_BLK_HEADER(blk, PACK(size, 1, 0));
    SET_BLK_FOOTER(blk, size);
    list_push(blk, size);
}

/*
 * mm_realloc - resize an allocated block
 *     1. if new size is smaller, shrink current block
 *        split it into two blocks with the latter one free if possible
 *     2. if the next block is free, and the space combined is enough,
 *        then combine current block with the free block.
 *        split the next block into two blocks with the latter one free if possible
 *     3. if current block is the last block, expend heap for extra space
 *     4. otherwise, do malloc & copy & free
 */
void *mm_realloc(void *ptr, size_t size) {
    size_t orig_size = BLK_SIZE(ptr);
    size = align(size);

    void *next_blk = NEXT_BLK(ptr);

    if (size <= orig_size) { // shrink current block
        // decide not to split current block, do nothing and return original ptr
        if (orig_size < size + 4 * WORD_SIZE) return ptr;
        // split current block into two parts, the latter is free
        size_t free_size = orig_size - size - WORD_SIZE;
        if (BLK_ALLOC(next_blk)) { // next block is alloc
            SET_PREV_ALLOC(next_blk, 0);
        } else { // next block is free, combine the free space
            // assert(BLK_ALLOC(NEXT_BLK(next_blk)));
            list_rm(next_blk);
            free_size += BLK_SIZE(next_blk) + WORD_SIZE;
        }
        // recreate block header
        SET_BLK_HEADER(ptr, PACK(size, BLK_PREV_ALLOC(ptr), 1));
        // create the new free block
        void *free_blk = NEXT_BLK(ptr);
        SET_BLK_HEADER(free_blk, PACK(free_size, 1, 0));
        SET_BLK_FOOTER(free_blk, free_size);
        list_push(free_blk, free_size); // add to free list
        return ptr;
    }

    // current block needs expansion
    // next block is free and total space is enough for expansion
    if (!BLK_ALLOC(next_blk) && orig_size + BLK_SIZE(next_blk) + WORD_SIZE >= size) {
        // assert(BLK_ALLOC(NEXT_BLK(next_blk)));
        alloc_block(next_blk, size - orig_size - WORD_SIZE); // will do split automatically if possible
        // recreate block header with a different size
        SET_BLK_HEADER(ptr, PACK(orig_size + BLK_SIZE(next_blk) + WORD_SIZE, BLK_PREV_ALLOC(ptr), 1));
        return ptr;
    }

    // current block is the last block, expend heap
    if (next_blk == TOP) {
        mem_sbrk(size - orig_size);
        // recreate block header with a different size
        SET_BLK_HEADER(ptr, PACK(size, BLK_PREV_ALLOC(ptr), 1));
        // reset the 'virtual' last block
        SET_BLK_HEADER(TOP, PACK(0, 1, 1));
        return ptr;
    }

    // need to do malloc & copy & free
    void *new_blk = mm_malloc(size);
    memcpy(new_blk, ptr, orig_size);
    mm_free(ptr);
    return new_blk;
}
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

team_t team = {
    /* Team name */
    "ateam",
    /* First member's full name */
    "Harry Bovik",
    /* First member's email address */
    "bovik@cs.cmu.edu",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* Basic marcos */
#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (mem_pagesize())
#define ALIGN(size) (((size) + (DSIZE-1)) & ~0x7)
#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define MAX(x, y) ((x) > (y) ? (x) : (y))
/* Pack size and alloc to a word */
#define PACK(size, alloc) ((size) | (alloc))
/* Get a word at p */
#define GET(p) (*(unsigned int *)(p))
/* Put a word at p */
#define PUT(p, val) (*(unsigned int *)(p) = val)
/* Get size at p */
#define GET_SIZE(p) (GET(p) & ~0x7)
/* Get alloc at p */
#define GET_ALLOC(p) (GET(p) & 0x1)
/* Get header of bp */
#define HDRP(bp) ((char *)(bp) - WSIZE)
/* Get footer of bp via size in HDRP */
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)
/* Get next blk via size info in HDRP of bp */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)))
/* Get prev blk via size info in FTRP of prev blk */
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))
/* Start point */
static void* heap_listp;
/* Try to coalesce prev and next */
static void *coalesce(void *bp)
{
    /* Alloc stats of prev and next blk */
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    /* Size of current blk */
    size_t size = GET_SIZE(HDRP(bp));
    /* Set header and footer accordingly */
    if (prev_alloc && next_alloc) {
        return bp;
    }
    else if (prev_alloc && !next_alloc) {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    else if (!prev_alloc && next_alloc) {
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    else {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp))) + GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    return bp;
}

static void *extend_heap(size_t words)
{
    char *bp;
    /* aligned to DWORD */
    size_t sz = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(sz)) == -1)
        return NULL;
    /* Set free block header, footer... */
    PUT(HDRP(bp), PACK(sz, 0));
    PUT(FTRP(bp), PACK(sz, 0));
    /* ...and epilogue header */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));
    return coalesce(bp);
}

int mm_init(void)
{
    /* Init heap structure */
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1)
        return -1;
    PUT(heap_listp, 0);
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1));
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1));
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1));
    heap_listp += (2 * WSIZE);

    if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
        return -1;
    return 0;
}

static void *find_fit(size_t asize)
{
    char *bp = mem_heap_lo();
    while (bp < (char *)mem_heap_hi())
    {
        if (GET_ALLOC(HDRP(bp))) {
            size_t bpsz = GET_SIZE(HDRP(bp));
            if (bpsz >= asize + 2 * DSIZE)
                return bp;
        }
        bp = NEXT_BLKP(bp);
    }
    return NULL;
}

static void place(void *bp, size_t asize)
{
    size_t sz = GET_SIZE(HDRP(bp));
    if (sz - asize >= 2 * DSIZE) {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        sz -= asize;
        PUT(HDRP(NEXT_BLKP(bp)), PACK(sz, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(sz, 0));
    } else {
        PUT(HDRP(bp), PACK(sz, 1));
        PUT(FTRP(bp), PACK(sz, 1));
    }
}

void *mm_malloc(size_t size)
{
    size_t asize, extendsize;
    char *bp;

    if (size == 0)
        return NULL;
    if (size <= DSIZE)
        asize = 2 * DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);

    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }

    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;
    place(bp, asize);
    return bp;
}

void mm_free(void *ptr)
{
    size_t sz = GET_SIZE(HDRP(ptr));
    PUT(HDRP(ptr), PACK(sz, 0));
    coalesce(ptr);
    return;
}

void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    
    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;
    copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}




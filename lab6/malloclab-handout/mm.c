/* 
 * Author:
 * Name: Haoyang Yuan
 * Andrew ID: haoyangy
 *
 * Description:
 * Simple, 32-bit and 64-bit clean allocator based on segregated 
 * explict lists, first-fit placement, and boundary tag coalescing,
 * Blocks must be aligned to doubleword (8 byte) boundaries. 
 * Minimum block size is 16 bytes.
 * 
 * Implementation : Segregated List / First fit
 *
 * Block:
 * Each block have a 4 byte header and 4 byte footer.
 * One bit is for indicating allocated(0x1) or not(0x0)
 * The rest part of the header and footer contains the size of the block
 * | size=31bit | alloc= 1bit | content | size=31bit | alloc= 1bit |
 * For free block, there are a 4 byte next pointer and 4 byte prev pointer
 * The pointer contains the offset bytes from the heap start position
 * | size | alloc | next offset=32 bit | prev offset=32 bit | size | alloc |
 *
 * Freelist:
 * There are 8 free lists, each is corresponding to certain size.
 * In the prologue part, there are 8 bytes, each have two 4 bytes part.
 * The first 4 bytes part contains the offset of first block in the list
 * The scond 4 bytes part is the prev pointer pointed to the head itself
 * When the new block is freed, it will be added to the first place LIFO policy
 * When a block is deleted, it only needs to redirect pointers of prev and next blocks
 *
 * 
 * Debug:
 * Using the mm_heapcheck function to check all the environments
 * at that time, including heap check, block check, and list check
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mm.h"
#include "memlib.h"

/* do not change the following! */
#ifdef DRIVER
/* create aliases for driver tests */
#define malloc mm_malloc
#define free mm_free
#define realloc mm_realloc
#define calloc mm_calloc
#endif /* def DRIVER */

/* Basic constants and macros */
#define WSIZE            4          /* Word and header/footer size (bytes) */
#define DSIZE            8          /* Doubleword size (bytes) */
#define CHUNKSIZE        (1 << 8)   /* Extend heap by this amount (bytes) */
#define LISTNUM        8            /* Number of lists in segregate list*/

/* round up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size_t)(size) + (7)) & ~0x7)

/* find max one  */
#define MAX(x, y)       ((x) > (y) ? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc)   ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p)          (*(unsigned int *)(p))
#define PUT(p, val)     (*(unsigned int *)(p) = (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(bp)        (GET(bp) & ~0x7)
#define GET_ALLOC(bp)       (GET(bp) & 0x1)

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp)            ((char *)(bp) - WSIZE)
#define FTRP(bp)            ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and prev block */
#define NEXT_BLKP(bp)       ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp)       ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

/* Given block ptr bp, compute its next and prev pointer and position of free block */
#define NEXT_PTR(bp)        (bp)
#define PREV_PTR(bp)        ((char *)(bp) + WSIZE)
#define NEXT_POS(bp)  (heap_star + (*(unsigned int *)(NEXT_PTR(bp))))
#define PREV_POS(bp)  (heap_star + (*(unsigned int *)(PREV_PTR(bp))))

/* Global Variables */
static char *heap_listp = NULL;     // heap start and then move to prologue
static char *heap_star = NULL;      // heap start address
static char *epilogue;              // epilogue part

/* Function prototypes for internal helper routines */
static void place(void *bp, size_t asize);
static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static inline char *chooselist(size_t asize);
static inline void addblock(void *bp, char *free_list_head);
static inline void deleteblock(void *bp);
void mm_checkheap(int lineno);
static size_t getprealloc(void* bp);

/*
 * mm_init - Initialize the memory manager
 */
int mm_init(void)
{
    /* Create the initial empty heap */
    /* Prologue part */
    size_t prologue_size = LISTNUM * DSIZE + DSIZE;
    
    /* extend space for Prologue + Lists + Epilogue */
    if ((heap_listp = mem_sbrk(prologue_size + DSIZE)) == (void *)-1)
    {
        return -1;
    }
    /* The basement of address */
    heap_star = heap_listp;
    
    PUT(heap_listp, 0);
    heap_listp += DSIZE;
    
    /* Prologue header */
    PUT(heap_star + WSIZE, PACK(prologue_size, 1));
    /* Prologue footer */
    PUT(heap_star + prologue_size,PACK(prologue_size, 1));
    
    /* List Header and Footer part*/
    /* 8byte for each free list head and tail pointers */
    for (int i = 0; i < LISTNUM; ++i) {
        size_t offset = (i+1) * DSIZE;
        PUT(heap_star + offset, offset);
        PUT(heap_star + offset + WSIZE, offset);
    }
    
    /* Epilogue part */
    epilogue = heap_star + prologue_size + WSIZE;
    PUT(epilogue, PACK(0, 1));
    
    /* initial extend */
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL) {
        return -1;
    }
    return 0;
}

/*
 * malloc - Allocate a block with at least size bytes of payload
 */
void *malloc(size_t size)
{
    size_t asize;      /* Adjusted block size */
    size_t extendsize; /* Amount to extend heap if no fit */
    char *bp;
    
    if (heap_listp == 0){
        mm_init();
    }
    /* Ignore spurious requests */
    if (size == 0)
        return NULL;
    
    /* Adjust block size to include overhead and alignment reqs. */
    if (size <= DSIZE)
        asize = 2*DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);
    
    /* Search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }
    
    /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize,CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;
    place(bp, asize);
    return bp;
}


/*
 * free - Free a block
 */
void free(void *bp)
{
    if (bp == 0)
        return;
    
    size_t size = GET_SIZE(HDRP(bp));
    if (heap_listp == 0){
        mm_init();
    }
    
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    coalesce(bp);
}

/*
 * realloc - Process a former allocated ptr to free or change the size
 */
void *realloc(void *ptr, size_t size)
{
    size_t oldsize;
    void *newptr;
    
    /* If size == 0 then this is just free, and we return NULL. */
    if(size == 0) {
        mm_free(ptr);
        return 0;
    }
    
    /* If oldptr is NULL, then this is just malloc. */
    if(ptr == NULL) {
        return mm_malloc(size);
    }
    
    newptr = mm_malloc(size);
    
    /* If realloc() fails the original block is left untouched  */
    if(!newptr) {
        return 0;
    }
    
    /* Copy the old data. */
    oldsize = GET_SIZE(HDRP(ptr));
    if(size < oldsize) oldsize = size;
    memcpy(newptr, ptr, oldsize);
    
    /* Free the old block. */
    mm_free(ptr);
    
    return newptr;
}


/*
 * find_fit - Find the First fit block from the corresponding list
 */
static void *find_fit(size_t asize)
{
    void *bp;
    char *list = chooselist(asize);
    
    /* first fit policy, tranverse all the lists in outer loop */
    /* tranverse each list in inner loop */
    for (; list != heap_listp +  LISTNUM * DSIZE; list += DSIZE) {
        for (bp = NEXT_POS(list);bp!=list;bp = NEXT_POS(bp)) {
            if (asize <= GET_SIZE(HDRP(bp))) {
                return bp;
            }
        }
    }
    return NULL;
}


/*
 * chooselist - Helper function that choose the head of list 
 * corresponding to certain size
 * 
 */
static inline char *chooselist(size_t asize)
{
    size_t offset;
    if (asize <= 16)
        offset = 1;
    else if (asize>16 && asize <= 32)
        offset = 2;
    else if (asize>32 && asize <= 48)
        offset = 3;
    else if (asize>48 && asize <= 64)
        offset = 4;
    else if (asize>64 && asize <= 80)
        offset = 5;
    else if (asize>80 && asize <= 128)
        offset = 6;
    else if (asize>128 && asize <= 256)
        offset = 7;
    else
        offset = 8;
    
    /* return the header address */
    return heap_star + (offset * DSIZE);
}


/*
 * addblock - Helper function to insert block to free list
 */
static inline void addblock(void *bp, char *head)
{
    /* insert to the first place pointed by list head */
    /* make this block points to the current first block */
    PUT(NEXT_PTR(bp), GET(NEXT_PTR(head)));
    PUT(PREV_PTR(bp), GET(PREV_PTR(NEXT_POS(head))));
    size_t offset = (size_t)bp - (size_t)heap_star;
    
    /* make head point to this block */
    PUT(NEXT_PTR(head), offset);
    PUT(PREV_PTR(NEXT_POS(bp)), offset);
}

/*
 * deleteblock - Helper function to delete block from free list
 */
static inline void deleteblock(void *bp)
{
    /* change the pointer of pre and next block*/
    PUT(NEXT_PTR(PREV_POS(bp)), GET(NEXT_PTR(bp)));
    PUT(PREV_PTR(NEXT_POS(bp)), GET(PREV_PTR(bp)));
}

/*
 * place - Place block of asize bytes at start of free block bp
 *         and split if remainder would be at least minimum block size
 */
static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));
    
    /* split current block to make on free block */
    if ((csize - asize) >= (2*DSIZE)) {
        deleteblock(bp);
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        
        /* store extra free block */
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize - asize, 0));
        PUT(FTRP(bp), PACK(csize - asize, 0));
        
        char *list = chooselist(csize - asize);
        addblock(bp, list);
        
    }
    
    /* use current block */
    else {
        deleteblock(bp);
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
    
    
}


/*
 * extend_heap - Extend heap with free block and return its block pointer
 */
static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;
    
    /* Allocate an even number of words to maintain alignment */
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;
    
    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));
    
    /* Coalesce if the previous block was free */
    return coalesce(bp);
}
/*
 * getprealloc - return if the prev block is allocated
 */
static size_t getprealloc(void* bp){
    size_t prev_alloc;
    
    /* reach the prologue part, prev block is allocated */
    if((size_t)(PREV_BLKP(bp)) - (size_t)(heap_star) <= 9)
        prev_alloc = 0x1;
    
    /* check if allocated from header */
    else {
        prev_alloc = GET_ALLOC(HDRP(PREV_BLKP(bp)))?0x1:0x0 ;
    }
    return prev_alloc;
}

/*
 * coalesce - Boundary tag coalescing. Return ptr to coalesced block
 */
static void *coalesce(void *bp)
{
    /* Get initial size of the block */
    /* check if prev and next block is allocated */
    size_t size = GET_SIZE(HDRP(bp));
    size_t prev_alloc = getprealloc(bp);
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    
    /*
     * case 1 - both sides are allocated
     */
    if(prev_alloc && next_alloc) {
    /* do nothing, no need to coalesce */
    }
    
    /*
     * case 2 - only prev block is allocated
     */
    else if(prev_alloc && !next_alloc) {
        
        /* extend size and repack the header footer information*/
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        deleteblock(NEXT_BLKP(bp));
        
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size,0));
    }
    
    
    /*
     * case 3 - only next block is allocated
     */
    else if(!prev_alloc && next_alloc) {
        
        /* extend size and repack the header footer information*/
        size+= GET_SIZE(HDRP(PREV_BLKP(bp)));
        deleteblock(PREV_BLKP(bp));
        
        PUT(HDRP(PREV_BLKP(bp)),PACK(size,  0));
        PUT(FTRP(bp), PACK(size,  0));
        bp = PREV_BLKP(bp);
    }
    
    /*
     * case 3 - both sides are not allocated
     */
    else {
        
        /* extend size and repack the header footer information*/
        size += GET_SIZE(HDRP(PREV_BLKP(bp)))+ GET_SIZE(HDRP(NEXT_BLKP(bp)));
        
        deleteblock(NEXT_BLKP(bp));
        deleteblock(PREV_BLKP(bp));
        
        PUT(HDRP(PREV_BLKP(bp)),PACK(size,  0));
        PUT(FTRP(NEXT_BLKP(bp)),PACK(size,  0));
        
        bp = PREV_BLKP(bp);
        
    }
  
    /* insert new free block to free list */
    char *list = chooselist(size);
    addblock(bp, list);
    return (bp);
    
}

/*
 * calloc - Allocate the block and set it to zero.
 */
void *calloc (size_t nmemb, size_t size)
{
    size_t bytes = nmemb * size;
    void *newptr;
    
    newptr = malloc(bytes);
    memset(newptr, 0, bytes);
    
    return newptr;
}

/*
 * printblock - Print block information
 */
static void printblock(void *bp)
{
    size_t hsize, halloc, fsize, falloc;
    
    /* get block information */
    hsize = GET_SIZE(HDRP(bp));
    halloc = GET_ALLOC(HDRP(bp));
    fsize = GET_SIZE(FTRP(bp));
    falloc = GET_ALLOC(FTRP(bp));
    
    if (hsize == 0) {
        printf("%p: epilogue\n", bp);
        return;
    }
    
    /* print block information */
    printf("block %p: header: [%zu:%c] footer: [%zu:%c]\n", bp,
           hsize, (halloc ? 'a' : 'f'), fsize, (falloc ? 'a' : 'f'));
}

/*
 * checkoneblock - check one block, if the header, footer and size
 * information meet the request
 *
 */
static void checkoneblock(void *bp)
{
    /* get block information */
    size_t hsize = GET_SIZE(HDRP(bp));
    size_t halloc = GET_ALLOC(HDRP(bp));
    size_t fsize = GET_SIZE(FTRP(bp));
    size_t falloc = GET_ALLOC(FTRP(bp));
    
    /* check position alignment */
    if((size_t)bp % 8){
        printf("Error: %p is not doubleword aligned\n", bp);
    }
    
    /* check if block boundry */
    if (!(bp <= mem_heap_hi() && bp >= mem_heap_lo())){
        printf("Error: %p out of range [%p:%p]\n",bp, mem_heap_lo(), mem_heap_hi());
    }
    
    /* Check if header and footer match */
    if(halloc!=falloc){
        printf("Error: %p the alloc label of this block is inconsistent\n",bp);
    }
    
    if(hsize!=fsize){
        printf("Error: %p the size label of this block is inconsistent\n",bp);
    
    }
    
    /* Check block's size's alignment */
    if (hsize & 0x7) {
        printf( "Error: %p the size is %zu, not alligned\n" , bp, hsize);
    }
    
    return;
}

/*
 * checkfreelist - check all free lists to see they meet the request
 */
static void checkfreelist(size_t *count)
{
    char *list = heap_listp;
    char *bp;

    /* tranverse through all the blocks in free lists */
    for (; list != heap_listp + LISTNUM * DSIZE; list += DSIZE) {
        for (bp = NEXT_POS(list); bp!=list;bp = NEXT_POS(bp)) {
            
            printblock(bp);
            checkoneblock(bp);
            (*count)++;
            
            /* Check if block with certain size is in the corresponding list */
            size_t size = GET_SIZE(HDRP(bp));
            char* templist = chooselist(size);
            if (templist!=list) {
               printf( "Error: %p the free block in wrong range %p!=%p \n",bp,list,templist);
                return;
            }
            
            /* check prev and next ptr consistency */
            if ( bp != PREV_POS(NEXT_POS(bp))) {
                printf( "Error: %p the free block prev next pointer mismatch %p!=%p",bp,
                       NEXT_POS(bp), PREV_POS(NEXT_POS(bp)));
                return;
            }
        }
    }
    return;
}

/*
 * checkallblock - tranverse through heap to check all the blocks
 *
 */
static void checkallblock(size_t *count)
{
    void *bp = heap_listp;
    size_t recentfree = 1;
    
    /* tranverse through all the blocks in heap */
    while (GET_SIZE(HDRP(bp))!=0) {
        /* ignore the prologue part */
        if((size_t)bp > (size_t)(heap_star + DSIZE + LISTNUM * DSIZE + WSIZE)) {
            
            size_t alloc = GET_ALLOC(HDRP(bp));
            
            /* check each block */
            printblock(bp);
            checkoneblock(bp);

            /* count the free block */
            if (alloc==0) {
                (*count)++;
                if (recentfree > 1 ) {
                    printf( "Error: %p has consecutive block, need coalesce\n",bp);
                    return ;
                }
                recentfree++;
            }
            /* meet an allocated block */
            else {
                recentfree = 1;
            }
        }
        
        /* point to next block */
        bp = NEXT_BLKP(bp);
    }
    return;
}

/*
 * mm_checkheap - Check the heap for correctness. Helpful hint: You
 *                can call this function using mm_checkheap(__LINE__);
 *                to identify the line number of the call site.
 */
void mm_checkheap(int lineno)
{

    
    /* check prologue */
    
    char *prologue = heap_listp;
    size_t prologue_size = LISTNUM * DSIZE + DSIZE;
    
    checkoneblock(prologue);
    printblock(prologue);
    
    if (GET_SIZE(HDRP(prologue)) != prologue_size) {
        printf( "Error: %p prologue size error",prologue);
    }
    
    if (!GET_ALLOC(HDRP(prologue))) {
        printf( "Error: %p prologue alloc error",prologue);
    }
    
    /* check all the blocks from lists and heap */
    
    size_t freeblocks1 = 0;
    size_t freeblocks2 = 0;
    
    checkfreelist(&freeblocks2);
    checkallblock(&freeblocks1);
    
    if (freeblocks1 != freeblocks2) {
        printf( "Free blocks counts does not match\n" );
    }
    
    /* check epilogue */
    
    printblock(epilogue);
    if (GET_SIZE(epilogue) != 0) {
        printf( "Error: %p epilogue size error",epilogue);
    }
    if (!GET_ALLOC(epilogue)) {
        printf( "Error: %p epilogue alloc error",epilogue);
    }
    

}


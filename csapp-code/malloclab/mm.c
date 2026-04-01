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
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "memlib.h"
#include "mm.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "The one",
    /* First member's full name */
    "Verzil",
    /* First member's email address */
    "@qq.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

// #define WSIZE           sizeof(void*)
// #define DSIZE           (2 * WSIZE)
#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1 << 12)
#define PACK(size, is_alloc) ((size) | (is_alloc))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = val)
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)
#define NEXT_BP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)))
#define PREV_BP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

// 空闲块相关指针操作
#define FREE_PREV(bp) ((char **)(bp))
#define FREE_NEXT(bp) ((char **)((char *)(bp) + WSIZE))

#define SET_PREV_FREE_BP(bp, prev) (*FREE_PREV(bp) = prev)
#define GET_PREV_FREE_BP(bp) (*FREE_PREV(bp))
#define SET_NEXT_FREE_BP(bp, next) (*FREE_NEXT(bp) = next)
#define GET_NEXT_FREE_BP(bp) (*FREE_NEXT(bp))

// 链表类头操作，定义多个类（类从0开始）
#define CLASS_NUM 11
#define GET_CLASSN_DUMMY_HEAD(n) ((char *)free_block_array_head_listp + (n) * DSIZE)
#define GET_NEXT_DUMMY_HEAD(dummy_head) ((char *)dummy_head + DSIZE)

// ======================== MIT 6.5840 风格调试打印 ========================
#define DEBUG 0

// 颜色定义
#define RED "\033[31m"
#define GREEN "\033[32m"
#define YELLOW "\033[33m"
#define BLUE "\033[34m"
#define PURPLE "\033[35m"
#define RESET "\033[0m"

// 核心通用调试打印（和 Raft Lab dprint 完全一样）
#if DEBUG
#define DPrint(format, ...)                                                                        \
    printf("[DEBUG] " format " | %s():%d\n", ##__VA_ARGS__, __func__, __LINE__)
#else
#define DPrint(format, ...)
#endif

// Malloc 专用彩色打印
#define DMalloc(format, ...) DPrint(BLUE "[MALLOC] " format RESET, ##__VA_ARGS__)
#define DFree(format, ...) DPrint(YELLOW "[FREE]   " format RESET, ##__VA_ARGS__)
#define DCoalesce(format, ...) DPrint(PURPLE "[COALESCE]" format RESET, ##__VA_ARGS__)
#define DExtend(format, ...) DPrint(GREEN "[EXTEND] " format RESET, ##__VA_ARGS__)
#define DWarn(format, ...) DPrint(RED "[WARN]   " format RESET, ##__VA_ARGS__)
// =========================================================================

static char *heap_listp;
static char **free_block_array_head_listp;

static void *coalesce(void *bp);
static void *alloc_strategy(size_t asize);
static void *find(size_t asize);
static void place(void *bp, size_t asize);
static void *extend_heap(size_t words);
static void mm_check();
static void mm_check_unconsistency(void *bp);
static void pop_free_bp(void *bp);
static void insert_free_bp(void *bp);
static void place_block_seg(void *bp, size_t asize);
static void *get_match_class_dummy_head(size_t asize); // 和CLASS_NUM同步更新

/**
 * check连续性
 */
static int malloc_times = 0;
static int free_times = 0;

static void mm_check_unconsistency(void *bp) {
    if (!GET_ALLOC(HDRP(bp)) && (!GET_ALLOC(HDRP(PREV_BP(bp))) || !GET_ALLOC(HDRP(NEXT_BP(bp))))) {
        printf("%s mm_check unconsistency malloc:%d free:%d %s\n", RED, malloc_times, free_times,
               RESET);
        exit(0);
    }
}

/**
 * 主要负责check每个块的完整性和check空块是否存在连续
 */
static void mm_check() {
    printf("%s mm_check start malloc:%d free:%d %s\n", GREEN, malloc_times, free_times, RESET);
    int sequences = 0;
    char *bp = heap_listp;
    char *next_bp;
    size_t heap_size_cnt = DSIZE;

    // 在循环开始前检查
    if (GET_SIZE(HDRP(heap_listp)) != (CLASS_NUM + 1) * DSIZE || !GET_ALLOC(HDRP(heap_listp)))
        DWarn("mm_check 序言块被破坏 malloc:%d free:%d\n", malloc_times, free_times);

    while (GET_SIZE(HDRP(bp)) > 0) {
        if (bp < mem_heap_lo() || bp > mem_heap_hi())
            DWarn("mm_check bp越界 malloc:%d free:%d\n", malloc_times, free_times);

        if (GET_SIZE(HDRP(bp)) % ALIGNMENT != 0)
            DWarn("mm_check 块未对齐 malloc:%d free:%d \n", malloc_times, free_times);

        next_bp = NEXT_BP(bp);

        if (!GET_ALLOC(HDRP(bp)) && !GET_ALLOC(HDRP(next_bp))) {
            DWarn("mm_check 空闲块前后连续 sequences:%d malloc:%d free:%d\n", sequences,
                  malloc_times, free_times);
        }

        if (GET_ALLOC(HDRP(bp)) != GET_ALLOC(FTRP(bp)) ||
            GET_SIZE(HDRP(bp)) != GET_SIZE(FTRP(bp))) {
            DWarn("mm_check 块头尾大小不一致 sequences:%d malloc:%d free:%d\n", sequences,
                  malloc_times, free_times);
        }

        // ============= printf 块信息 ============
        if (bp == heap_listp) {
            int idx;
            char *class_head = bp;
            for (idx = 0; idx < CLASS_NUM; idx++) {
                printf("class:%d addr=%-12p | prev=%-12p next=%-12p | \n", idx, class_head,
                       GET_PREV_FREE_BP(class_head), GET_NEXT_FREE_BP(class_head));
                class_head += DSIZE;
            }
        } else if (GET_SIZE(HDRP(bp)) != 0) {
            printf("bp=%-12p state=%d | asize=%-4lu |", bp, GET_ALLOC(HDRP(bp)),
                   GET_SIZE(HDRP(bp)));
            if (!GET_ALLOC(HDRP(bp)))
                printf("prev=%-12p | next=%-12p", GET_PREV_FREE_BP(bp), GET_NEXT_FREE_BP(bp));
            printf("\n");
        }
        // =======================================

        sequences++;
        heap_size_cnt += GET_SIZE(HDRP(bp));
        bp = next_bp;
    }

    if (heap_size_cnt != mem_heapsize())
        DWarn("mm_check 堆大小不一致 malloc:%d free:%d \n", malloc_times, free_times);
}

/*
 * mm_init - initialize the malloc package.
 */
int mm_init(void) {
    // 对堆初始化
    mem_init();
    if ((heap_listp = mem_sbrk(4 * WSIZE + CLASS_NUM * DSIZE)) == (void *)-1)
        return -1;

    // 初始化序言块、填充块
    PUT(heap_listp, 0);
    PUT(heap_listp + WSIZE, PACK((CLASS_NUM + 1) * DSIZE, 1));
    int idx;
    for (idx = 0; idx < CLASS_NUM; idx++) {
        PUT(heap_listp + ((2 + 2 * idx) * WSIZE), 0);
        PUT(heap_listp + ((3 + 2 * idx) * WSIZE), 0);
    }
    PUT(heap_listp + ((2 * (CLASS_NUM + 1)) * WSIZE), PACK((CLASS_NUM + 1) * DSIZE, 1));
    PUT(heap_listp + ((2 * (CLASS_NUM + 1) + 1) * WSIZE), PACK(0, 1));
    heap_listp += 2 * WSIZE;
    free_block_array_head_listp = (char **)heap_listp;
    if (DEBUG) {
        printf("init \n");
        char *p = mem_heap_lo();
        printf("=== 初始化堆全字段打印 ===\n");
        printf("addr:%p | val:0x%08x | 对齐填充块\n", p, GET(p));
        p += WSIZE;
        printf("addr:%p | val:0x%08x | 序言块头部\n", p, GET(p));
        p += WSIZE;
        for (idx = 0; idx < CLASS_NUM; idx++) {
            printf("addr:%p | val:%p | class_num:%d  prev\n", p, (void *)*(char **)p, idx);
            p += WSIZE;
            printf("addr:%p | val:%p | class_num:%d  next\n", p, (void *)*(char **)p, idx);
            p += WSIZE;
        }
        printf("addr:%p | val:0x%08x | 序言块尾部\n", p, GET(p));
        p += WSIZE;
        printf("addr:%p | val:0x%08x | 结尾块头部\n", p, GET(p));
        printf("===========================\n");
        // exit(0);
    }

    return 0;
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size) {
    DMalloc("start malloc:%d free:%d ", malloc_times, free_times);

    malloc_times++;

    if (size <= 0)
        return NULL;

    // 假如是64位的系统里，这个asize需要重新定义，因为最小合法块是32字节的
    size_t asize = ALIGN(size + DSIZE);
    char *bp;
    if ((bp = find(asize)) != NULL) {
        // printf("malloc find place \n");
        place(bp, asize);
        if (DEBUG)
            mm_check();
        return bp;
    }

    // 找不到空缺开始分配策略
    // printf("try alloc \n");
    if ((bp = alloc_strategy(asize)) == NULL)
        return NULL;
    place(bp, asize);

    if (DEBUG)
        mm_check();
    // exit(0)
    return bp;
}

/**
 * 空闲块不足的时候，启动分配策略
 * 可以直接扩展堆，也可以整理堆(malloc lab里不允许这么操作)
 */
static void *alloc_strategy(size_t asize) {
    char *bp;
    size_t extend_size = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extend_size / WSIZE)) == NULL) {
        return NULL;
    }

    return bp;
}

static void *extend_heap(size_t words) {
    DExtend(" ");
    size_t size = words % 2 ? (words + 1) * WSIZE : words * WSIZE;
    char *bp;
    if ((bp = mem_sbrk((int)size)) == (void *)-1)
        return NULL;

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(NEXT_BP(bp)), PACK(0, 1));
    SET_NEXT_FREE_BP(bp, NULL);
    SET_PREV_FREE_BP(bp, NULL);

    return coalesce(bp);
}

/**
 * 查找空闲块
 * 如果没有return NULL
 */
static void *find(size_t asize) {
    char *dummy_head = get_match_class_dummy_head(asize);
    while (dummy_head != GET_CLASSN_DUMMY_HEAD(CLASS_NUM)) {
        char *cur_bp = GET_NEXT_FREE_BP(dummy_head);
        while (cur_bp != NULL) {
            if (GET_SIZE(HDRP(cur_bp)) >= asize)
                return cur_bp;
            cur_bp = GET_NEXT_FREE_BP(cur_bp);
        }
        dummy_head = GET_NEXT_DUMMY_HEAD(dummy_head);
    }

    return NULL;
}

/**
 * 将大小为asize的块放置于当前bp处（语义上必须要空闲块，且bp的size >= asize）
 */
static void place(void *bp, size_t asize) {
    pop_free_bp(bp);
    place_block_seg(bp, asize);
}

/**
 * place中拆分前后块的逻辑
 * bp为当前的块，而asize为当前需要设置块的大小
 */
static void place_block_seg(void *bp, size_t asize) {

    size_t cur_size = GET_SIZE(HDRP(bp));
    // 要求2个DSIZE是因为只放头尾的块是没有意义的，而且用不了导致无意义碎片
    // 2*DSIZE是最小合法有效块
    if (cur_size - asize >= 2 * DSIZE) {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));

        char *next = NEXT_BP(bp);
        PUT(HDRP(next), PACK(cur_size - asize, 0));
        PUT(FTRP(next), PACK(cur_size - asize, 0));

        insert_free_bp(next);
    } else {
        PUT(HDRP(bp), PACK(cur_size, 1));
        PUT(FTRP(bp), PACK(cur_size, 1));
    }
}

/**
 * 将空闲块从链表中弹出（必须可重复调用）
 */
static void pop_free_bp(void *bp) {
    char *prev_free_bp = GET_PREV_FREE_BP(bp);
    char *next_free_bp = GET_NEXT_FREE_BP(bp);

    SET_NEXT_FREE_BP(prev_free_bp, next_free_bp);
    if (next_free_bp != NULL)
        SET_PREV_FREE_BP(next_free_bp, prev_free_bp);
}

/**
 * 根据传进来的asize决定应该放到哪个类
 * jupyter分析脚本用的是size分析的，划分的时候依据也是size大小，所以需要计算size
 * （该函数为定制化函数，和CLASS_NUM强绑定）
 */
static void *get_match_class_dummy_head(size_t asize) {
    int idx = 0;
    if (asize <= 16)
        idx = 0;
    else if (asize <= 32)
        idx = 1;
    else if (asize <= 64)
        idx = 2;
    else if (asize <= 128)
        idx = 3;
    else if (asize <= 256)
        idx = 4;
    else if (asize <= 512)
        idx = 5;
    else if (asize <= 1024)
        idx = 6;
    else if (asize <= 2048)
        idx = 7;
    else if (asize <= 4096)
        idx = 8;
    else if (asize <= 8192)
        idx = 9;
    else
        idx = 10;

    return GET_CLASSN_DUMMY_HEAD(idx);
}

/**
 * 将空闲块插入链表（插入策略——头插法）
 */
static void insert_free_bp(void *bp) {
    char *dummy_head = get_match_class_dummy_head(GET_SIZE(HDRP(bp)));
    char *head = GET_NEXT_FREE_BP(dummy_head);

    SET_PREV_FREE_BP(bp, dummy_head);
    SET_NEXT_FREE_BP(bp, head);
    SET_NEXT_FREE_BP(dummy_head, bp);
    if (head != NULL)
        SET_PREV_FREE_BP(head, bp);
}

/**
 * 空闲块合并逻辑
 * return empty_head
 */
static void *coalesce(void *bp) {
    DCoalesce(" ");
    // 合并逻辑不需要采用递归，因为只有四种可能的情况
    size_t is_prev_alloc = GET_ALLOC(HDRP(PREV_BP(bp)));
    size_t is_next_alloc = GET_ALLOC(HDRP(NEXT_BP(bp)));
    size_t cur_size = GET_SIZE(HDRP(bp));

    if (!is_prev_alloc && !is_next_alloc) {
        cur_size += GET_SIZE(HDRP(PREV_BP(bp))) + GET_SIZE(HDRP(NEXT_BP(bp)));

        char *prev_bp = PREV_BP(bp);
        char *next_bp = NEXT_BP(bp);
        pop_free_bp(prev_bp);
        pop_free_bp(next_bp);

        // 合并
        PUT(HDRP(prev_bp), PACK(cur_size, 0));
        PUT(FTRP(prev_bp), PACK(cur_size, 0));

        insert_free_bp(prev_bp);

        bp = prev_bp;
    } else if (!is_prev_alloc && is_next_alloc) {
        char *prev_bp = PREV_BP(bp);
        pop_free_bp(prev_bp);
        cur_size += GET_SIZE(HDRP(prev_bp));

        // 合并
        PUT(HDRP(prev_bp), PACK(cur_size, 0));
        PUT(FTRP(prev_bp), PACK(cur_size, 0));

        insert_free_bp(prev_bp);

        bp = prev_bp;
    } else if (is_prev_alloc && !is_next_alloc) {
        char *next_bp = NEXT_BP(bp);
        cur_size += GET_SIZE(HDRP(next_bp));
        pop_free_bp(next_bp);

        // 合并
        PUT(HDRP(bp), PACK(cur_size, 0));
        PUT(FTRP(bp), PACK(cur_size, 0));

        insert_free_bp(bp);
    } else {
        insert_free_bp(bp);
    }

    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr) {
    DFree("start malloc:%d free:%d ", malloc_times, free_times);
    free_times++;

    // 释放块
    size_t size = GET_SIZE(HDRP(ptr));
    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));

    // 合并空闲块
    ptr = coalesce(ptr);

    if (DEBUG)
        mm_check();
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 * 核心优化方案是减少memmove和memcopy的调用
 */
void *mm_realloc(void *ptr, size_t size) {
    // printf("realloc \n");
    void *oldptr = ptr;
    void *newptr = ptr;
    size_t copySize = MIN(GET_SIZE(HDRP(oldptr)), size);
    size_t cur_size = GET_SIZE(HDRP(oldptr));
    size_t asize = ALIGN((size + DSIZE));
    char *next_bp = NEXT_BP(ptr);

    if (cur_size >= asize) {
        place_block_seg(newptr, asize);
    } else if (!GET_ALLOC(HDRP(next_bp)) && cur_size + GET_SIZE(HDRP(next_bp)) >= asize) {
        // 将下一个块合到当前块上
        pop_free_bp(next_bp);
        cur_size += GET_SIZE(HDRP(next_bp));
        PUT(HDRP(newptr), PACK(cur_size, 0));
        PUT(HDRP(newptr), PACK(cur_size, 0));
        place_block_seg(newptr, asize);
    } else if (GET_SIZE(HDRP(next_bp)) == 0 ||
               (!GET_ALLOC(HDRP(next_bp)) && GET_SIZE(HDRP(NEXT_BP(next_bp))) == 0)) {
        // 直接扩堆
        if ((next_bp = alloc_strategy(asize)) == NULL)
            return NULL;

        pop_free_bp(next_bp);
        cur_size += GET_SIZE(HDRP(next_bp));
        PUT(HDRP(newptr), PACK(cur_size, 0));
        PUT(FTRP(newptr), PACK(cur_size, 0));
        place_block_seg(newptr, asize);
    } else {
        // 重新malloc和free
        newptr = mm_malloc(size);
        if (newptr == NULL)
            return NULL;
        memmove(newptr, oldptr, copySize);
        mm_free(oldptr);
    }

    if (DEBUG)
        mm_check();

    return newptr;
}

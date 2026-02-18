#include "cachelab.h"
#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define MAX_LINE 256
#define ADDRESS_LEN 64

long timestamp = 0;

// 单个缓存行：包含有效位、标记位、LRU时间戳（核心字段）
typedef struct cache_line {
    // 对齐内存
    uint64_t tag;      // 标记位：地址拆分后的tag部分（64位避免溢出）
    int valid;         // 有效位：0=无效，1=有效
    int lru_timestamp; // LRU时间戳：值越大=最近越常使用，实现LRU替换
} CacheLine;

// 单个缓存组：包含指向E个缓存行的指针（无需存B/E，缓存级统一管理）
typedef struct cache_group {
    CacheLine *lines; // 指向该组的所有行（替换你的柔性数组成员，更易malloc）
} CacheGroup;

// 整个缓存：管理全局配置（b/s/E）+ 所有组的指针
typedef struct cache {
    // cache大小参数
    int b;  // 块偏移位数（B=2^b是块大小，无需存B，计算即可）
    int s;  // 组索引位数（S=2^s是组数）
    int E;  // 相联度（每组E行，全局存一份即可，无需每组都存）
    long S; // 2^s，选择long类型，避免不必要的隐式类型转化导致的错误

    // 统计指标
    long hits;
    long misses;
    long evictions;

    CacheGroup *groups; // 指向所有缓存组（替换你的柔性数组成员）
} Cache;

void parse_params(int argc, char *argv[], int *s, int *E, int *b, int *verbose, char **file);

Cache *init_cache(int s, int E, int b);

void simulator(char *file_path, Cache *cache);

int parse_opt_line(char *line, char *opt, uint64_t *addr, int *size);

void memory_access(Cache *cache, uint64_t addr);

void parse_addr(Cache *cache, uint64_t addr, uint64_t *tag, uint64_t *s_index);

void cache_free(Cache *cache);

int main(int argc, char *argv[]) {
    int s = -1;
    int E = -1;
    int b = -1;
    int verbose = 0;
    char *file = NULL;
    parse_params(argc, argv, &s, &E, &b, &verbose, &file);
    Cache *cache = init_cache(s, E, b);
    simulator(file, cache);
    printSummary(cache->hits, cache->misses, cache->evictions);
    cache_free(cache);
    return 0;
}

// 解析参数，并且确保没有参数错误问题
void parse_params(int argc, char *argv[], int *s, int *E, int *b, int *v, char **file) {
    int opt;
    while ((opt = getopt(argc, argv, "")) != -1) {
        switch (opt) {
        case 's':
            *s = atoi(optarg);
            break;
        case 'E':
            *E = atoi(optarg);
            break;
        case 'b':
            *b = atoi(optarg);
            break;
        case 'v':
            *v = 1;
            break;
        case 't':
            *file = optarg;
            break;
        case '?':
            fprintf(stderr, "Invalid option: -%c\n", optopt);
            exit(1);
        default:
            break;
        }
    }

    // 检查必要参数是否全部提供（s/E/b/t不能为初始值-1/NULL）
    if (*s == -1 || *E == -1 || *b == -1 || *file == NULL) {
        fprintf(stderr, "Error: Missing required arguments!\n");
        fprintf(stderr, "Usage: %s -s <s> -E <E> -b <b> -t <tracefile> [-v]\n", argv[0]);
        exit(1);
    }

    // 4. 检查参数合法性（数值不能为负，避免非法缓存配置）
    if (*s < 0 || *E <= 0 || *b < 0) {
        fprintf(stderr, "Error: Invalid parameter values!\n");
        fprintf(stderr, "s/b must be non-negative, E must be positive\n");
        exit(1);
    }

    // 5. 解析完成，打印参数（调试用，-v模式下可输出）
    if (*v) {
        printf("Cache config: s=%d, E=%d, b=%d\n", s, E, b);
        printf("Trace file: %s\n", *file);
    }
}

// 初始化缓存
Cache *init_cache(int s, int E, int b) {
    long S = 1 << s;
    if (S <= 0) {
        fprintf(stderr, "S is negative");
        exit(1);
    }

    Cache *cache = (Cache *)calloc(1, sizeof(Cache));
    cache->s = s;
    cache->E = E;
    cache->b = b;
    cache->S = S;

    cache->hits = 0;
    cache->misses = 0;
    cache->evictions = 0;

    cache->groups = (CacheGroup *)calloc(S, sizeof(CacheGroup));
    int i = 0;
    for (i = 0; i < S; i++) {
        cache->groups[i].lines = (CacheLine *)calloc(E, sizeof(CacheLine));
    }

    return cache;
}

// 读取文件操作，模拟操作运行，并且统计结果
void simulator(char *file_path, Cache *cache) {
    FILE *file = fopen(file_path, "r");
    if (file == NULL) {
        perror("error: file open");
        exit(1);
    }

    char line[MAX_LINE];
    char opt;
    uint64_t addr;
    int size;

    while (fgets(line, MAX_LINE, file) != NULL) {
        if (parse_opt_line(line, &opt, &addr, &size) == 0) {
            switch (opt) {
            case 'I':
                /* code */
                break;
            case 'L':
            case 'S':
                memory_access(cache, addr);
                break;
            case 'M':
                // modify本质是两次访存操作：第一次读，第二次写
                // 第二次必定是命中的，所以无需再次调用
                memory_access(cache, addr);
                cache->hits++;
                break;
            default:
                fprintf(stderr, "error: simulator illegal opt:%c", opt);
                break;
            }
        }
    }
}

//
int parse_opt_line(char *line, char *opt, uint64_t *addr, int *size) {
    int params = sscanf(line, "%c %llu,%d", opt, addr, size);

    if (params != 3) {
        fprintf(stderr, "error: parse_opt_line line:%s params=%d", line, params);
        exit(1);
    }

    return 0;
}

// 访存操作
void memory_access(Cache *cache, uint64_t addr) {
    uint64_t tag;
    uint64_t s_index;
    parse_addr(cache, addr, &tag, &s_index);

    CacheLine *lines = cache->groups[s_index].lines;

    int i = 0;
    CacheLine *line = lines;
    CacheLine *target_line = NULL;
    CacheLine *least_access_line = NULL;
    CacheLine *empty_line = NULL;
    for (i = 0, line = lines; i < cache->E; i++, line++) {
        if (line->valid) {
            if (line->tag == tag)
                target_line = line;

            if (least_access_line == NULL || line->lru_timestamp < least_access_line->lru_timestamp)
                least_access_line = line;
        } else {
            empty_line = line;
        }
    }

    if (target_line != NULL) {
        // 缓存命中
        target_line->lru_timestamp = ++timestamp;

        cache->hits++;
    } else {
        // 缓存不命中，开始考虑是否存在空页能够进行替换
        if (empty_line != NULL) {
            empty_line->valid = 1;
            empty_line->tag = tag;
            empty_line->lru_timestamp = ++timestamp;

            cache->misses++;
        } else {
            // 开始进行替换操作，将最不常访问的替换了
            if (least_access_line == NULL) {
                fprintf(stderr, "unknow error: memory_access least_access_line==NULL");
                exit(1);
            }

            least_access_line->tag = tag;
            least_access_line->lru_timestamp = ++timestamp;

            cache->misses++;
            cache->evictions++;
        }
    }
}

// 访存地址解析
void parse_addr(Cache *cache, uint64_t addr, uint64_t *tag, uint64_t *s_index) {
    if (cache == NULL || tag == NULL || s_index == NULL) {
        fprintf(stderr, "error: parse_addr illegal params");
        exit(1);
    }

    int s = cache->s;
    int b = cache->b;
    uint64_t tag_mask = ((1ULL << (ADDRESS_LEN - s - b)) - 1) << (s + b);
    uint64_t s_index_mask = ((1ULL << s) - 1) << s;
    *tag = addr & tag_mask;
    *s_index = addr & s_index_mask;
}

// 将堆上分配的内存释放
void cache_free(Cache *cache) {
}

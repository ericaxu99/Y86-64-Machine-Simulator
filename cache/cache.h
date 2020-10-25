#include <stdio.h> 
#include <stdbool.h>
#ifndef CACHE_H
#define CACHE_H

typedef unsigned long long int mem_addr_t;
typedef long long word_t;
typedef unsigned char byte_t;

void initCache(int s_in, int b_in, int E_in);
void freeCache();
void accessData(mem_addr_t addr);

bool handle_miss(word_t pos, void *block, word_t *evicted_pos, void *evicted_block);
bool check_hit(word_t pos);

#endif /* CACHELAB_H */
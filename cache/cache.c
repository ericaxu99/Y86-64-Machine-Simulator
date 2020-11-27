/* 
 * cache.c - A cache simulator that can replay traces from Valgrind
 *     and output statistics such as number of hits, misses, and
 *     evictions.  The replacement policy is LRU.
 *
 * Implementation and assumptions:
 *  1. Each load/store can cause at most one cache miss. (I examined the trace,
 *  the largest request I saw was for 8 bytes).
 *  2. Instruction loads (I) are ignored, since we are interested in evaluating
 *  trans.c in terms of its data cache performance.
 *  3. data modify (M) is treated as a load followed by a store to the same
 *  address. Hence, an M operation can result in two cache hits, or a miss and a
 *  hit plus an possible eviction.
 *
 * The function printSummary() is given to print output.
 * Please use this function to print the number of hits, misses and evictions.
 * This is crucial for the driver to evaluate your work. 
 */
#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <math.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
#include "cache.h"

//#define DEBUG_ON
#define ADDRESS_LENGTH 64

/* Globals set by command line args */
int verbosity_cache = 0; /* print trace if set */
int s = 0;               /* set index bits */
int b = 0;               /* block offset bits */
int E = 0;               /* associativity */

/* Derived from command line args */
int S; /* number of sets */
int B; /* block size (bytes) */
unsigned long long counter;

/* Counters used to record cache statistics in printSummary().
   test-cache uses these numbers to verify correctness of the cache. */

//Increment when a miss occurs
int miss_count = 0;

//Increment when a hit occurs
int hit_count = 0;

//Increment when an eviction occurs
int eviction_count = 0;

/* 
 * A possible hierarchy for the cache. The helper functions defined below
 * are based on this cache structure.
 * lru is a counter used to implement LRU replacement policy.
 */
typedef struct cache_line
{
    char valid;
    mem_addr_t tag;
    unsigned long long int lru;
    byte_t *data;
} cache_line_t;

typedef struct cache_set
{
    cache_line_t *lines;
} cache_set_t;

typedef struct cache
{
    cache_set_t *sets;
} cache_t;

cache_t cache;

/* TODO: add more globals, structs, macros if necessary */

/* 
 * Initialize the cache according to specified arguments
 * Called by cache-runner so do not modify the function signature
 * 
 * The code provided here shows you how to initialize a cache structure
 * defined above. It's not complete and feel free to modify/add code.
 */
void initCache(int s_in, int b_in, int E_in)
{
    /* see cache-runner for the meaning of each argument */
    s = s_in;
    b = b_in;
    E = E_in;
    S = (unsigned int)pow(2, s);
    B = (unsigned int)pow(2, b);

    int i, j;
    cache.sets = (cache_set_t *)calloc(S, sizeof(cache_set_t));
    for (i = 0; i < S; i++)
    {
        cache.sets[i].lines = (cache_line_t *)calloc(E, sizeof(cache_line_t));
        for (j = 0; j < E; j++)
        {
            cache.sets[i].lines[j].valid = 0;
            cache.sets[i].lines[j].tag = 0;
            cache.sets[i].lines[j].lru = 0;
            cache.sets[i].lines[j].data = calloc(B, sizeof(byte_t));
        }
    }
    /* TODO: add more code for initialization */
    counter = 0;
}

/* 
 * Free allocated memory. Feel free to modify it
 */
void freeCache()
{
    int i;
    for (i = 0; i < S; i++)
    {
        free(cache.sets[i].lines);
    }
    free(cache.sets);
}

unsigned long long get_set(word_t addr)
{
    unsigned long long address = (unsigned long long)addr;
    return (address >> b) << (64 - s) >> (64 - s);
}

unsigned long long get_tag(word_t addr)
{
    unsigned long long address = (unsigned long long)addr;
    return (address >> b) >> s;
}

/* TODO:
 * Get the line for address contained in the cache
 * On hit, return the cache line holding the address
 * On miss, returns NULL
 */
cache_line_t *get_line(word_t addr)
{
    unsigned long long set = get_set(addr); //get set bits
    unsigned long long tag = get_tag(addr); //get tag bits
    for (int i = 0; i < E; i++)
    {
        if (cache.sets[set].lines[i].valid == 1 && cache.sets[set].lines[i].tag == tag)
        {
            return &cache.sets[set].lines[i];
        }
    }
    return NULL;
}
/* TODO:
 * Select the line to fill with the new cache line
 * Return the cache line selected to filled in by addr
 */
cache_line_t *select_line(word_t addr)
{
    unsigned long long set = get_set(addr);
    //unsigned long long tag = get_tag(addr);
    //find empty line
    for (int j = 0; j < E; j++)
    {
        if (cache.sets[set].lines[j].valid == 0)
        {
            //cache.sets[set].lines[j].valid = 1;
            //cache.sets[set].lines[j].tag = tag;
            return &cache.sets[set].lines[j];
        }
        // else if (cache.sets[set].lines[j].valid == 1 && cache.sets[set].lines[j].tag == tag)
        // {
        //     return &cache.sets[set].lines[j];
        // }
    }
    //cant find empty space, get LRU
    cache_line_t *min = &cache.sets[set].lines[0];
    for (int i = 0; i < E; i++)
    {
        if (cache.sets[set].lines[i].lru < min->lru)
        {
            min = &cache.sets[set].lines[i];
        }
    }
    return min;
}

/*  TODO:
 * Check if the address is hit in the cache, updating hit and miss data. 
 * Return True if pos hits in the cache.
 */
bool check_hit(word_t pos)
{
    if (get_line(pos) != NULL) //hit valid=1 && tag=tag
    {
        hit_count++;
        counter++;
        get_line(pos)->lru = counter;
        return true;
    }
    else //valid = 0
    {
        miss_count++;
        return false;
    }
}

/*  TODO:
 * Handles Misses, evicting from the cache if necessary. If evicted_pos and evicted_block
 * are not NULL, copy the evicted data and address out.
 * If block is not NULL, copy the data from block into the cache line. 
 * Return True if a line was evicted.
 */
bool handle_miss(word_t pos, void *block, word_t *evicted_pos, void *evicted_block)
{
    cache_line_t *targetline = select_line(pos);
    counter++;
    targetline->lru = counter;
    if (targetline->valid == 1 && (targetline->tag = get_tag(pos)))
    {
        eviction_count++;
    }
    targetline->tag = get_tag(pos);
    targetline->valid = 1;
    if (block != NULL)
    {
        memcpy(targetline->data, block, sizeof(byte_t));
    }
    return false;
}

/* 
 * Access data at memory address addr
 * If it is already in cache, increast hit_count
 * If it is not in cache, bring it in cache, increase miss count
 * Also increase eviction_count if a line is evicted
 * 
 * Called by cache-runner; no need to modify it if you implement
 * check_hit() and handle_miss()
 */
void accessData(mem_addr_t addr)
{
    if (!check_hit(addr))
        handle_miss(addr, NULL, NULL, NULL);
}
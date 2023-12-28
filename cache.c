#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "cache.h"

static cache_entry_t *cache = NULL;
static int cache_size = 0;
static int clock = 0;
static int num_queries = 0;
static int num_hits = 0;

int cache_create(int num_entries) {
  //if the cache is already enabled or has less than 2 entries or more than 4096 entries return failure
  if(cache_enabled() == true || num_entries < 2 || num_entries > 4096)
    return -1;
  //Otherwise dynamically allocate space for num entries cache
  cache = calloc(num_entries, sizeof(cache_entry_t));
  cache_size = num_entries;
  //Return success
  return 1;
}

int cache_destroy(void) {
  //If the cache is already disabled return a failure
  if(cache_enabled() == false)
    return -1;
  //Otherwise free the cache and set it to null
  free(cache);
  cache = NULL;
  //Return success
  return 1;
}

int cache_lookup(int disk_num, int block_num, uint8_t *buf) {
  //If the cache is disabled or the buf is set to null return a failure
  if(cache_enabled() == false || buf == NULL)
    return -1;

  for(int entry = 0; entry < cache_size; ++entry) {
    //If the block is found
    if(cache[entry].valid == true && cache[entry].disk_num == disk_num && cache[entry].block_num == block_num) {
      //Copy the block into buf
      memcpy(buf, cache[entry].block, JBOD_BLOCK_SIZE);
      //Record access time
      cache[entry].access_time = clock;
      //Increment clock by one
      clock += 1;
      //Increment the number of hits by one
      num_hits += 1;
      //Increment the number of queries by one
      num_queries += 1;
      //Return a success
      return 1;
    }
  }
  //If the block was not found
  //Increment clock by one
  clock += 1;
  //Increment the number of queries by one
  num_queries += 1;
  //Return a failure
  return -1;
}

void cache_update(int disk_num, int block_num, const uint8_t *buf) {
  for(int entry = 0; entry < cache_size; ++entry) {
    //If the block is found
    if(cache[entry].valid == true && cache[entry].disk_num == disk_num && cache[entry].block_num == block_num) {
      //Update the block data from the new data in buf
      memcpy(cache[entry].block, buf, JBOD_BLOCK_SIZE);
      //Record access time
      cache[entry].access_time = clock;
      //Increment clock by one
      clock += 1;
      break;
    }
  }
}

int cache_insert(int disk_num, int block_num, const uint8_t *buf) {
  uint8_t myBuf[256];
  //If the cache is disabled or the buf is set to null or attempting to insert an existing entry return a failure
  if(cache_enabled() == false || buf == NULL || cache_lookup(disk_num, block_num, myBuf) == 1)
    return -1;
  //If attmepting to insert an ivalid disk or block number return a failure
  else if(disk_num > JBOD_NUM_DISKS || disk_num < 0 || block_num > JBOD_NUM_BLOCKS_PER_DISK || block_num < 0)
    return -1;

  //Loop for if there is space available in the cache
  for(int entry = 0; entry < cache_size; ++entry) {
    if(cache[entry].valid == false) {
      //goto block in cache and insert entry
      cache[entry].disk_num = disk_num;
      cache[entry].block_num = block_num;
      memcpy(cache[entry].block, buf, JBOD_BLOCK_SIZE);
      //Update clock and access time after new entry
      cache[entry].access_time = clock;
      clock += 1;
      //Change entry to be valid 
      cache[entry].valid = true;
      //Return a success
      return 1;
    }
  }

  //Set lru to be the first access time
  int lru = cache[0].access_time;
  //Loop to find smallest access time
  for(int entry = 0; entry < cache_size; ++entry) 
    //If the access time is smaller than the previous access time
    if(lru > cache[entry].access_time)
      //Replace lru with smallest access time
      lru = cache[entry].access_time;

  //Loop for if there is no space available in the cache using lru policy
  for(int entry = 0; entry < cache_size; ++entry) {
    //If the access time is equal to the smallest access time
    if(lru == cache[entry].access_time) {
      //Goto block in cache and replace entry
      cache[entry].disk_num = disk_num;
      cache[entry].block_num = block_num;
      memcpy(cache[entry].block, buf, JBOD_BLOCK_SIZE);
      //Update clock and access time after new entry
      cache[entry].access_time = clock;
      clock += 1;
      //Return a success
      return 1;
    }
  }
  //Return a failure if a success is never returned
  return -1;
}

bool cache_enabled(void) {
  //If the cache is empty then return that the cache is disabled
  if(cache == NULL)
    return false;
  //Otherwise return that the cache is enabled
  return true;
}

void cache_print_hit_rate(void) {
  //Print hit rate in cache
  fprintf(stderr, "Hit rate: %5.1f%%\n", 100 * (float) num_hits / num_queries);
}


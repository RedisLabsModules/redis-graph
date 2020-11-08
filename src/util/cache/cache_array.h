/*
 * Copyright 2018-2020 Redis Labs Ltd. and Contributors
 *
 * This file is available under the Redis Labs Source Available License Agreement
 */

#pragma once

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>

typedef void (*CacheEntryFreeFunc)(void *);
typedef void *(*CacheEntryCopyFunc)(void *);

/**
 * @brief  A struct for a an entry in cache array with a key and value.
 */
typedef struct CacheEntry_t {
	char *key;      // Entry key.
	ulong LRU;      // Indicates the time when the entry was last recently used.
	void *value;    // Entry stored value.
} CacheEntry;


// Returns a pointer to the entry in the cache array with the minimum LRU.
CacheEntry *CacheArray_FindMinLRU(CacheEntry *cache_arr, uint cap);

// Assign new values to the fields of a cache entry.
CacheEntry *CacheArray_PopulateEntry(ulong counter, CacheEntry *entry, char *key,
  			void *value);

// Free the fields of a cache entry to prepare it for reuse.
void CacheArray_CleanEntry(CacheEntry *entry, CacheEntryFreeFunc free_entry);

/*
 * flash_tlv_cache.h
 * @brief
 * Created on: Apr 10, 2022
 * Author: Yanye
 */

#ifndef _FLASH_TLV_CACHE_H_
#define _FLASH_TLV_CACHE_H_

#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "flash_tlv.h"

#define CACHE_AGE_MAX    0xFF
#define TLV_CACHE_MAX    16

typedef struct _cache_item {
    uint8_t valid;
    uint8_t age;
    uint16_t tag;
    uint32_t entity;
}cache_item_t;

typedef struct _cache_obj {
    uint32_t cursor;
    cache_item_t cache[TLV_CACHE_MAX];
}cache_obj_t;

void invalidate_cache(cache_obj_t *obj);

bool get_cache(cache_obj_t *obj, uint16_t tag, tlv_block_t *blk);

void set_cache(cache_obj_t *obj, uint16_t tag, tlv_block_t *blk);

void remove_cache(cache_obj_t *obj, uint16_t tag);

#endif

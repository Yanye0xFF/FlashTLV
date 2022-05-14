/*
 * flash_tlv_cache.c
 * @brief
 * Created on: Apr 10, 2022
 * Author: Yanye
 */
#include "flash_tlv_cache.h"
#include "spi_flash.h"

void invalidate_cache(cache_obj_t *obj) {
    obj->cursor = 0;
    memset(obj->cache, 0x00, sizeof(cache_item_t) * TLV_CACHE_MAX);
}

bool get_cache(cache_obj_t *obj, uint16_t tag, tlv_block_t *blk) {
    for(uint32_t i = 0; i < obj->cursor; i++) {
        if(obj->cache[i].valid && (obj->cache[i].tag == tag)) {
            if(obj->cache[i].age < CACHE_AGE_MAX) {
                obj->cache[i].age++;
            }
            flash_read((obj->cache[i].entity - TLV_MEAT_SIZE), TLV_MEAT_SIZE, (uint8_t *)blk);
            blk->entity = obj->cache[i].entity;
            return true;
        }
    }
    return false;
}

void set_cache(cache_obj_t *obj, uint16_t tag, tlv_block_t *blk) {
    uint32_t index;
    uint32_t min_position = 0;
    uint32_t age_min = obj->cache[0].age;

    for(uint32_t i = 0; i < obj->cursor; i++) {
        if((obj->cache[i].tag == tag) && obj->cache[i].valid) {
            if(obj->cache[i].age < CACHE_AGE_MAX) {
                obj->cache[i].age++;
            }
            obj->cache[i].entity = blk->entity;
            return;
        }
        if(obj->cache[i].age < age_min) {
            age_min = obj->cache[i].age;
            min_position = i;
        }
    }

    if(obj->cursor >= TLV_CACHE_MAX) {
        index = min_position;
    }else {
        index = obj->cursor;
        obj->cursor++;
    }
    obj->cache[index].tag = tag;
    obj->cache[index].age = 1;
    obj->cache[index].valid = 1;
    obj->cache[index].entity = blk->entity;
}

void remove_cache(cache_obj_t *obj, uint16_t tag) {
    for(uint32_t i = 0; i < obj->cursor; i++) {
        if(obj->cache[i].valid && (obj->cache[i].tag == tag)) {
            obj->cache[i].valid = 0;
            break;
        }
    }
}

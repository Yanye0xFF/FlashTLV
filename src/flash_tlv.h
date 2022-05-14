/*
 * flash_tlv.h
 * @brief
 * Created on: Apr 10, 2022
 * Author: Yanye
 */

#ifndef _FLASH_TLV_H_
#define _FLASH_TLV_H_

#include "stdint.h"
#include <stdbool.h>

#define FLASH_TLV_DEBUG        0
#define FLASH_TLV_USE_CACHE    1

#define INVALID_ADDRESS        0xFFFFFFFF

typedef enum {
    TLV_RESULT_OK = 0,
    TLV_RESULT_NOT_FOUND,
    TLV_NO_VALID_SECTOR,
    TLV_META_SPACE_LOW,
    TLV_DATA_SPACE_LOW,
} tlv_err_t;

#define TLV_MEAT_SIZE     8

#define HEADER_EMPTY_TLV          0xFFFF
#define HEADER_VALID_TLV          0xAA55

#define TLV_STATE_NONE            0xFF
#define TLV_STATE_WRITE           0xFE
#define TLV_STATE_VERIFY          0xFC
#define TLV_STATE_DELETE          0xF8

typedef struct _tlv_block {
    // 结构头 固定0x55 0xaa
    uint16_t header;
    // 结构状态
    uint8_t status;
    // X^8+X^2+X^1+1
    // crc8 = calc_crc8(tag + length + entity[...])
    uint8_t crc8;
    uint16_t tag;
    uint16_t length;
    // 数据域的起始地址(此参数不存储到Flash)
    uint32_t entity;
} tlv_block_t;

typedef struct _tlv_sector {
    // 扇区1地址，对齐到'sector_size'
    uint32_t major_sector;
    // 扇区2地址，对齐到'sector_size'
    uint32_t minor_sector;
    // 扇区大小, unit:byte
    uint16_t sector_size;
    // 脏块数量, 包括写入后校验失败块，标记删除块，Meta域异常坏块
    uint16_t dirty_blocks;
    // 当前工作扇区地址，major_sector或minor_sector其中一个
    uint32_t work_sector;
    // 写入重复Tag时，旧Tag的地址(新Tag写入完成后标记旧Tag删除)
    uint32_t mark_address;
} tlv_sector_t;

#define TLV_SECTOR_TAG            0xCAEE
#define TLV_VERSION_MIN           0x0000
#define TLV_VERSION_MAX           0xFFFF
#define TLV_SECTOR_HEADER_SIZE    4

typedef struct _tlv_sector_header {
    uint16_t tag;
    uint16_t version;
} tlv_sector_header_t;

void flash_tlv_init(tlv_sector_t *sector, uint32_t major, uint32_t minor, uint16_t size);

void flash_tlv_format(tlv_sector_t *sector);

bool flash_tlv_append(tlv_sector_t *sector, uint16_t tag, const uint8_t *data, uint16_t length);

bool flash_tlv_query(tlv_sector_t *sector, uint16_t tag, tlv_block_t *block);

uint32_t flash_tlv_read(tlv_block_t *block, uint8_t *buffer, uint16_t offset, uint16_t length);

bool flash_tlv_verify(tlv_block_t *block);

bool flash_tlv_delete(tlv_sector_t *sector, uint16_t tag);

#endif

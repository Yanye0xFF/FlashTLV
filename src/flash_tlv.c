/*
 * flash_tlv.c
 * @brief
 * Created on: Apr 10, 2022
 * Author: Yanye
 */
#include "flash_tlv.h"
#include "spi_flash.h"
#include "utils.h"
#include "string.h"
#include "stdio.h"
#include "stdbool.h"
#if FLASH_TLV_USE_CACHE
#include "flash_tlv_cache.h"
#endif

#define TLV_BLOCK_APPEND    0
#define TLV_BLOCK_QUERY     1
#define TLV_BLOCK_DELETE    2

#define log_line(lfmt, ...)           \
    do {                              \
        printf(lfmt, ##__VA_ARGS__);  \
        printf("\n");                 \
    }while (0)

#if FLASH_TLV_DEBUG
    #define log(fmt, ...)      log_line(fmt, ##__VA_ARGS__)
#else
    #define log(...)
#endif

#if FLASH_TLV_USE_CACHE
static cache_obj_t tlv_cache;
#endif

static tlv_err_t search_tlv(tlv_sector_t *sector, tlv_block_t *block, uint8_t flag);

static uint32_t flash_tlv_gc(tlv_sector_t *sector);

/**
 * @brief 初始化tlv存储扇区地址
 * @note major和minor扇区在记录中会交换使用
 * @param major 扇区1
 * @param minor 扇区2
 * @param size 扇区大小(bytes)
 * */
void flash_tlv_init(tlv_sector_t *sector, uint32_t major, uint32_t minor, uint16_t size) {
    sector->major_sector = major;
    sector->minor_sector = minor;
    sector->sector_size = size;

    sector->work_sector = INVALID_ADDRESS;
    sector->mark_address = 0;
    sector->dirty_blocks = 0;
#if FLASH_TLV_USE_CACHE
    invalidate_cache(&tlv_cache);
#endif
}

/**
 * @brief 格式化tlv占用的分区，用于物理擦除扇区内所有数据，需要先调用flash_tlv_init初始化tlv_sector_t
 * @note 通常不需要主动调用flash_tlv_format，新建的扇区会在第一次使用时自动初始化
 * @param sector tlv工作扇区
 * */
void flash_tlv_format(tlv_sector_t *sector) {
    const uint32_t sector_header = (TLV_VERSION_MIN << 16) | TLV_SECTOR_TAG;
    // 只擦除数据区并写入有效头
    flash_erase(sector->major_sector, sector->sector_size);
    flash_erase(sector->minor_sector, sector->sector_size);
    flash_write(sector->major_sector, sizeof(uint32_t), (uint8_t *)&sector_header);
    sector->work_sector = sector->major_sector;
#if FLASH_TLV_USE_CACHE
    invalidate_cache(&tlv_cache);
#endif
}

/**
 * @brief 追加一条记录，如果存在tag相同的旧记录，它将被标记删除
 * @note 如果启用了缓存，追加到Flash的记录会同步到缓存列表
 * @param sector tlv操作扇区
 * @param tag 写入的标签[0x0000, 0xFFFF]
 * @param data 写入的数据
 * @param length 数据的长度(bytes)
 * @return true: 写入成功, false: 空间不足写入失败
 * */
bool flash_tlv_append(tlv_sector_t *sector, uint16_t tag, const uint8_t *data, uint16_t length) {
    tlv_err_t status;
    tlv_block_t block;
    uint32_t count;
    uint32_t offset = 0;
    uint8_t crc8;
    uint8_t buffer[32];

    // 查找可用空间
    block.tag = tag;
    block.length = length;
    status = search_tlv(sector, &block, TLV_BLOCK_APPEND);
    if(status == TLV_RESULT_OK) {
        goto LAB_WRITE_TLV;
    }
    count = flash_tlv_gc(sector);
    if(count < (TLV_MEAT_SIZE + length)) {
        return false;
    }
    status = search_tlv(sector, &block, TLV_BLOCK_APPEND);
    if(status != TLV_RESULT_OK) {
        return false;
    }

    LAB_WRITE_TLV:
    // 计算CRC8
    crc8 = calc_crc8(0x00, (const uint8_t *)&tag, sizeof(uint16_t));
    crc8 = calc_crc8(crc8, (const uint8_t *)&length, sizeof(uint16_t));
    crc8 = calc_crc8(crc8, data, length);

    block.header = HEADER_VALID_TLV;
    block.status = TLV_STATE_WRITE;
    block.crc8 = crc8;
    // 写入数据
    flash_write(block.entity, TLV_MEAT_SIZE, (uint8_t *)&block);
    flash_write(block.entity + TLV_MEAT_SIZE, length, data);
    // 校验头部
    flash_read(block.entity, TLV_MEAT_SIZE, buffer);
    if(memcmp(buffer, (uint8_t *)&block, TLV_MEAT_SIZE) != 0) {
        return false;
    }
    // 校验数据域
    do {
        count = (length > 32) ? 32 : length;
        flash_read((block.entity + TLV_MEAT_SIZE + offset), count, buffer);
        if(memcmp(buffer, (data + offset), count) != 0) {
            return false;
        }
        offset += count;
        length -= count;
    }while(length);

    // 更新确认标记(1->0)，0xFE变成0xFC
    buffer[0] = TLV_STATE_VERIFY;
    flash_write((block.entity + 2), 1, buffer);
    // entity域更新到实际数据域起始地址
    block.entity += TLV_MEAT_SIZE;
    // 删除上一条相同tag的记录(如果存在)
    if(sector->mark_address != 0) {
        buffer[0] = TLV_STATE_DELETE;
        flash_write((sector->mark_address + 2), 1, buffer);
        log("mark delete:0x%04x", block.tag);
        sector->mark_address = 0;
        sector->dirty_blocks++;
    }
    // 更新缓存
#if FLASH_TLV_USE_CACHE
    log("append: add to cache");
    set_cache(&tlv_cache, tag, &block);
#endif
    return true;
}

/**
 * @brief 查询指定标签的记录，如果启用了缓存，会先尝试从缓存取数据
 * @param sector 工作扇区
 * @param tag 被查询的标签
 * @param block 用于接收查询结果(仅在返回值为true时有值)
 * @return true:查询成功
 * */
bool flash_tlv_query(tlv_sector_t *sector, uint16_t tag, tlv_block_t *block) {
    tlv_err_t err;
#if FLASH_TLV_USE_CACHE
    bool res = get_cache(&tlv_cache, tag, block);
    if(res) {
        log("fetch from cache");
        return true;
    }
#endif
    block->tag = tag;
    err = search_tlv(sector, block, TLV_BLOCK_QUERY);
#if FLASH_TLV_USE_CACHE
    if(err == TLV_RESULT_OK) {
        log("query: add to cache");
        set_cache(&tlv_cache, tag, block);
    }
#endif
    return (err == TLV_RESULT_OK);
}

/**
 * @brief 读取TLV结构的数据
 * @param tlv flash_tlv_query查询得到的TVL结构
 * @param buffer 存放读取数据的缓冲区
 * @param offset TLV数据域偏移量 < tlv.length
 * @param length TLV数据域读取的长度 >= 1
 * @return 实际读取到的长度
 * */
uint32_t flash_tlv_read(tlv_block_t *block, uint8_t *buffer, uint16_t offset, uint16_t length) {
    if(offset >= block->length) {
        return 0;
    }
    if((offset + length) > block->length) {
        return 0;
    }
    flash_read(block->entity + offset, length, buffer);
    return length;
}

/**
 * @brief 验证flash_tlv_query获取到的TLV记录块完整性，使用CRC8
 * @note 验证操作是可选的
 * @param block 被验证的TLV数据块
 * */
bool flash_tlv_verify(tlv_block_t *block) {
    uint8_t crc8;
    uint8_t buffer[32];
    uint32_t trunk, offset = 0;
    uint32_t length = block->length;
    // Tag and length
    memcpy(buffer, &(block->tag), sizeof(uint16_t));
    memcpy(buffer + 2, &(block->length), sizeof(uint16_t));
    crc8 = calc_crc8(0x00, buffer, 4);
    // data
    do {
        trunk = (length > 32) ? 32 : length;
        flash_read((block->entity  + offset), trunk, buffer);
        crc8 = calc_crc8(crc8, buffer, trunk);
        offset += trunk;
        length -= trunk;
    }while(length);

    return (block->crc8 == crc8);
}

/**
 * @brief 删除指定Tag的记录，只是标记删除
 * @return true: 删除成功, false: 无此标签
 * */
bool flash_tlv_delete(tlv_sector_t *sector, uint16_t tag) {
    tlv_err_t err;
    tlv_block_t block;
#if FLASH_TLV_USE_CACHE
    remove_cache(&tlv_cache, tag);
#endif
    block.tag = tag;
    err = search_tlv(sector, &block, TLV_BLOCK_DELETE);
    return (err == TLV_RESULT_OK);
}

/**
 * 查找当前有效的工作扇区
 * @param tlv_sec major_sector和minor_sector必须配置完成，work_sector填充初值0xFFFFFFFF
 * @note 调用flash_tlv_init完成tlv_sec结构初始化
 * @return true:找到有效工作扇区
 * */
static bool find_valid_sector(tlv_sector_t *tlv_sec) {
    tlv_sector_header_t  header_major;
    tlv_sector_header_t  header_minor;

    flash_read(tlv_sec->major_sector, TLV_SECTOR_HEADER_SIZE, (uint8_t *)&header_major);
    flash_read(tlv_sec->minor_sector, TLV_SECTOR_HEADER_SIZE, (uint8_t *)&header_minor);

    if((header_major.tag == TLV_SECTOR_TAG) && (header_minor.tag == TLV_SECTOR_TAG)) {
        if(header_major.version == TLV_VERSION_MAX && header_minor.version == TLV_VERSION_MIN) {
            // 优化分支，避免GC完成后擦除一次旧主扇区
            tlv_sec->work_sector = tlv_sec->minor_sector;
        }else if(header_major.version == TLV_VERSION_MIN && header_minor.version == TLV_VERSION_MAX) {
            // 优化分支，避免GC完成后擦除一次旧主扇区
            tlv_sec->work_sector = tlv_sec->major_sector;
        }else {
            // 都有TLV_SECTOR_TAG, 取版本号大的为主分区
            tlv_sec->work_sector = (header_major.version > header_minor.version) ?
                    tlv_sec->major_sector : tlv_sec->minor_sector;
        }
    }else if((header_major.tag != TLV_SECTOR_TAG) && (header_minor.tag != TLV_SECTOR_TAG)) {
        // 两个分区tag都无效时，全部格式化，初始化为major分区
        flash_tlv_format(tlv_sec);

    }else if(header_major.tag == TLV_SECTOR_TAG) {
        tlv_sec->work_sector = tlv_sec->major_sector;

    }else if(header_minor.tag == TLV_SECTOR_TAG) {
        tlv_sec->work_sector = tlv_sec->minor_sector;
    }
    return (tlv_sec->work_sector != INVALID_ADDRESS);
}

/**
 * @brief 检查TLV数据块，只检查meta域，数据域发生错误不影响存储结构迭代
 * @note 检查条件：header!=0xFFFF and 0xAA55, 0xstatus!=0xFF, length!=0xFFFF 且在当前扇区范围内
 *       检查不通过时下一block地址为当前block地址+元数据大小(8字节)。(假定Nor Flash是顺序编程的)
 * @param current_addr 当前TLV结构的物理地址
 * @param end_addr 当前工作扇区的结束地址，可访问地址=(end_addr - 1)
 * @param block 被检查的TLV
 * @return true:block检查通过，false:不通过
 * */
static bool check_tlv_block(uint32_t current_addr, uint32_t end_addr, tlv_block_t *block) {
    if(block->header == HEADER_EMPTY_TLV) {
        // test pass
        return true;
    }
    if(block->header != HEADER_VALID_TLV) {
        // test pass
        return false;
    }
    if((block->status == TLV_STATE_NONE) || (block->length == 0xFFFF)) {
        // test pass, test pass
        return false;
    }
    // test pass
    uint32_t available = (end_addr - current_addr - TLV_MEAT_SIZE);
    return (block->length <= available);
}

/**
 * @brief 按操作类型搜索数据块
 * @note TLV_BLOCK_APPEND: block.tag和block.length需要填写
 *       TLV_BLOCK_QUERY和TLV_BLOCK_DELETE: 只需要block.tag
 * @param sector 操作扇区
 * @param block 记录块
 * @param flag 搜索类型
 * */
static tlv_err_t search_tlv(tlv_sector_t *sector, tlv_block_t *block, uint8_t flag) {
    bool status = true;
    tlv_block_t temp_block;
    uint32_t start_addr, end_addr;
    const uint8_t delete_flag = TLV_STATE_DELETE;
    // 查找可用工作扇区
    if(sector->work_sector == INVALID_ADDRESS) {
        status = find_valid_sector(sector);
        log("find valid sector:0x%08x", sector->work_sector);
    }
    if(!status) {
        return TLV_NO_VALID_SECTOR;
    }
    start_addr = (sector->work_sector + TLV_SECTOR_HEADER_SIZE);
    sector->dirty_blocks = 0;
    log("use start addr:0x%08x", start_addr);

    end_addr = (start_addr >> 12) + 1;
    end_addr <<= 12;

    while(start_addr < end_addr) {
        if((start_addr + TLV_MEAT_SIZE) > end_addr) {
            return TLV_META_SPACE_LOW;
        }
        log("flash read:0x%08x", start_addr);
        flash_read(start_addr, TLV_MEAT_SIZE, (uint8_t *)&temp_block);
        if(!check_tlv_block(start_addr, end_addr, &temp_block)) {
            start_addr += TLV_MEAT_SIZE;
            sector->dirty_blocks++;
            log("bad block");
            continue;
        }
        if(temp_block.header == HEADER_VALID_TLV) {
            if(temp_block.status != TLV_STATE_VERIFY) {
                sector->dirty_blocks++;
            }
            if(temp_block.tag == block->tag) {
                if((flag == TLV_BLOCK_APPEND) && (temp_block.status != TLV_STATE_DELETE)) {
                    // 追加新记录时，遇到相同TAG的旧记录缓存下来
                    // 新纪录写入完成后，利用缓存地址将旧记录标记删除
                    sector->mark_address = start_addr;
                    log("mark:0x%04x,0x%08x", block->tag, sector->mark_address);
                }else if((flag == TLV_BLOCK_QUERY) && (temp_block.status == TLV_STATE_VERIFY)) {
                    // 查询时，找到相同TAG且状态有效
                    memcpy(block, &temp_block, TLV_MEAT_SIZE);
                    block->entity = (start_addr + TLV_MEAT_SIZE);
                    return TLV_RESULT_OK;
                }else if((flag == TLV_BLOCK_DELETE) && (temp_block.status != TLV_STATE_DELETE)) {
                    flash_write((start_addr + 2), 1, &delete_flag);
                    sector->dirty_blocks++;
                    return TLV_RESULT_OK;
                }
            }
            // 下一TLV块
            start_addr += (TLV_MEAT_SIZE + temp_block.length);
            log("next start_addr:0x%08x", start_addr);

        }else if(temp_block.header == HEADER_EMPTY_TLV) {
            if(flag == TLV_BLOCK_APPEND) {
                if((end_addr - start_addr) >= (TLV_MEAT_SIZE + block->length)) {
                    block->entity = start_addr;
                    return TLV_RESULT_OK;
                }else {
                    return TLV_DATA_SPACE_LOW;
                }
            }
            // flag is 'TLV_BLOCK_QUERY' or 'TLV_BLOCK_DELETE'
            break;
        }
    }
    return TLV_RESULT_NOT_FOUND;
}

/**
 * @brief tlv扇区整理，方式为标记+整理，完成后有效扇区无碎片产生
 * @return GC完成后可用空间(bytes)
 * */
static uint32_t flash_tlv_gc(tlv_sector_t *sector) {
    uint32_t swap_sector;
    uint32_t read_addr, write_addr, end_addr;
    tlv_sector_header_t sector_header;

    tlv_block_t temp_block;
    uint8_t buffer[32];
    uint32_t trunk;

    // 只会在flash_tlv_append时发生GC操作
    // 经历过一次全扇区扫描，可用得到准确的dirty_blocks数量
    if(sector->dirty_blocks == 0) {
        return 0;
    }
    swap_sector = (sector->work_sector == sector->major_sector) ?
                  sector->minor_sector : sector->major_sector;

    read_addr = (sector->work_sector + TLV_SECTOR_HEADER_SIZE);
    write_addr = (swap_sector + TLV_SECTOR_HEADER_SIZE);

    end_addr = (read_addr >> 12) + 1;
    end_addr <<= 12;

    flash_erase(swap_sector, sector->sector_size);

    while(read_addr < end_addr) {
        if((read_addr + TLV_MEAT_SIZE) > end_addr) {
            break;
        }
        flash_read(read_addr, TLV_MEAT_SIZE, (uint8_t *)&temp_block);
        if(!check_tlv_block(read_addr, end_addr, &temp_block)) {
            read_addr += TLV_MEAT_SIZE;
            continue;
        }
        if(temp_block.status == TLV_STATE_VERIFY) {
            // 移动有效数据到第二分区
            flash_write(write_addr, TLV_MEAT_SIZE, (uint8_t *)&temp_block);
            write_addr += TLV_MEAT_SIZE;
            read_addr += TLV_MEAT_SIZE;
            do{
                trunk = (temp_block.length > 32) ? 32 : temp_block.length;

                flash_read(read_addr, trunk, buffer);
                flash_write(write_addr, trunk, buffer);

                read_addr += trunk;
                write_addr += trunk;
                temp_block.length -= trunk;
            }while(temp_block.length);
        }else {
            read_addr += (TLV_MEAT_SIZE + temp_block.length);
        }
    }

    flash_read(sector->work_sector, TLV_SECTOR_HEADER_SIZE, (uint8_t *)&sector_header);
    if(sector_header.version == TLV_VERSION_MAX) {
        sector_header.version = TLV_VERSION_MIN;
    }else {
        sector_header.version++;
    }
    flash_write(swap_sector, TLV_SECTOR_HEADER_SIZE, (uint8_t *)&sector_header);
    sector->work_sector = swap_sector;

    end_addr = (swap_sector >> 12) + 1;
    end_addr <<= 12;
    log("gc done: %d", (end_addr - write_addr));
    return (end_addr - write_addr);
}

//
// Created by Yanye on 3/13/2022.
//

#ifndef FLASHTLV_SPI_FLASH_H
#define FLASHTLV_SPI_FLASH_H

#include "stdint.h"

#define FLASH_PAGE_SIZE    0x1000

void flash_create();
void flash_import(const char *filepath);
void flash_export(const char *filepath);
void flash_delete();

void flash_erase(uint32_t addr, uint32_t size);
void flash_write(uint32_t addr, uint32_t length, const uint8_t *buffer);
void flash_read(uint32_t addr, uint32_t length, uint8_t *buffer);

#endif //FLASHTLV_SPI_FLASH_H

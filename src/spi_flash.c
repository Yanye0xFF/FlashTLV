//
// Created by Yanye on 3/13/2022.
//
#include "spi_flash.h"
#include "stdlib.h"
#include "string.h"
#include "stdio.h"

static uint8_t *mem;

void flash_create() {
    mem = malloc(8192);
    printf("flash::malloc 8192 bytes\n");
}

void flash_import(const char *filepath) {
    FILE *file = fopen(filepath, "rb+");
    fread(mem, 8192, 1, file);
    fclose(file);
    printf("flash::import from: %s\n", filepath);
}

void flash_export(const char *filepath) {
    FILE *file = fopen(filepath, "wb+");
    fwrite(mem, 8192, 1, file);
    fclose(file);
    printf("flash::export at: %s\n", filepath);
}

void flash_delete() {
    free(mem);
    printf("flash::deleted\n");
}

/**
 * @brief Flash擦除
 * @param addr 擦除的起始地址
 * @param size 擦除的大小(bytes)
 * */
void flash_erase(uint32_t addr, uint32_t size) {
    memset(mem + addr, 0xFF, size);
    printf("flash::erase %08x, size:%d\n", addr, size);
}

/**
 * @brief Flash写入
 * @note 写入前需要保证地址对应的扇区擦除过
 * @param addr 写入的起始地址
 * @param length 写入的长度(bytes)
 * @param buffer 写入的数据
 * */
void flash_write(uint32_t addr, uint32_t length, const uint8_t *buffer) {
    memcpy(mem + addr, buffer, length);
}

/**
 * @brief Flash读取
 * @param addr 读取的起始地址
 * @param length 读取的长度(bytes)
 * @param buffer 存放读出的数据
 * */
void flash_read(uint32_t addr, uint32_t length, uint8_t *buffer) {
    memcpy(buffer, mem + addr,length);
}

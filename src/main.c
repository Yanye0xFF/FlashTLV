#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "spi_flash.h"
#include "flash_tlv.h"

static void test_append(tlv_sector_t *sec);
static void test_gc(tlv_sector_t *sec);
static void test_read(tlv_sector_t *sec);
static void test_delete(tlv_sector_t *sec);

int main(int argc, char **argv) {
    tlv_sector_t tlvSector;

    flash_create();
    //flash_import("G:\\ramdisk.bin");

    printf("flash_tlv_init\n");
    flash_tlv_init(&tlvSector, 0x0, 0x1000, 4096);

    printf("test_append\n");
    test_append(&tlvSector);

    printf("test_gc\n");
    test_gc(&tlvSector);

    printf("test_read\n");
    test_read(&tlvSector);

    printf("test_delete\n");
    test_delete(&tlvSector);

    flash_export("G:\\ramdisk.bin");

    flash_delete();

    return 0;
}

static void test_append(tlv_sector_t *sec) {
    const char *text = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    const char *text2 = "my flash tlv data container";
    const char *text3 = "replace text";
    const uint8_t buffer[] = {0x11, 0x22, 0x33, 0x44};

    flash_tlv_append(sec, 0x1122, text, strlen(text));
    flash_tlv_append(sec, 0x1123, text2, strlen(text2));
    flash_tlv_append(sec, 0xCCAA, buffer, 4);
    flash_tlv_append(sec, 0x1122, text3, strlen(text3));
    flash_tlv_append(sec, 0xCC69, buffer, 4);
}

static void test_gc(tlv_sector_t *sec) {
    uint8_t bufferExt[16];
    for(int i = 0; i < 166; i++) {
        memset(bufferExt, i, 16);
        flash_tlv_append(sec, i, bufferExt, 16);
    }
}

static void test_read(tlv_sector_t *sec) {
    tlv_block_t block;
    bool result;
    uint8_t *buffer;

    result = flash_tlv_query(sec, 0x1122, &block);
    printf("query result:%d\n", result);

    result = flash_tlv_verify( &block);
    printf("verify result:%d\n", result);

    buffer = (uint8_t *)malloc(block.length);

    uint32_t read = flash_tlv_read(&block, buffer, 0, block.length);
    printf("read bytes:%d\n", read);

    printf("read data: \"");
    for(int i = 0; i < block.length; i++) {
        putchar(buffer[i]);
    }
    printf("\"\n");

    free(buffer);
}

static void test_delete(tlv_sector_t *sec) {
    bool result = flash_tlv_delete(sec, 0xCC69);
    printf("delete result:%d\n", result);
}

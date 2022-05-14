cmake_minimum_required(VERSION 3.21)
project(FlashTLV C)

set(CMAKE_C_STANDARD 99 -02)

add_executable(FlashTLV
        src/main.c
        src/spi_flash.h
        src/spi_flash.c
        src/utils.c
        src/utils.h
        src/flash_tlv.h
        src/flash_tlv.c
        src/flash_tlv_cache.c src/flash_tlv_cache.h)

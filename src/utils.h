/*
 * utils.h
 * @brief
 */

#ifndef _UTILS_H_
#define _UTILS_H_

#include "stdint.h"

uint32_t calc_crc32(uint32_t crc, const void *buffer, size_t size);

unsigned char calc_crc8(unsigned char init, const unsigned char *data, unsigned int len);

#endif /* EXAMPLES_BLE_EPD_SRC_UTILS_H_ */

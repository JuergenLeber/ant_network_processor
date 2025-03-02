/*
This software is subject to the license described in the LICENSE_A+SS.txt file
included with this software distribution. You may not use this file except in compliance
with this license.

Copyright (c) Garmin Canada Inc. 2019
All rights reserved.
*/

#ifndef DSI_UTILITY_H
#define DSI_UTILITY_H

#include <stdbool.h>
#include <stdint.h>
#include "appconfig.h"

/**
 * @brief Read unsigned 16-bit from buffer (little endian)
 */
uint16_t DSI_GetUShort(uint8_t *pucData);

/**
 * @brief Write unsigned 16-bit from buffer (little endian)
 */
void DSI_PutUShort(uint16_t val, uint8_t *pucData);

/**
 * @brief Read unsigned 32-bit from buffer (little endian)
 */
uint32_t DSI_GetULong(uint8_t *pucData);

/**
 * @brief Buffer copy utility
 */
void DSI_memcpy(uint8_t *pucDest, uint8_t *pucSrc, uint8_t ucSize);

/**
 * @brief Buffer set utility
 */
void DSI_memset(uint8_t *pucDest, uint8_t ucValue, uint8_t ucSize);

/**
 * @brief Buffer compare utility
 */
bool DSI_memcmp(uint8_t *pucSrc1, uint8_t *pucSrc2, uint8_t ucSize);


#endif // DSI_UTILITY_H

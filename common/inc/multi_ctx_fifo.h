/*
This software is subject to the license described in the LICENSE_A+SS.txt file
included with this software distribution. You may not use this file except in compliance
with this license.

Copyright (c) Garmin Canada Inc. 2019
All rights reserved.
*/

 #ifndef MULTI_CTX_FIFO_H
 #define MULTI_CTX_FIFO_H

/**
 * Provides a generic FIFO that allows for queueing data at multiple contexts.
 * All data must be dequeued at a single context level.
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

// Can change this depending on how large of a fifo is required.
typedef uint16_t fifo_offset_t;

typedef struct
{
   uint8_t *pucBuff;
   fifo_offset_t uiSize;
   fifo_offset_t uiHead;
   fifo_offset_t uiTail;
   // Used to handle simultaneous push ops at multiple contexts.
   // It is advanced at the start of every push. The real head is
   // updated once all push ops are confirmed complete.
   fifo_offset_t uiPushHead;
} multi_ctx_fifo_t;

#define mc_fifo_init(pstFifo, uiLen) do {    \
   multi_ctx_fifo_t *_pstFifo = (pstFifo);   \
   memset(_pstFifo, 0, sizeof(*_pstFifo));   \
   static uint8_t _aucFifoBuff [uiLen];     \
   _pstFifo->pucBuff = _aucFifoBuff;         \
   _pstFifo->uiSize = sizeof(_aucFifoBuff);  \
   } while (0)

/**
 * Insert new data into the fifo.
 *
 * This may be called at any context level.
 *
 * Operation is atomic w.r.t. all other pop and push operations.
 * The push will not be partial, either all of the data will be inserted or
 * the fifo will be left unchanged.
 *
 * @param[in] pstFifo The fifo to append to.
 * @param[in] pvSrc The data to append.
 * @param[in] uiLen Length of data (in bytes) to append.
 *
 * @return true if the data was copied into the fifo, false if it could not
 *          fit.
 */
bool mc_fifo_push(multi_ctx_fifo_t *pstFifo, const void *pvSrc, fifo_offset_t uiLen);

/**
 * Retrieve data from the head of the fifo.
 *
 * The application must ensure that all pops are done from the same context
 * level.
 *
 * Operation is atomic w.r.t. all push operations.
 * The pop will not be partial. Either the full amount of requested data will
 * be removed, or the fifo will be left unchanged.
 *
 * @param[in] pstFifo The fifo to remove data from.
 * @param[out] pvDst The destination buffer for the data.
 * @param[in] uiLen The length of data to copy out of the fifo.
 *
 * @return true if the data was copied out of the fifo, false if there was not
 *          enough data to satisfy the request.
 */
bool mc_fifo_pop(multi_ctx_fifo_t *pstFifo, void *pvDst, fifo_offset_t uiLen);

/**
 * Query the amount of valid data currently in the fifo.
 *
 * Can be called from any context level.
 *
 * The result is atomic w.r.t. all push and pop operations.
 *
 * @param[in] pstFifo The fifo to query the status of.
 *
 * @return the amount of data in the fifo.
 */
fifo_offset_t mc_fifo_get_data_len(multi_ctx_fifo_t *pstFifo);

#endif // MULTI_CTX_FIFO_H

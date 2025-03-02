/*
This software is subject to the license described in the LICENSE_A+SS.txt file
included with this software distribution. You may not use this file except in compliance
with this license.

Copyright (c) Garmin Canada Inc. 2019
All rights reserved.
*/

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "multi_ctx_fifo.h"
#include "nrf_nvic.h"

// a - b with wraparound for given size.
static fifo_offset_t fifo_diff(
   fifo_offset_t a,
   fifo_offset_t b,
   fifo_offset_t size)
{
   if (b <= a)
   {
      return a - b;
   }
   else
   {
      return (size - b) + a;
   }
}

// a + b with wraparound for given size.
static fifo_offset_t fifo_sum(
   fifo_offset_t a,
   fifo_offset_t b,
   fifo_offset_t size)
{
   if (b < (size - a))
   {
      return a + b;
   }
   else
   {
      return b - (size - a);
   }
}

/**
 * Attempt to allocate a chunk of the fifo buffer for pushing data.
 * If successful the starting offset to use for the write will be returned.
 * Otherwise the size of the fifo will be returned to indicate failure.
 */
static fifo_offset_t mc_fifo_push_alloc(multi_ctx_fifo_t *pstFifo, fifo_offset_t uiLen)
{
   fifo_offset_t uiChunkStart = pstFifo->uiSize;

   uint8_t bNested;
   sd_nvic_critical_region_enter(&bNested);

   // Math is done as size - used_space in order to properly deal with the
   // fifo empty condition (head == tail). It also subtracts 1 from the
   // free space in order to avoid setting the empty condition when the fifo
   // is in fact full.
   fifo_offset_t uiFreeSpace =
      (pstFifo->uiSize - fifo_diff(pstFifo->uiPushHead, pstFifo->uiTail, pstFifo->uiSize)) - 1;

   if (uiLen <= uiFreeSpace)
   {
      uiChunkStart = pstFifo->uiPushHead;
      pstFifo->uiPushHead = fifo_sum(uiChunkStart, uiLen, pstFifo->uiSize);
   }

   sd_nvic_critical_region_exit(bNested);

   return uiChunkStart;
}

bool mc_fifo_push(multi_ctx_fifo_t *pstFifo, const void *pvSrc, fifo_offset_t uiLen)
{
   fifo_offset_t uiChunkStart = mc_fifo_push_alloc(pstFifo, uiLen);
   const uint8_t* pucRawSrc = pvSrc;

   if (uiChunkStart == pstFifo->uiSize)
   {
      return false;
   }

   fifo_offset_t chunk_split = pstFifo->uiSize - uiChunkStart;
   if (chunk_split < uiLen)
   {
      memcpy(&pstFifo->pucBuff[uiChunkStart], &pucRawSrc[0], chunk_split);
      memcpy(&pstFifo->pucBuff[0], &pucRawSrc[chunk_split], uiLen - chunk_split);
   }
   else
   {
      memcpy(&pstFifo->pucBuff[uiChunkStart], pucRawSrc, uiLen);
   }

   // Don't need a critical section for this check because the head pointer
   // is always adjusted by the lowest context that allocated data.
   if (uiChunkStart == pstFifo->uiHead)
   {
      // This push is the lowest-context push. Need to adjust the head pointer
      // to commit all the data.
      // This works because after we allocated our section all other pushes
      // at higher contexts must occur before or after the following critical
      // section.
      // If before, they will have updated push_head and completed their data
      // copy, so will be accounted for in the copy of push_head to head.
      // If after then they will see that push_head == head when they alloc, and
      // thus will be the new lowest-context push.
      uint8_t bNested;
      sd_nvic_critical_region_enter(&bNested);
      pstFifo->uiHead = pstFifo->uiPushHead;
      sd_nvic_critical_region_exit(bNested);
   }

   return true;
}

bool mc_fifo_pop(multi_ctx_fifo_t *pstFifo, void *pvDst, fifo_offset_t uiLen)
{
   // This function does not need critical section for the following reasons:
   // The single read of the head is atomic.
   // All pops (and therefore moves of the tail) are done at the same context.
   // Outside of that tail is always accessed read-only, and is updated
   // atomically here.

   fifo_offset_t uiCachedHead = pstFifo->uiHead;
   uint8_t *pucRawDst = pvDst;

   fifo_offset_t data_avail = fifo_diff(uiCachedHead, pstFifo->uiTail, pstFifo->uiSize);

   if (data_avail < uiLen)
   {
      return false;
   }

   fifo_offset_t chunk_split = pstFifo->uiSize - pstFifo->uiTail;
   if (chunk_split < uiLen)
   {
      memcpy(&pucRawDst[0], &pstFifo->pucBuff[pstFifo->uiTail], chunk_split);
      memcpy(&pucRawDst[chunk_split], &pstFifo->pucBuff[0], uiLen - chunk_split);
   }
   else
   {
      memcpy(pucRawDst, &pstFifo->pucBuff[pstFifo->uiTail], uiLen);
   }

   pstFifo->uiTail = fifo_sum(pstFifo->uiTail, uiLen, pstFifo->uiSize);
   return true;
}

fifo_offset_t mc_fifo_get_data_len(multi_ctx_fifo_t *pstFifo)
{
   fifo_offset_t uiDataLen;

   uint8_t bNested;
   sd_nvic_critical_region_enter(&bNested);
   uiDataLen = fifo_diff(pstFifo->uiHead, pstFifo->uiTail, pstFifo->uiSize);
   sd_nvic_critical_region_exit(bNested);

   return uiDataLen;
}

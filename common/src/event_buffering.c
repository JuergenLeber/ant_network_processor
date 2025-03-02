/*
This software is subject to the license described in the LICENSE_A+SS.txt file
included with this software distribution. You may not use this file except in compliance
with this license.

Copyright (c) Garmin Canada Inc. 2019
All rights reserved.
*/

#include "nrf_nvic.h"
#include "appconfig.h"
#include "ant_interface.h"
#include "ant_parameters.h"
#include "dsi_utility.h"
#include "event_buffering.h"
#include "multi_ctx_fifo.h"
#include "system.h"

#define TIMEBASE_CONVERSION_TO_10MS       ((uint16_t) 328) //convert # in 10ms time frame to # of ticks (32768 time base)

// TODO: These should probably go in ant_parameters.h
#define EVENT_BUFFER_CONFIG_LOW_PRIO      0x00
#define EVENT_BUFFER_CONFIG_ALL           0x01

static volatile bool bFlushing;

static uint8_t ucConfig;
static uint16_t usSizeThreshold;
static uint32_t ulTimeThreshold;
static uint32_t ulFlushTime;

static multi_ctx_fifo_t stEventFifo;

static bool is_event_bufferable(uint8_t event)
{
   switch (ucConfig)
   {
      case EVENT_BUFFER_CONFIG_LOW_PRIO:
         switch (event)
         {
            case EVENT_TX:
            case EVENT_RX_FAIL:
            case EVENT_CHANNEL_COLLISION:
               return true;
         }
         break;

      case EVENT_BUFFER_CONFIG_ALL:
         return true;
   }

   return false;
}

static bool has_flush_timeout_expired(void)
{
   return (ulTimeThreshold != 0) &&
      ((System_GetTime_32K() - ulFlushTime) >= ulTimeThreshold);
}

void event_buffering_init(void)
{
   bFlushing = false;
   ucConfig = DEFAULT_EVENT_BUFFERING_CONFIG;
   usSizeThreshold = DEFAULT_EVENT_BUFFERING_SIZE_THRESHOLD;
   ulTimeThreshold = DEFAULT_EVENT_BUFFERING_TIME_THRESHOLD;
   ulFlushTime = System_GetTime_32K();

   mc_fifo_init(&stEventFifo, ANT_STACK_MESSAGE_QUEUE_SIZE);
}

bool event_buffering_put(const ant_event_t *pstEvent)
{
   fifo_offset_t uiTotalSize =
      pstEvent->stMessage.ANT_MESSAGE_ucSize +
      sizeof(pstEvent->stHeader) +
      MESG_SIZE_SIZE + MESG_ID_SIZE;

   bool was_pushed = mc_fifo_push(&stEventFifo, &pstEvent->stHeader, uiTotalSize);

   // Use header pointer to skip the padding.
   if (!was_pushed ||
      !is_event_bufferable(pstEvent->stHeader.ucEvent) ||
      mc_fifo_get_data_len(&stEventFifo) > usSizeThreshold ||
      has_flush_timeout_expired())
   {
      event_buffering_flush();
   }

   return was_pushed;
}

bool event_buffering_get(ant_event_hdr_t *pstEvent, ANT_MESSAGE *pstMessage)
{
   bool bGotMsg = false;

   if (bFlushing)
   {
      // Cleared here in case more data is pushed while we check for a message.
      bFlushing = false;

      bGotMsg = mc_fifo_pop(&stEventFifo, pstEvent, sizeof(*pstEvent));

      if (bGotMsg)
      {
         // Continue flushing
         bFlushing = true;

         mc_fifo_pop(
            &stEventFifo,
            &pstMessage->ANT_MESSAGE_ucSize,
            MESG_SIZE_SIZE);
         mc_fifo_pop(
            &stEventFifo,
            pstMessage->ANT_MESSAGE_aucFramedData,
            pstMessage->ANT_MESSAGE_ucSize + MESG_ID_SIZE);
      }
   }

   return bGotMsg;
}

void event_buffering_config_set(uint8_t ucConfig_, uint16_t usSizeThreshold_, uint16_t usTimeThreshold_)
{
   if (usSizeThreshold_ > ANT_STACK_MESSAGE_QUEUE_SIZE)
   {
      usSizeThreshold_ = ANT_STACK_MESSAGE_QUEUE_SIZE;
   }

   // Need to request the system timer when performing time-based event buffering.
   if (ulTimeThreshold == 0 && usTimeThreshold_ != 0)
   {
      System_TimerRequest();
   }
   else if (ulTimeThreshold != 0 && usTimeThreshold_ == 0)
   {
      System_TimerRelease();
   }

   // No need to do this atomically, we are flushing the buffer at the end regardless of the settings.
   ucConfig = ucConfig_;
   usSizeThreshold = usSizeThreshold_;
   ulTimeThreshold = (uint32_t)usTimeThreshold_ * TIMEBASE_CONVERSION_TO_10MS;

   event_buffering_flush();
}

void event_buffering_config_get(uint8_t *pucConfig, uint16_t *pusSizeThreshold, uint16_t *pusTimeThreshold)
{
   // No need to do this atomically, get and set are always called at thread context.
   *pucConfig = ucConfig;
   *pusSizeThreshold = usSizeThreshold;
   *pusTimeThreshold = ulTimeThreshold / TIMEBASE_CONVERSION_TO_10MS;
}

void event_buffering_flush(void)
{
   bFlushing = true;
   ulFlushTime = System_GetTime_32K();
}

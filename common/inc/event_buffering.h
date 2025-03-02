/*
This software is subject to the license described in the LICENSE_A+SS.txt file
included with this software distribution. You may not use this file except in compliance
with this license.

Copyright (c) Garmin Canada Inc. 2019
All rights reserved.
*/

#ifndef _EVENT_BUFFERING_H_
#define _EVENT_BUFFERING_H_

#include <stdbool.h>
#include <stdint.h>

#include "ant_interface.h"
#include "ant_parameters.h"

#define DEFAULT_EVENT_BUFFERING_CONFIG             0
#define DEFAULT_EVENT_BUFFERING_SIZE_THRESHOLD     0
#define DEFAULT_EVENT_BUFFERING_TIME_THRESHOLD     0

typedef struct
{
   uint8_t ucChannel;
   uint8_t ucEvent;
} ant_event_hdr_t;

typedef struct
{
   // Needed because ANT_MESSAGE is aligned to 4 bytes, but fifo needs continuous memory.
   uint8_t __padding[2];
   ant_event_hdr_t stHeader;
   ANT_MESSAGE stMessage;
} ant_event_t;

/**
 * Init event buffer.
 *
 * Call from thread context.
 */
void event_buffering_init(void);

/**
 * Put an event in the buffer.
 *
 * Call from thread or interrupt context.
 *
 * @return true if the message was placed in the buffer. false otherwise.
 *          It is up to the caller to hold onto the message and retry at a later
 *          time.
 */
bool event_buffering_put(const ant_event_t *pstEvent);

/**
 * Attempt to retrieve an event from the buffer.
 *
 * Call from thread context.
 *
 * This doesn't mirror the event_buffering_put call exactly. It is a result of
 * how main currently deals with the TxMessage variable (it's actually a pointer
 * into a serial buffer).
 *
 * @return true if there was an event to retrieve. false if there was no event
 *          to retrieve. This is used instead of NO_EVENT because NO_EVENT
 *          indicates command responses.
 */
bool event_buffering_get(ant_event_hdr_t *pstEvent, ANT_MESSAGE *pstMessage);

/**
 * Set the event buffering configuration.
 *
 * Controls the thresholds used for triggering buffer flushes.
 */
void event_buffering_config_set(uint8_t ucConfig, uint16_t usSizeThreshold, uint16_t usTimeThreshold);

/**
 * Retrieve the current event buffering configuration.
 */
void event_buffering_config_get(uint8_t *pucConfig, uint16_t *pusSizeThreshold, uint16_t *pusTimeThreshold);

/**
 * Trigger an explicit flush of the event buffer.
 *
 * Call from any context.
 */
void event_buffering_flush(void);

#endif //_EVENT_BUFFERING_H_

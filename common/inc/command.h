/*
This software is subject to the license described in the LICENSE_A+SS.txt file
included with this software distribution. You may not use this file except in compliance
with this license.

Copyright (c) Garmin Canada Inc. 2019
All rights reserved.
*/

#ifndef COMMAND_H
#define COMMAND_H

#include <stdbool.h>
#include <stdint.h>
#include "ant_parameters.h"
#include "appconfig.h"

typedef struct
{
   uint8_t ucChannel;
   uint8_t ucResponseID;
   uint8_t ucResponseSubID;
   uint8_t ucResponse;
   bool bExtIDResponse;

} COMMAND_RESPONSE;

// Mask for filtering out certain prohibited events (see command.c for list)
extern uint16_t usEventFilterMask;

/**
 * @brief ANT serial burst command message handler
 */
bool Command_BurstMessageProcess(ANT_MESSAGE *pstRxMessage, ANT_MESSAGE *pstTxMessage);

/**
 * @brief ANT serial command message handler
 */
void Command_SerialMessageProcess(ANT_MESSAGE *pstRxMessage, ANT_MESSAGE *pstTxMessage);

/**
 * @brief ANT serial command response handler
 */
void Command_ResponseMessage(COMMAND_RESPONSE stCmdResponse, ANT_MESSAGE *pstTxMessage);

#if defined (USE_INTERFACE_LOCK)
/**
 * @brief ANT serial command interface lock
 */
void Command_SetInterfaceLock(bool bLock);
#endif // USE_INTERFACE_LOCK

#endif // COMMAND_H

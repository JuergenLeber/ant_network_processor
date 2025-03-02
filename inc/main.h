/*
This software is subject to the license described in the LICENSE_A+SS.txt file
included with this software distribution. You may not use this file except in compliance
with this license.

Copyright (c) Garmin Canada Inc. 2019
All rights reserved.
*/

#ifndef MAIN_H
#define MAIN_H

#include <stdint.h>
#include "ant_parameters.h"
#include "appconfig.h"

/**
 * @brief Set application queued burst mode
 */
void Main_SetQueuedBurst(void);


/**
 * @brief Application debug command handler
 */
uint8_t Main_DebugMsgProcess(uint8_t *aucPayload, ANT_MESSAGE *pstTxMessage);

#endif // MAIN_H

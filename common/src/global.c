/*
This software is subject to the license described in the LICENSE_A+SS.txt file
included with this software distribution. You may not use this file except in compliance
with this license.

Copyright (c) Garmin Canada Inc. 2019
All rights reserved. Canada Inc. 2019
All rights reserved.
*/

#include "global.h"

#include <stdbool.h>
#include <stdint.h>

#include "appconfig.h"
#include "ant_interface.h"
#include "ant_parameters.h"
#include "compiler_abstraction.h"

/*
 * Global control flags: Must be accessed/changed atomically as they can be accessed by multiple contexts
 * If bitfields are used, the entire bitfield operation must be atomic!!
 */
volatile bool bEventRXSerialMessageProcess;
volatile bool bEventANTProcessStart;
volatile bool bEventANTProcess;
volatile bool bEventBurstMessageProcess;
volatile bool bEventStartupMessage;
volatile bool bEventSetBaudrate;
volatile uint8_t ucQueuedTxBurstChannel;
volatile bool bEventInternalProcess;

uint8_t ucBurstSequence;

ANT_ENABLE stANTChannelEnable;
__ALIGN(4) uint8_t aucANTChannelBlock[ANT_ENABLE_GET_REQUIRED_SPACE(ANT_STACK_TOTAL_CHANNELS_ALLOCATED_MAX, ANT_STACK_ENCRYPTED_CHANNELS_MAX, ANT_STACK_TX_BURST_QUEUE_SIZE_MAX, ANT_STACK_EVENT_QUEUE_NUM_EVENTS_MAX)];

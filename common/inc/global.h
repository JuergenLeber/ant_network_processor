/*
This software is subject to the license described in the LICENSE_A+SS.txt file
included with this software distribution. You may not use this file except in compliance
with this license.

Copyright (c) Garmin Canada Inc. 2019
All rights reserved.
*/

#ifndef GLOBAL_H
#define GLOBAL_H

#include <stdbool.h>
#include <stdint.h>
#include "appconfig.h"
#include "ant_interface.h"
#include "ant_parameters.h"

/*
 *  Global control flags: Must be accessed/changed atomically as they can be accessed by multiple contexts
 *  If bitfields are used, the entire bitfield operation must be atomic!!
 */
extern volatile bool bEventRXSerialMessageProcess;
extern volatile bool bEventANTProcessStart;
extern volatile bool bEventANTProcess;
extern volatile bool bEventBurstMessageProcess;
extern volatile bool bEventStartupMessage;
extern volatile bool bEventSetBaudrate;
extern volatile uint8_t ucQueuedTxBurstChannel;
extern volatile bool bEventInternalProcess;

extern uint8_t ucBurstSequence;

extern ANT_ENABLE stANTChannelEnable;
extern uint8_t aucANTChannelBlock[ANT_ENABLE_GET_REQUIRED_SPACE(ANT_STACK_TOTAL_CHANNELS_ALLOCATED_MAX, ANT_STACK_ENCRYPTED_CHANNELS_MAX, ANT_STACK_TX_BURST_QUEUE_SIZE_MAX, ANT_STACK_EVENT_QUEUE_NUM_EVENTS_MAX)];

#define APP_VERSION_SIZE    11
extern const char acAppVersion[];


#endif // GLOBAL_H

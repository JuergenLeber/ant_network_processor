/*
This software is subject to the license described in the LICENSE_A+SS.txt file
included with this software distribution. You may not use this file except in compliance
with this license.

Copyright (c) Garmin Canada Inc. 2019
All rights reserved.
*/

#ifndef APPCONFIG_H
#define APPCONFIG_H

// Pre-allocated maximum memory allocation block for scalable channel support
#define ANT_STACK_TOTAL_CHANNELS_ALLOCATED_MAX        15    // Max number of channels supported by ANT stack (SIZE_OF_NONENCRYPTED_ANT_CHANNEL bytes each)
#define ANT_STACK_ENCRYPTED_CHANNELS_MAX              15    // Max number of encrypted channels supported by ANT stack (SIZE_OF_ENCRYPTED_ANT_CHANNEL bytes each)
#define ANT_STACK_TX_BURST_QUEUE_SIZE_MAX             128   // Max burst queue size allocated to ANT stack (bytes)
#define ANT_STACK_EVENT_QUEUE_NUM_EVENTS_MAX          128   // Max number of events in the event queue

// Default SCALABLE_CHANNELS_DEFAULT settings
#define ANT_STACK_TOTAL_CHANNELS_ALLOCATED_DEFAULT    15    // Default number of channels supported by ANT stack (SIZE_OF_NONENCRYPTED_ANT_CHANNEL bytes each)
#define ANT_STACK_ENCRYPTED_CHANNELS_DEFAULT          15    // Default number of encrypted channels supported by ANT stack (SIZE_OF_ENCRYPTED_ANT_CHANNEL bytes each)
#define ANT_STACK_TX_BURST_QUEUE_SIZE_DEFAULT         128   // Default burst queue size allocated to ANT stack (bytes)
#define ANT_STACK_EVENT_QUEUE_NUM_EVENTS_DEFAULT      32    // Default number of events in event queue

#define ANT_STACK_MESSAGE_QUEUE_SIZE                  0x200 // Size in bytes of the message queue in the network processor. When full events will back up into the softdevice buffer.


#if defined (NRF52_N548_CONFIG) //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// General Application Config
//#define SERIAL_NUMBER_NOT_AVAILABLE                                        // No serial number support
#define COMPLETE_CHIP_SYSTEM_RESET                                         // ANT reset message causes NRF51 hard reset
#define SYSTEM_SLEEP                                                       // Enable deep sleep command
#define SERIAL_REPORT_RESET_MESSAGE                                        // Generate startup message

#define SCALABLE_CHANNELS_DEFAULT                                          // Use SCALABLE_CHANNELS default settings


#else
   #error undefined application configuration

#endif

#endif // APPCONFIG_H

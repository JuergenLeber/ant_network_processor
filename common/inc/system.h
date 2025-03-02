/*
This software is subject to the license described in the LICENSE_A+SS.txt file
included with this software distribution. You may not use this file except in compliance
with this license.

Copyright (c) Garmin Canada Inc. 2019
All rights reserved. Canada Inc. 2019
All rights reserved.
*/

#ifndef SYSTEM_H
#define SYSTEM_H

#include <stdbool.h>
#include <stdint.h>
#include "ant_parameters.h"
#include "appconfig.h"
#include "nrf_assert.h"

#define UNUSED_VARIABLE(X)  ((void)(X))
#define UNUSED_PARAMETER(X) UNUSED_VARIABLE(X)

/*
 * UICR reserved for Customer Block
 */
#define SYSTEM_UICR_CUST_WORDS_SIZE          31 // NRF52 specification states 0x080 - 0x0FC of UICR base is reserved for customer
#define SYSTEM_UICR_CUST_ANT_ID_OFFSET       4
#define SYSTEM_UICR_CUST_RSSI_CAL_OFFSET     3
#define SYSTEM_UICR_CUST_BIST_M6_OFFSET      2
#define SYSTEM_UICR_CUST_MFG_ESN_OFFSET      1
#define SYSTEM_UICR_CUST_BIST_OFFSET         0

/*
 * Device unique identifier types/locations
 */
#define SYSTEM_ID_MFG_ESN                    0
#define SYSTEM_ID_ANT_ID                     1

/*
 * RSSI Calibration Settings
 */
#if defined(D52Q_PREMIUM_MODULE) || defined(D52M_PREMIUM_MODULE)
#define SYSTEM_RSSI_CAL_OFFSET               127
#define SYSTEM_RSSI_CAL_INVALID              0xFF
#endif


typedef struct
{
   uint32_t USER_CFG[SYSTEM_UICR_CUST_WORDS_SIZE];
} NRF_UICR_Custom_Struct;
#define SYSTEM_UICR_CUST_STRUCT            ((NRF_UICR_Custom_Struct*) (NRF_UICR_BASE + 0x80)) //offset 0x080 nrF52 Reserved for Customer

/**
 * @brief Application system level initialization
 */
void System_Init(void);

/**
 * Used to do any low-priority work. Should be called at least once every time
 * through the main loop.
 */
void System_Tick(void);

/**
 * @brief Application system reset message
 */
void System_ResetMesg(ANT_MESSAGE *pstTxMessage);

/**
 * @brief Application system reset handler
 */
uint8_t System_Reset(uint8_t ucResetCmd);

/**
 * @brief Application deep sleep configuration handler
 */
uint8_t System_SetDeepSleep(ANT_MESSAGE *pstRxMessage); //counter 30.517 uS Resolution.

/**
 * @brief Used for asynchronous serial suspend function
 */
void System_SetSuspendSleep(void);

/**
 * @brief Application deep sleep handler
 */
void System_DeepSleep(void);



#if !defined (SERIAL_NUMBER_NOT_AVAILABLE)
/**
 * @brief Constructs serial number message and calls function to retrieve serial number.
 */
void System_GetSerialNumberMesg(ANT_MESSAGE *pstTxMessage);
/**
 * @brief Returns the requested unit ID.
 */
void System_GetSerialNumber(uint8_t ucID, uint8_t *pucESN);
/**
 * @brief Writes specified ID number to specified location in UICR
 */
uint8_t System_SetSerialNum(ANT_MESSAGE *pstRxMessage);

#endif // !SERIAL_NUMBER_NOT_AVAILABLE

/**
 * @brief Writes RSSI calibration byte to the UICR
 */
uint8_t System_SetRSSICal(ANT_MESSAGE *pstRxMessage);

/**
 * @brief Constructs RSSI calibration message and calls function to retrieve calibration data.
 */
void System_GetRSSICalDataMesg(ANT_MESSAGE *pstTxMessage);
/**
 * @brief Retrieves RSSI calibration data from UICR
 */
void System_GetRSSICalData(uint8_t *pucRSSICal);

/**
 * @brief Initiates a flash write operation with given parameters
 */
uint32_t System_FlashWrite(uint32_t* ulAddress, uint32_t * pulData, uint8_t ucLength);

/**
 * @brief Process for updating a flag when a flash write is complete
 */
void System_FlashEventProcess(uint32_t ulEvent);

/**
 * @brief Returns flag indicating whether a flash write is ongoing
 */
bool System_FlashBusy(void);

/**
 * @brief Read the given address to empty the system bus write buffer
 */
void System_WriteBufferEmpty(const volatile uint32_t * pulAddressToRead);

/**
 * Request that the system timer be enabled.
 *
 * Context: Main
 */
void System_TimerRequest(void);

/**
 * Release a request to keep the system timer enabled. Must match a call to
 * System_TimerRequest.
 *
 * Context: Main
 */
void System_TimerRelease(void);

/**
 * Gets a system time value that increments at 32KHz. Wraps at 0xFFFF_FFFF.
 *
 * Undefined behaviour if the system timer is disabled when time is requested.
 *
 * Context: Any
 */
uint32_t System_GetTime_32K(void);

#endif // SYSTEM_H

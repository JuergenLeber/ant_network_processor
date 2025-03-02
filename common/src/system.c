/*
This software is subject to the license described in the LICENSE_A+SS.txt file
included with this software distribution. You may not use this file except in compliance
with this license.

Copyright (c) Garmin Canada Inc. 2019
All rights reserved. Canada Inc. 2019
All rights reserved. Canada Inc. 2019
All rights reserved.
*/

#include "system.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "nrf.h"
#include "nrf_delay.h"

#include "nrf_soc.h"
#include "nrf_sdm.h"
#include "nrf_nvic.h"

#include "ant_interface.h"
#include "ant_parameters.h"
#include "ant_error.h"
#include "appconfig.h"
#include "boardconfig.h"
#include "dsi_utility.h"
#include "serial.h"
#include "global.h"

#include "app_util_platform.h"


// Temporary message IDs until they are added to the nrf-softdevice repos
#ifndef MESG_RSSI_CAL_ID
   #define MESG_RSSI_CAL_ID                              ((uint16_t)0xE402) ///< ANT application - device RSSI calibration value request ID
#else
   //#error "MESG_RSSI_CAL_ID: already defined, check ant_parameters.h"
#endif
#ifndef MESG_RSSI_CAL_SIZE
   #define MESG_RSSI_CAL_SIZE                            ((uint8_t)2)
#else
   //#error "MESG_RSSI_CAL_SIZE: already defined, check ant_parameters.h"
#endif
#ifndef MESG_SET_SERIAL_NUM_ID
   #define MESG_SET_SERIAL_NUM_ID                        ((uint16_t)0xE403) ///< ANT application - sets the device serial number in UICR
#else
   //#error "MESG_SET_SERIAL_NUM_ID: already defined, check ant_parameters.h"
#endif

#define SYS_TIME_RTC                                     NRF_RTC1
#define SYS_TIME_RTC_IRQn                                RTC1_IRQn
#define SYS_TIME_RTC_BITS                                24
#define SYS_TIME_RTC_OVRFLW                              (1ul << SYS_TIME_RTC_BITS)
#define SYS_TIME_RTC_HALF                                (SYS_TIME_RTC_OVRFLW >> 1)
// Since the RTC is on a different clock domain we need to delay to ensure
// tasks have taken effect.
#define SYS_TIME_RTC_TASK_LATENCY_US                     46

static bool bFlashBusy = false;                                         // For monitoring flash write progress

// Keep track of a base time since the RTC doesn't have enough bits.
static uint32_t ulSysTimeOffset;

static uint32_t ulTimerRequests;


/**
 * @brief Application system level initialization
 */
void System_Init(void)
{
   //Default pin settings.
   NRF_GPIO->OUT = PIN_OUT_INIT;
   NRF_GPIO->DIR = PIN_DIR_INIT;

   SCB->SCR |= SCB_SCR_SEVONPEND_Msk; // allow wakeup from enabled events and all interrupts (including disabled)

   // Setup RTC for time keeping at 32KHz
   SYS_TIME_RTC->TASKS_STOP = 1;
   SYS_TIME_RTC->TASKS_CLEAR = 1;
   // Needed to guarantee that the stop has taken effect.
   nrf_delay_us(SYS_TIME_RTC_TASK_LATENCY_US);

   ulSysTimeOffset = 0;
   SYS_TIME_RTC->INTENCLR = ~0;
   // The EVTENCLR write ensures there is enough time between writing INTENCLR and clearing the IRQ.
   SYS_TIME_RTC->EVTENCLR = ~0;
   sd_nvic_DisableIRQ(SYS_TIME_RTC_IRQn);
   sd_nvic_ClearPendingIRQ(SYS_TIME_RTC_IRQn);
   // Need interupt enabled here so that the overflow event wakes us up.
   SYS_TIME_RTC->EVENTS_OVRFLW = 0;
   SYS_TIME_RTC->INTENSET = RTC_INTENSET_OVRFLW_Msk;
   // Want to tick at input clock rate (32KHz).
   SYS_TIME_RTC->PRESCALER = 0;
   ulTimerRequests = 0;
}

void System_Tick(void)
{
   // Update time offset if needed. This can handle a large latency
   // (hundreds of seconds) so it's done here instead of in an actual interrupt.
   if (SYS_TIME_RTC->EVENTS_OVRFLW)
   {
      uint8_t bNested;
      sd_nvic_critical_region_enter(&bNested);
      ulSysTimeOffset += SYS_TIME_RTC_OVRFLW;
      // Clear the pending interrupt to allow sleep.
      SYS_TIME_RTC->EVENTS_OVRFLW = 0;
      // Make sure write takes effect before clearing the IRQ.
      (void)SYS_TIME_RTC->EVENTS_OVRFLW;
      sd_nvic_ClearPendingIRQ(SYS_TIME_RTC_IRQn);
      sd_nvic_critical_region_exit(bNested);
   }
}

/**
 * @brief Application system reset message
 */
void System_ResetMesg(ANT_MESSAGE *pstTxMessage)
{
   uint32_t ulErrorCode;
   uint32_t ulReason;
   uint8_t ucResetMesg = 0;  // Default power on reset

   ulErrorCode = sd_power_reset_reason_get((uint32_t*)&ulReason);
   if (ulErrorCode != NRF_SUCCESS)
   {
      ASSERT(false);
   }

//   if ( ulReason & POWER_RESETREAS_OFF_Msk) // from Power OFF mode (lowest powerdown). TODO: not sure is this is correct
//      ucResetMesg |= RESET_SUSPEND;
   if ( ulReason & POWER_RESETREAS_LOCKUP_Msk) // it looks like nrf_nvic_SystemReset generates this instead of SREQ
      ucResetMesg |= RESET_CMD;
   if ( ulReason & POWER_RESETREAS_SREQ_Msk) // from System reset req
      ucResetMesg |= RESET_CMD;
   if ( ulReason & POWER_RESETREAS_DOG_Msk) // from watchdog
      ucResetMesg |= RESET_WDT;
   if ( ulReason & POWER_RESETREAS_RESETPIN_Msk) // from the reset pin
      ucResetMesg |= RESET_RST;

   pstTxMessage->ANT_MESSAGE_aucMesgData[0] = ucResetMesg;
   pstTxMessage->ANT_MESSAGE_ucSize         = MESG_STARTUP_MESG_SIZE;
   pstTxMessage->ANT_MESSAGE_ucMesgID       = MESG_STARTUP_MESG_ID;
}

/**
 * @brief Application system reset handler
 */
uint8_t System_Reset(uint8_t ucResetCmd)
{
   uint32_t ulErrorCode;

#if defined (COMPLETE_CHIP_SYSTEM_RESET)

   ulErrorCode = sd_power_reset_reason_clr( POWER_RESETREAS_OFF_Msk | POWER_RESETREAS_LOCKUP_Msk | POWER_RESETREAS_SREQ_Msk | POWER_RESETREAS_DOG_Msk | POWER_RESETREAS_RESETPIN_Msk);
   if (ulErrorCode != NRF_SUCCESS)
   {
      ASSERT(false);
   }

   ulErrorCode = sd_nvic_SystemReset();
   // should not return here, but in case it does catch it
   if (ulErrorCode != NRF_SUCCESS)
   {
      ASSERT(false);
   }

#else
   #error //unsupported reset scheme

#endif
   return NO_RESPONSE_MESSAGE;
}

/**
 * @brief Application deep sleep configuration handler
 */
static uint8_t ucDeepSleepFlag = 0;
uint8_t System_SetDeepSleep(ANT_MESSAGE *pstRxMessage)
{
   if (pstRxMessage->ANT_MESSAGE_ucSize == 0x01)
      ucDeepSleepFlag = 1;
   else
      return INVALID_MESSAGE;

   return RESPONSE_NO_ERROR;
}

/**
 * @brief Used for asynchronous serial suspend function
 */
void System_SetSuspendSleep(void)
{
  ucDeepSleepFlag = 1;
}

/**
 * @brief Application deep sleep handler
 */
void System_DeepSleep(void)
{
   uint32_t ulErrorCode;

   if (ucDeepSleepFlag)
   {
      ulErrorCode = sd_power_system_off(); // this should not return;
      if (ulErrorCode != NRF_SUCCESS)
      {
         ASSERT(false);
      }
      while(1); //reset is our only hope.
   }
}

#if !defined (SERIAL_NUMBER_NOT_AVAILABLE)
/**
 * @brief Constructs serial number message and calls function to retrieve serial number.
 */
void System_GetSerialNumberMesg(ANT_MESSAGE *pstTxMessage)
{
   pstTxMessage->ANT_MESSAGE_ucSize   = MESG_GET_SERIAL_NUM_SIZE;
   pstTxMessage->ANT_MESSAGE_ucMesgID = MESG_GET_SERIAL_NUM_ID;
   System_GetSerialNumber(pstTxMessage->ANT_MESSAGE_ucChannel, pstTxMessage->ANT_MESSAGE_aucMesgData);
}

/**
 * @brief Returns ESN.
 */
void System_GetSerialNumber(uint8_t ucID, uint8_t *pucESN)
{
   uint32_t dev_id;
   #if  defined(D52Q_PREMIUM_MODULE) || defined(D52M_PREMIUM_MODULE)

      // Data fields with valid content are dependent on the module in use.
      // MFG_ESN and ANT ID are only populated for G.FIT and Premium modules.
      if (ucID == SYSTEM_ID_MFG_ESN)
      {
         // Retrieve the production ESN
         dev_id = NRF_UICR->CUSTOMER[SYSTEM_UICR_CUST_MFG_ESN_OFFSET];
      }
      else if (ucID == SYSTEM_ID_ANT_ID)
      {
         // Retrieve unique and sequential ANT ID
         dev_id = NRF_UICR->CUSTOMER[SYSTEM_UICR_CUST_ANT_ID_OFFSET];
      }
      else
   #endif
      {
         // Retrieve lower 32-bits of 64-bit Nordic device ID stored in FICR
         dev_id = NRF_FICR->DEVICEID[0];
      }

   //construct ESN to little endian 32-bit number
   pucESN[0] = dev_id >>  0 & 0x000000FF;
   pucESN[1] = dev_id >>  8 & 0x000000FF;
   pucESN[2] = dev_id >> 16 & 0x000000FF;
   pucESN[3] = dev_id >> 24 & 0x000000FF;
}

/**
 * @brief Writes specified ID number to specified location in UICR
 */
uint8_t System_SetSerialNum(ANT_MESSAGE *pstRxMessage)
{

   // Figure out the location we are interested in
   uint8_t ucUICRIndex = 0;

   // G.FIT and Premium modules have ANT ID assigned during
   // manufacturing. See Application Note D00001706 -
   // G.FIT and Premium Module Manufacturing Considerations.
   if (pstRxMessage->ANT_MESSAGE_aucPayload[0] == SYSTEM_ID_ANT_ID)
   {
      ucUICRIndex = SYSTEM_UICR_CUST_ANT_ID_OFFSET;
   }
   else
   {
      // Unknown location
      return INVALID_PARAMETER_PROVIDED;
   }

   // Check if the location is empty
   if (SYSTEM_UICR_CUST_STRUCT->USER_CFG[ucUICRIndex] == 0xFFFFFFFF)
   {
      // Form ESN to write
      uint32_t esn;
      esn  = (pstRxMessage->ANT_MESSAGE_aucPayload[1] <<  0) & 0x000000FF;
      esn |= (pstRxMessage->ANT_MESSAGE_aucPayload[2] <<  8) & 0x0000FF00;
      esn |= (pstRxMessage->ANT_MESSAGE_aucPayload[3] << 16) & 0x00FF0000;
      esn |= (pstRxMessage->ANT_MESSAGE_aucPayload[4] << 24) & 0xFF000000;

      if (System_FlashWrite((uint32_t*)&SYSTEM_UICR_CUST_STRUCT->USER_CFG[ucUICRIndex], &esn, 1) == NRF_SUCCESS)
      {
            // Wait for write to finish
            while(System_FlashBusy());
      }
   }

   return RESPONSE_NO_ERROR;
}
#endif // !SERIAL_NUMBER_NOT_AVAILABLE

/**
 * @brief Writes RSSI calibration byte to the UICR
 */
uint8_t System_SetRSSICal(ANT_MESSAGE *pstRxMessage)
{
   // Check if the location is empty
   if (SYSTEM_UICR_CUST_STRUCT->USER_CFG[SYSTEM_UICR_CUST_RSSI_CAL_OFFSET] == 0xFFFFFFFF)
   {
      // Write only the LSB of the UICR location, keep rest of bits unset
      uint32_t ulCal  = pstRxMessage->ANT_MESSAGE_aucPayload[0] | 0xFFFFFF00;

      if (System_FlashWrite((uint32_t*)&SYSTEM_UICR_CUST_STRUCT->USER_CFG[SYSTEM_UICR_CUST_RSSI_CAL_OFFSET], &ulCal, 1) == NRF_SUCCESS)
      {
            // Wait for write to finish
            while(System_FlashBusy());
      }
   }

   return RESPONSE_NO_ERROR;
}

/**
 * @brief Constructs RSSI calibration message and calls function to retrieve calibration data.
 */
void System_GetRSSICalDataMesg(ANT_MESSAGE *pstTxMessage)
{
   pstTxMessage->ANT_MESSAGE_ucSize    = MESG_RSSI_CAL_SIZE;
   System_GetRSSICalData(pstTxMessage->ANT_MESSAGE_aucPayload);
}

/**
 * @brief Retrieves RSSI calibration data from UICR
 */
void System_GetRSSICalData(uint8_t *pucRSSICal)
{
   // Read value from UICR
   uint32_t ulCalData = NRF_UICR->CUSTOMER[SYSTEM_UICR_CUST_RSSI_CAL_OFFSET];

   // Construct cal data message in little Endian format
   // Only the LSB is used for cal data
   pucRSSICal[0] = ulCalData & 0x000000FF;
}

/**
 * @brief Starts the SD flash write call and sets up the Flash Busy flag
 */
uint32_t System_FlashWrite(uint32_t* ulAddress, uint32_t * pulData, uint8_t ucLength)
{
   uint32_t ulErrCode = sd_flash_write(ulAddress , pulData, ucLength);
   if (ulErrCode == NRF_SUCCESS)
   {
      bFlashBusy = true;
   }
   return ulErrCode;
}

/**
 * @brief Monitors events for flash operations and updates the flag
 */
void System_FlashEventProcess(uint32_t ulEvent)
{
   if ((ulEvent == NRF_EVT_FLASH_OPERATION_SUCCESS) || (ulEvent == NRF_EVT_FLASH_OPERATION_ERROR))
   {
      bFlashBusy = false;
   }
}

/**
 * @brief Reports whether flash write is in progress
 */
bool System_FlashBusy(void)
{
   return bFlashBusy;
}

/**
 * @brief Read the given address to empty the system bus write buffer
 */
void System_WriteBufferEmpty(const volatile uint32_t * pulAddressToRead)
{
  volatile uint32_t temp = *pulAddressToRead;
  (void)temp;
}

void System_TimerRequest(void)
{
   if (ulTimerRequests == 0)
   {
      SYS_TIME_RTC->TASKS_START = 1;
      nrf_delay_us(SYS_TIME_RTC_TASK_LATENCY_US);
   }
   ulTimerRequests += 1;
}

void System_TimerRelease(void)
{
   switch (ulTimerRequests)
   {
      case 0:
         // Mismatched requests.
         break;
      case 1:
         // Releasing last request.
         SYS_TIME_RTC->TASKS_STOP = 1;
         nrf_delay_us(SYS_TIME_RTC_TASK_LATENCY_US);
         // FALL-THROUGH
      default:
         ulTimerRequests -= 1;
   }
}

uint32_t System_GetTime_32K(void)
{
   uint32_t result = SYS_TIME_RTC->COUNTER;

   // No critical section here because the overflow count update always occurs
   // at thread priority, and a critical section is used there to keep the
   // update atomic.

   // Need to take care for the case where overflow was set after we read the
   // counter above.
   if (result < SYS_TIME_RTC_HALF && SYS_TIME_RTC->EVENTS_OVRFLW)
   {
      result += SYS_TIME_RTC_OVRFLW;
   }

   result += ulSysTimeOffset;

   return result;
}

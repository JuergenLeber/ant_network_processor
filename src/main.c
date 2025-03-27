/*
This software is subject to the license described in the LICENSE_A+SS.txt file
included with this software distribution. You may not use this file except in compliance
with this license.

Copyright (c) Garmin Canada Inc. 2019
All rights reserved.
*/

#include "main.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "nordic_common.h"
#include "nrf.h"
#include "nrf_error.h"
#include "nrf_soc.h"
#include "nrf_sdm.h"
#include "nrf_nvic.h"

#include "ant_interface.h"
#include "ant_parameters.h"
#include "ant_error.h"
#include "app_error.h"
#include "appconfig.h"
#include "boardconfig.h"
#include "command.h"
#include "event_buffering.h"
#include "global.h"
#include "serial.h"
#include "system.h"



#include "app_util_platform.h"

uint32_t ulErrorCode;
bool bAllowSleep = 0;
bool bAllowSerialSleep = 0;
ant_event_t stResponse;
bool bResponsePending;
ant_event_t stSdEvent;
volatile bool bStallStackEvents;
ANT_MESSAGE *pstRxMessage;
ANT_MESSAGE *pstTxMessage;
uint8_t aucCapabilities[12]; // require 8, 4 extra padding just in case for future
nrf_nvic_state_t nrf_nvic_state;

extern uint16_t usEventFilterMask;

/**
 * @brief Handler for application asserts
 */
void assert_nrf_callback(uint16_t usLineNum, const uint8_t *pucFileName)
{
   // Copying parameters to static variables because parameters are not accessible in debugger
   static volatile uint8_t  ucDebugFileName[32];
   static volatile uint16_t usDebugLineNum;

   strcpy((char *)ucDebugFileName, (const char *)pucFileName);
   usDebugLineNum = usLineNum;
   (void)ucDebugFileName;
   (void)usDebugLineNum;

#if defined (RESET_ON_ASSERT_AND_FAULTS)
   NVIC_SystemReset();
#else
    while(1); // loop for debugging
#endif // RESET_ON_ASSERT_AND_FAULTS
}

/**
 * @brief Handler for softdevice asserts
 */
void softdevice_assert_callback( uint32_t ulId, uint32_t ulPC, uint32_t ulInfo )
{
   static volatile uint32_t id;
   static volatile uint32_t pc;
   static volatile uint32_t info;

   id = ulId;
   pc = ulPC;
   info = ulInfo;

   while(1);
}

/**
 * @brief Application error handler function
 */
void app_error_handler(uint32_t ulErrorCode, uint32_t ulLineNum, const uint8_t * pucFileName)
{
   // Copying parameters to static variables because parameters are not accessible in debugger.
   static volatile uint8_t  ucDebugFileName[32];
   static volatile uint16_t usDebugLineNum;
   static volatile uint32_t ulDebugErrorCode;

   strcpy((char *)ucDebugFileName, (const char *)pucFileName);
   usDebugLineNum   = ulLineNum;
   ulDebugErrorCode = ulErrorCode;
   (void)ucDebugFileName;
   (void)usDebugLineNum;
   (void)ulDebugErrorCode;

#if defined (RESET_ON_ASSERT_AND_FAULTS)
   NVIC_SystemReset();
#else
    while(1); // loop for debugging
#endif // RESET_ON_ASSERT_AND_FAULTS
}

/**
 * @brief Handler for hard faults
 */
void HardFault_Handler(uint32_t ulProgramCounter, uint32_t ulLinkRegister)
{
   (void)ulProgramCounter;
   (void)ulLinkRegister;

#if defined (RESET_ON_ASSERT_AND_FAULTS)
   NVIC_SystemReset();
#else
    while(1); // loop for debugging
#endif // RESET_ON_ASSERT_AND_FAULTS
}

/**
 * @brief Handler for SWI0 interrupts
 */
void SWI0_IRQHandler(void)
{
   // unused
}

/**
 * @brief Handler for radio notification interrupts (SWI1)
 */
void RADIO_NOTIFICATION_IRQHandler(void)
{
   // unused
}

/**
 * @brief Handler for protocol events & SOC event interrupts (SWI2)
 */
void SD_EVT_IRQHandler(void)
{
   uint32_t ulEvent;

   while (sd_evt_get(&ulEvent) == NRF_SUCCESS) // read out SOC events
   {
   }

   while (!bStallStackEvents && sd_ant_event_get(
      &stSdEvent.stHeader.ucChannel,
      &stSdEvent.stHeader.ucEvent,
      stSdEvent.stMessage.aucMessage) == NRF_SUCCESS)
   {
      bEventANTProcessStart = 1; // start ANT event handler to check if there are any ANT events
      if (!event_buffering_put(&stSdEvent))
      {
         bStallStackEvents = 1;
      }
   }
}

/**
 * @brief Handler for UART0 interrupts
 */
#if defined(SERIAL_USE_UARTE)
void UARTE0_UART0_IRQHandler(void)
#else
void UART0_IRQHandler(void)
#endif
{
   Serial_UART0_IRQHandler();
}

/**
 * @brief Handler for GPIOTE interrupts
 */
void GPIOTE_IRQHandler(void)
{
   Serial_GPIOTE_IRQHandler();
}

/**
 * @brief Set application queued burst mode
 */
void Main_SetQueuedBurst(void)
{
   bEventBurstMessageProcess = 1; // ANT burst message to process
}

/**
 * @brief Main
 */
int main()
{

   #if defined(XIAO_NRF52840)
   //Initialize LED GPIOs and start with red only until SoftDevice is started 
   NRF_GPIO->PIN_CNF[LED_RED_PIN] = (GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos) |
                                    (GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos) |
                                    (GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos) |
                                    (GPIO_PIN_CNF_DRIVE_S0S1 << GPIO_PIN_CNF_DRIVE_Pos) |
                                    (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos);
   NRF_GPIO->PIN_CNF[LED_GREEN_PIN] = (GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos) |
                                      (GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos) |
                                      (GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos) |
                                      (GPIO_PIN_CNF_DRIVE_S0S1 << GPIO_PIN_CNF_DRIVE_Pos) |
                                      (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos);
   NRF_GPIO->PIN_CNF[LED_BLUE_PIN] = (GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos) |
                                     (GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos) |
                                     (GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos) |
                                     (GPIO_PIN_CNF_DRIVE_S0S1 << GPIO_PIN_CNF_DRIVE_Pos) |
                                     (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos);
   NRF_GPIO->OUTSET = (1UL << LED_BLUE_PIN);
   NRF_GPIO->OUTCLR = (1UL << LED_RED_PIN);
   NRF_GPIO->OUTSET = (1UL << LED_GREEN_PIN);
   #endif	

   nrf_clock_lf_cfg_t clock_source;

   // Initialize nrf_nvic_state to 0
   memset(&nrf_nvic_state, 0, sizeof(nrf_nvic_state));


   /*** scatter file loading done by sd_softdevice_enable must be done first before any RAM access ***/

   //Set up clock structure
   clock_source.source = NRF_CLOCK_LF_SRC_XTAL;
   clock_source.rc_ctiv = 0; //Must be 0 for XTAL
   clock_source.rc_temp_ctiv = 0; //Must be 0 for XTAL
   clock_source.accuracy = NRF_CLOCK_LF_ACCURACY_50_PPM;

   ulErrorCode = sd_softdevice_enable(&clock_source, softdevice_assert_callback, ANT_LICENSE_KEY);

   #if defined(XIAO_NRF52840)
   //After SoftDevice is enabled switch to red+blue 
   NRF_GPIO->OUTCLR = (1UL << LED_BLUE_PIN);
   NRF_GPIO->OUTCLR = (1UL << LED_RED_PIN);
   NRF_GPIO->OUTSET = (1UL << LED_GREEN_PIN);
   #endif	

   APP_ERROR_CHECK(ulErrorCode);

   // note: IRQ priorities must be set correctly before enabling them!
   ulErrorCode = sd_nvic_SetPriority(SWI0_IRQn, APP_IRQ_PRIORITY_LOWEST);
   APP_ERROR_CHECK(ulErrorCode);

   ulErrorCode = sd_nvic_SetPriority(RADIO_NOTIFICATION_IRQn, APP_IRQ_PRIORITY_MID); // SW1_IRQn
   APP_ERROR_CHECK(ulErrorCode);

   ulErrorCode = sd_nvic_SetPriority(SD_EVT_IRQn, APP_IRQ_PRIORITY_LOWEST); // SW2_IRQn
   APP_ERROR_CHECK(ulErrorCode);

   ulErrorCode = sd_nvic_EnableIRQ(SWI0_IRQn);
   APP_ERROR_CHECK(ulErrorCode);

   ulErrorCode = sd_nvic_EnableIRQ(RADIO_NOTIFICATION_IRQn); // SW1_IRQn
   APP_ERROR_CHECK(ulErrorCode);

   ulErrorCode = sd_nvic_EnableIRQ(SD_EVT_IRQn); // SW2_IRQn
   APP_ERROR_CHECK(ulErrorCode);

#if defined (SCALABLE_CHANNELS_DEFAULT)
   // Initialize ANT Channels to default
   stANTChannelEnable.ucTotalNumberOfChannels = ANT_STACK_TOTAL_CHANNELS_ALLOCATED_DEFAULT;
   stANTChannelEnable.ucNumberOfEncryptedChannels = ANT_STACK_ENCRYPTED_CHANNELS_DEFAULT;
   stANTChannelEnable.pucMemoryBlockStartLocation = aucANTChannelBlock;
   stANTChannelEnable.usMemoryBlockByteSize = ANT_ENABLE_GET_REQUIRED_SPACE(ANT_STACK_TOTAL_CHANNELS_ALLOCATED_DEFAULT, ANT_STACK_ENCRYPTED_CHANNELS_DEFAULT, ANT_STACK_TX_BURST_QUEUE_SIZE_DEFAULT, ANT_STACK_EVENT_QUEUE_NUM_EVENTS_DEFAULT);
   ulErrorCode = sd_ant_enable(&stANTChannelEnable);
   APP_ERROR_CHECK(ulErrorCode);
#endif // SCALABLE_CHANNELS_DEFAULT

   ulErrorCode = sd_ant_capabilities_get(aucCapabilities); // get num channels supported by stack
   APP_ERROR_CHECK(ulErrorCode);
   stANTChannelEnable.ucTotalNumberOfChannels = aucCapabilities[0];

   bEventRXSerialMessageProcess = 0;
   bEventANTProcessStart = 0;
   bEventANTProcess = 0;
   bEventBurstMessageProcess = 0;
   bEventStartupMessage = 0;
   bEventSetBaudrate = 0;
   ucQueuedTxBurstChannel = 0xFF; // invalid channel number
   ucBurstSequence = 0;
   bResponsePending = 0;
   bStallStackEvents = 0;

   System_Init();
   event_buffering_init();

   #if defined(XIAO_NRF52840)
   //After System init switch to blue 
   NRF_GPIO->OUTCLR = (1UL << LED_BLUE_PIN);
   NRF_GPIO->OUTSET = (1UL << LED_RED_PIN);
   NRF_GPIO->OUTSET = (1UL << LED_GREEN_PIN);
   #endif	

   Serial_Init();

   #if defined(XIAO_NRF52840)
   //After Serial init switch to blue+green 
   NRF_GPIO->OUTCLR = (1UL << LED_BLUE_PIN);
   NRF_GPIO->OUTSET = (1UL << LED_RED_PIN);
   NRF_GPIO->OUTCLR = (1UL << LED_GREEN_PIN);
   #endif	

   pstRxMessage = Serial_GetRxMesgPtr();
   pstTxMessage = Serial_GetTxMesgPtr();
#if defined (SERIAL_REPORT_RESET_MESSAGE)
   bEventStartupMessage = 1;
   System_ResetMesg((ANT_MESSAGE *)pstTxMessage); // send reset message upon system startup
#endif // !SERIAL_REPORT_RESET_MESSAGE

   // reset power reset reason after constructing startup message
   ulErrorCode = sd_power_reset_reason_clr(POWER_RESETREAS_OFF_Msk | POWER_RESETREAS_LOCKUP_Msk | POWER_RESETREAS_SREQ_Msk | POWER_RESETREAS_DOG_Msk | POWER_RESETREAS_RESETPIN_Msk); // clear reset reasons
   APP_ERROR_CHECK(ulErrorCode);

   #if defined(XIAO_NRF52840)
   //Everything done, switch to green 
   NRF_GPIO->OUTSET = (1UL << LED_BLUE_PIN);
   NRF_GPIO->OUTSET = (1UL << LED_RED_PIN);
   NRF_GPIO->OUTCLR = (1UL << LED_GREEN_PIN);
   #endif	


   // loop forever
   while (1)
   {
      bAllowSleep = 1;

      bAllowSerialSleep = Serial_RxMessage(); // poll for receive, check if serial interface can sleep

      if(bEventSetBaudrate)
      {
          bEventSetBaudrate = 0; // clear set baudrate event flag
          Serial_ActivateAsyncBaudrate();
      }
      else if (bEventStartupMessage) // send out startup serial message
      {
         bEventStartupMessage = 0;

         bAllowSleep = 0;
      }
      else if (bEventRXSerialMessageProcess && !bResponsePending) // rx serial message to handle
      {
         bEventRXSerialMessageProcess = 0; // clear the RX event flag
         Command_SerialMessageProcess((ANT_MESSAGE *)pstRxMessage, &stResponse.stMessage); // send to command handler
         bResponsePending = 1;
         bAllowSleep = 0;
      }
      else if (bEventANTProcessStart || bEventANTProcess) // protocol event message to handle
      {
         ant_event_hdr_t stHeader;

         if (!bEventANTProcess)
         {
            bEventANTProcessStart = 0;
            bEventANTProcess = 1;
         }

         if (event_buffering_get(&stHeader, (ANT_MESSAGE*)pstTxMessage)) // received event, send to event handlers
         {
            uint8_t ucEventType = stHeader.ucEvent;

            Serial_ANTEventHandler(ucEventType, pstTxMessage);

            if (((uint16_t)(1 << (ucEventType - 1))) & usEventFilterMask)
            {
               // Do not send the message through serial interface
               pstTxMessage->ANT_MESSAGE_ucSize = 0;
            }
         }
         else // no event
         {
            bEventANTProcess = 0;
         }

         bAllowSleep = 0;
      }
      else if (bEventBurstMessageProcess && !bResponsePending) // we have a burst message to process
      {
         bEventBurstMessageProcess = 0; // clear the burst event flag
         if (Command_BurstMessageProcess((ANT_MESSAGE *)pstRxMessage, &stResponse.stMessage)) // try to process the burst transfer message
         {
            ucQueuedTxBurstChannel = ((ANT_MESSAGE *)pstRxMessage)->ANT_MESSAGE_ucChannel & CHANNEL_NUMBER_MASK; // indicate queued burst transfer process with channel number
         }

         bAllowSleep = 0;
         bResponsePending = 1;
      }

      // Queue any generated response.
      if (bResponsePending)
      {
         stResponse.stHeader.ucChannel = stResponse.stMessage.ANT_MESSAGE_ucChannel;
         stResponse.stHeader.ucEvent = NO_EVENT;
         if (stResponse.stMessage.ANT_MESSAGE_ucSize == 0 ||
            event_buffering_put(&stResponse))
         {
            bResponsePending = false;
         }
         event_buffering_flush();

         // Need to make sure we run the service to pull the message out of the queue later.
         bEventANTProcess = 1;
         bAllowSleep = 0;
      }

      // Check if stack events need to be unblocked. This is done here so that
      // command responses get first go when the fifo fills up.
      if (bStallStackEvents && event_buffering_put(&stSdEvent))
      {
         // Allow interrupt to start pushing more events.
         bStallStackEvents = 0;
         // Make sure to flush any that were already ignored.
         sd_nvic_SetPendingIRQ(SD_EVT_IRQn);
      }

      System_Tick();
      Serial_TxMessage(); // transmit any pending tx messages, goes to sleep if it can

      if (bAllowSleep) // if sleep is allowed
      {
         if (bAllowSerialSleep)
            Serial_Sleep(); // serial interface sleep

         System_DeepSleep(); // goto deep sleep/system off if required

         (void)sd_app_evt_wait(); // wait for event low power mode
      }
   }
}

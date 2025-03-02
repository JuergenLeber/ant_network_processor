/*
This software is subject to the license described in the LICENSE_A+SS.txt file
included with this software distribution. You may not use this file except in compliance
with this license.

Copyright (c) Garmin Canada Inc. 2019
All rights reserved.
*/

#include "serial.h"

#include <stdbool.h>
#include <stdint.h>
#include "nrf.h"
#include "nrf_soc.h"
#include "nrf_sdm.h"
#include "nrf_nvic.h"

#include "ant_interface.h"
#include "ant_parameters.h"
#include "appconfig.h"
#include "boardconfig.h"
#include "global.h"
#include "serial.h"
#include "system.h"


#include "app_util_platform.h"

/*NOTES:
 * This serial.c is specifically designed for nrf52-ANT Serial Async/Sync Implementation.
 * please refer to Interfacing with ANT General Purpose Chipsets and Modules Rev 2.1.pdf
 *
 * Few differences to the specification above.
 * 1. Synchronous bit flow controlled is not supported
 * 2. Asynchronous RTS is hardware flow controlled. RTS flow control is byte level instead of message level
 *
 * NOTE: PERIPHERALS USED
 * 1. SPI0  (for Synchronous Serial)
 * 2. UART0 or UARTE0 (for Asynchronous Serial)
 * 3. RTC1  (for periodic low power wake-up poll)
 *
 * UARTE0 with EasyDMA is enabled via preprocessor definition SERIAL_USE_UARTE
 */

/***************************************************************************
 * GENERIC REGISTERS
 ***************************************************************************/

#define NUMBER_OF_NRF_GPIO_PINS              (32)

/***************************************************************************
 * NRF SYNCHRONOUS SPI PERIPHERAL ACCESS DEFINITIONS
 ***************************************************************************/
#define SERIAL_SYNC                          NRF_SPI0 //using NRF SPI Master 0

#define SERIAL_SYNC_FREQUENCY                SPI_FREQUENCY_FREQUENCY_M4
#define SERIAL_SYNC_FREQUENCY_SLOW           SPI_FREQUENCY_FREQUENCY_K500
#define SERIAL_SYNC_SERIAL_ENABLE()          (SERIAL_SYNC->ENABLE |= (SPI_ENABLE_ENABLE_Enabled << SPI_ENABLE_ENABLE_Pos)) // SPI Peripheral Enable\Disable
#define SERIAL_SYNC_SERIAL_DISABLE()         (SERIAL_SYNC->ENABLE &= ~(SPI_ENABLE_ENABLE_Enabled << SPI_ENABLE_ENABLE_Pos))
#define SYNC_SEN_ASSERT()                    NRF_GPIO->OUT &= ~(1UL << SERIAL_SYNC_PIN_SEN)
#define SYNC_SEN_DEASSERT()                  NRF_GPIO->OUT |= (1UL << SERIAL_SYNC_PIN_SEN)
#define IS_MRDY_ASSERTED()                   (!((NRF_GPIO->IN >> SERIAL_SYNC_PIN_SMSGRDY) & 1UL)) // true when asserted, is active low
#define IS_SRDY_ASSERTED()                   (!((NRF_GPIO->IN >> SERIAL_SYNC_PIN_SRDY) & 1UL)) // true when asserted, is active low
//#define IS_BIT_SYNC()                      ((NRF_GPIO->IN >>SERIAL_SYNC_PIN_SFLOW) & 1UL) // high is bit sync on. Bit sync mode unsupported
#if defined (SERIAL_PIN_PORTSEL)
   #define SERIAL_PORT_SEL                      (NRF_GPIO->IN & (0x1UL << SERIAL_PIN_PORTSEL)) // reading serial PORTSEL PIN
#endif

#if defined (SYNCHRONOUS_DISABLE)
   #define IS_SYNC_MODE()                    false
#elif defined (ASYNCHRONOUS_DISABLE)
   #define IS_SYNC_MODE()                    true
#else
   #define IS_SYNC_MODE()                    (SERIAL_PORT_SEL) // sync is PORTSEL high
#endif //SYNC ASYNC

#define EVENT_PIN_SMSGRDY                    SERIAL_SYNC_GPIOTE_EVENT_SMSGRDY // GPIO Task and Event number for SMSGRDY Pin
#define EVENT_PIN_SRDY                       SERIAL_SYNC_GPIOTE_EVENT_SRDY // GPIO Task and Event number for SRDY Pin
#define IS_INT_PIN_SMSGRDY_ASSERTED()        (NRF_GPIOTE->EVENTS_IN[EVENT_PIN_SMSGRDY] && (NRF_GPIOTE->INTENSET & (1UL << EVENT_PIN_SMSGRDY)))
#define IS_INT_PIN_SRDY_ASSERTED()           (NRF_GPIOTE->EVENTS_IN[EVENT_PIN_SRDY] && (NRF_GPIOTE->INTENSET & (1UL << EVENT_PIN_SRDY)))

#define INT_PIN_SMSGRDY_ENABLE()             NRF_GPIOTE->INTENSET = 1UL << EVENT_PIN_SMSGRDY;
#define INT_PIN_SMSGRDY_DISABLE()            NRF_GPIOTE->INTENCLR = 1UL << EVENT_PIN_SMSGRDY;
#define INT_PIN_SMSGRDY_FLAG_CLEAR()         NRF_GPIOTE->EVENTS_IN[EVENT_PIN_SMSGRDY] = 0
#define INT_PIN_SRDY_ENABLE()                NRF_GPIOTE->INTENSET = 1UL << EVENT_PIN_SRDY;
#define INT_PIN_SRDY_DISABLE()               NRF_GPIOTE->INTENCLR = 1UL << EVENT_PIN_SRDY;
#define INT_PIN_SRDY_FLAG_CLEAR()            NRF_GPIOTE->EVENTS_IN[EVENT_PIN_SRDY] = 0

#define SYNC_SRDY_SLEEP_DELAY_STEPS          10 // 10us
#define SYNC_SRDY_SLEEP_DELAY_US             (5 * SYNC_SRDY_SLEEP_DELAY_STEPS) // 50us delay wait on SRDY before going to sleep

/***************************************************************************
 * NRF ASYNCHRONOUS UART PERIPHERAL ACCESS DEFINITIONS
 ***************************************************************************/
#if defined(SERIAL_USE_UARTE)
   #define SERIAL_ASYNC                         NRF_UARTE0   //using NRF UARTE 0
   /*** Interrupt Control ***/
   #define SERIAL_ASYNC_INT_ENDTX_ENABLE()      SERIAL_ASYNC->INTENSET = (UARTE_INTENSET_ENDTX_Set << UARTE_INTENSET_ENDTX_Pos) // enable Tx End Interrupt
   #define SERIAL_ASYNC_INT_ENDTX_DISABLE()     SERIAL_ASYNC->INTENCLR = (UARTE_INTENCLR_ENDTX_Clear << UARTE_INTENCLR_ENDTX_Pos)// disable Tx End Interrupt
   #define SERIAL_ASYNC_INT_ENDRX_ENABLE()      SERIAL_ASYNC->INTENSET = (UARTE_INTENSET_ENDRX_Set << UARTE_INTENSET_ENDRX_Pos) // enable Rx End Interrupt
   #define SERIAL_ASYNC_INT_ENDRX_DISABLE()     SERIAL_ASYNC->INTENCLR = (UARTE_INTENCLR_ENDRX_Clear << UARTE_INTENCLR_ENDRX_Pos)// disable Rx End Interrupt
   /*** RTS Line Control ***/
   #define SERIAL_ASYNC_RTS_ENABLE()            {SERIAL_ASYNC_INT_ENDRX_ENABLE(); SERIAL_ASYNC->PSEL.RTS = SERIAL_ASYNC_PIN_RTS;} // assign async RTS to HW control, let HW decide state
   #define SERIAL_ASYNC_RTS_DISABLE()           {SERIAL_ASYNC_INT_ENDRX_DISABLE(); SERIAL_ASYNC->PSEL.RTS = 0xFFFFFFFF; NRF_GPIO->OUT |= (1UL << SERIAL_ASYNC_PIN_RTS);} // unassign async RTS from HW control and force it high
   /*** UARTE Peripheral Enable\Disable ***/
   #define SERIAL_ASYNC_SERIAL_ENABLE()         SERIAL_ASYNC->ENABLE = UARTE_ENABLE_ENABLE_Enabled << UARTE_ENABLE_ENABLE_Pos
   #define SERIAL_ASYNC_SERIAL_DISABLE()        SERIAL_ASYNC->ENABLE = UARTE_ENABLE_ENABLE_Disabled << UARTE_ENABLE_ENABLE_Pos
   /*** UARTE Receive Restart ***/
   #define SERIAL_ASYNC_RX_RESTART()            {SERIAL_ASYNC->EVENTS_ENDRX = 0; SERIAL_ASYNC->TASKS_STOPRX = 1; SERIAL_ASYNC->TASKS_STARTRX = 1;}
#else
   #define SERIAL_ASYNC                         NRF_UART0   //using NRF UART 0
   /*** Interrupt Control ***/
   #define SERIAL_ASYNC_INT_TXRDY_ENABLE()      SERIAL_ASYNC->INTENSET = (UART_INTENSET_TXDRDY_Set << UART_INTENSET_TXDRDY_Pos) // enable Tx Ready Interrupt
   #define SERIAL_ASYNC_INT_TXRDY_DISABLE()     SERIAL_ASYNC->INTENCLR = (UART_INTENCLR_TXDRDY_Clear << UART_INTENCLR_TXDRDY_Pos)// disable Tx Ready Interrupt
   #define SERIAL_ASYNC_INT_RXRDY_ENABLE()      SERIAL_ASYNC->INTENSET = (UART_INTENSET_RXDRDY_Set << UART_INTENSET_RXDRDY_Pos) // enable Rx Ready Interrupt
   #define SERIAL_ASYNC_INT_RXRDY_DISABLE()     SERIAL_ASYNC->INTENCLR = (UART_INTENCLR_RXDRDY_Clear << UART_INTENCLR_RXDRDY_Pos)// disable Rx Ready Interrupt
   /*** RTS Line Control ***/
   #define SERIAL_ASYNC_RTS_ENABLE()            {SERIAL_ASYNC_INT_RXRDY_ENABLE(); SERIAL_ASYNC->PSELRTS = SERIAL_ASYNC_PIN_RTS;} // assign async RTS to HW control, let HW decide state
   #define SERIAL_ASYNC_RTS_DISABLE()           {SERIAL_ASYNC_INT_RXRDY_DISABLE(); SERIAL_ASYNC->PSELRTS = 0xFFFFFFFF; NRF_GPIO->OUT |= (1UL << SERIAL_ASYNC_PIN_RTS);} // unassign async RTS from HW control and force it high
   /*** UART Peripheral Enable\Disable ***/
   #define SERIAL_ASYNC_SERIAL_ENABLE()         SERIAL_ASYNC->ENABLE = UART_ENABLE_ENABLE_Enabled << UART_ENABLE_ENABLE_Pos
   #define SERIAL_ASYNC_SERIAL_DISABLE()        SERIAL_ASYNC->ENABLE = UART_ENABLE_ENABLE_Disabled << UART_ENABLE_ENABLE_Pos
#endif
/*** Start and Stop Tx Task ***/
#define SERIAL_ASYNC_START_TX()              {SERIAL_ASYNC->TASKS_STARTTX = 1; bTransmitting = true;}
#define SERIAL_ASYNC_STOP_TX()               {SERIAL_ASYNC->TASKS_STOPTX = 1; bTransmitting = false;}
/*** Fixed Baudrate value ***/
#if defined(SERIAL_USE_UARTE)
   #define SERIAL_ASYNC_BAUDRATE_DEFAULT_VALUE  UARTE_BAUDRATE_BAUDRATE_Baud57600
#else
   #define SERIAL_ASYNC_BAUDRATE_DEFAULT_VALUE  UART_BAUDRATE_BAUDRATE_Baud57600
#endif

#if defined(PWRSAVE_DISABLE)
   #define IS_PIN_SLEEP_ASSERTED()              false
   #define IS_PIN_SUSPEND_ASSERTED()            false
#else
   #define IS_PIN_SLEEP_ASSERTED()              (NRF_GPIO->IN & (0x1UL << SERIAL_ASYNC_PIN_SLEEP))
   #define IS_PIN_SUSPEND_ASSERTED()            (!((NRF_GPIO->IN & (0x1UL << SERIAL_ASYNC_PIN_SUSPEND)))) // true when asserted, is active low
#endif // PWRSAVE_DISABLE

/***************************************************************************
 * Local Variables
 ***************************************************************************/
/*
 * Serial control flags: Must be accessed/changed atomically as they can be accessed by multiple contexts
 * If bitfields are used, the entire bitfield operation must be atomic!!
 */
static volatile bool bStartMessage;
static volatile bool bEndMessage;
static volatile bool bTransmitting;
static volatile bool bHold;
static volatile bool bSyncMode;
static volatile bool bSleep;
static volatile bool bSRdy;

/**/
#define PIN_SENSE_DISABLED    0
#define PIN_SENSE_LOW         1
#define PIN_SENSE_HIGH        2
#if !defined(SYNCHRONOUS_DISABLE)
   static uint8_t ucPinSenseSRDY = PIN_SENSE_DISABLED;
   static uint8_t ucPinSenseMRDY = PIN_SENSE_DISABLED;
   static uint16_t usSyncSRdySleepDelay;
#endif // SYNCHRONOUS_DISABLE
/*
 * This table is a lookup for the baud rate control registers,
 * based on the pre-established baud rates we support.
 */

#if !defined (ASYNCHRONOUS_DISABLE)
   #if defined(SERIAL_BAUDRATE_DEFAULT_NDX)
      uint8_t ucBaudrateNdx = SERIAL_BAUDRATE_DEFAULT_NDX; // default Asynch baudrate
   #else
      uint8_t ucBaudrateNdx;
   #endif

   #if defined(SERIAL_USE_UARTE)
      static const uint32_t asBaudControl[8] =
      {
         UARTE_BAUDRATE_BAUDRATE_Baud4800,    // 4800 baud.
         UARTE_BAUDRATE_BAUDRATE_Baud38400,   // 38400 baud
         UARTE_BAUDRATE_BAUDRATE_Baud19200,   // 19200 baud.
         UARTE_BAUDRATE_BAUDRATE_Baud57600,   // 57600 baud.  50000 on some ANT modules. Spare value, may be updated later.
         UARTE_BAUDRATE_BAUDRATE_Baud115200,  // 115200 baud.
         UARTE_BAUDRATE_BAUDRATE_Baud9600,    // 9600 baud.
         UARTE_BAUDRATE_BAUDRATE_Baud2400,    // 2400 baud.
         UARTE_BAUDRATE_BAUDRATE_Baud57600    // 57600 baud.
      };
   #else
      static const uint32_t asBaudControl[8] =
      {
         UART_BAUDRATE_BAUDRATE_Baud4800,     // 4800 baud.
         UART_BAUDRATE_BAUDRATE_Baud38400,    // 38400 baud
         UART_BAUDRATE_BAUDRATE_Baud19200,    // 19200 baud.
         UART_BAUDRATE_BAUDRATE_Baud57600,    // 57600 baud.  50000 on some ANT modules. Spare value, may be updated later.
         UART_BAUDRATE_BAUDRATE_Baud115200,   // 115200 baud.
         UART_BAUDRATE_BAUDRATE_Baud9600,     // 9600 baud.
         UART_BAUDRATE_BAUDRATE_Baud2400,     // 2400 baud.
         UART_BAUDRATE_BAUDRATE_Baud57600     // 57600 baud.
      };
   #endif
#endif

/*
 * Any change to asBaudLookup must be made in accordance with:
 *
 * BAUDRATE_TYPE enum
 * UART_BAUD_SUPPORTED_BITFIELD bitfield
 */

#if !defined (ASYNCHRONOUS_DISABLE)
   #if defined(SERIAL_USE_UARTE)
      static const uint32_t asBaudLookup[12] =
      {
          UARTE_BAUDRATE_BAUDRATE_Baud1200,   // 1200 baud.
          UARTE_BAUDRATE_BAUDRATE_Baud2400,   // 2400 baud.
          UARTE_BAUDRATE_BAUDRATE_Baud4800,   // 4800 baud.
          UARTE_BAUDRATE_BAUDRATE_Baud9600,   // 9600 baud.
          UARTE_BAUDRATE_BAUDRATE_Baud19200,  // 19200 baud.
          UARTE_BAUDRATE_BAUDRATE_Baud38400,  // 38400 baud.
          0,                                  // reserved for 50000 baud.
          UARTE_BAUDRATE_BAUDRATE_Baud57600,  // 57600 baud.
          UARTE_BAUDRATE_BAUDRATE_Baud115200, // 115200 baud.
          UARTE_BAUDRATE_BAUDRATE_Baud230400, // 230400 baud.
          UARTE_BAUDRATE_BAUDRATE_Baud460800, // 460800 baud.
          UARTE_BAUDRATE_BAUDRATE_Baud921600  // 921600 baud.
      };
   #else
      static const uint32_t asBaudLookup[12] =
      {
          UART_BAUDRATE_BAUDRATE_Baud1200,   // 1200 baud.
          UART_BAUDRATE_BAUDRATE_Baud2400,   // 2400 baud.
          UART_BAUDRATE_BAUDRATE_Baud4800,   // 4800 baud.
          UART_BAUDRATE_BAUDRATE_Baud9600,   // 9600 baud.
          UART_BAUDRATE_BAUDRATE_Baud19200,  // 19200 baud.
          UART_BAUDRATE_BAUDRATE_Baud38400,  // 38400 baud.
          0,                                 // reserved for 50000 baud.
          UART_BAUDRATE_BAUDRATE_Baud57600,  // 57600 baud.
          UART_BAUDRATE_BAUDRATE_Baud115200, // 115200 baud.
          UART_BAUDRATE_BAUDRATE_Baud230400, // 230400 baud.
          UART_BAUDRATE_BAUDRATE_Baud460800, // 460800 baud.
          UART_BAUDRATE_BAUDRATE_Baud921600  // 921600 baud.
      };
   #endif
#endif

#if !defined (SYNCHRONOUS_DISABLE)
   #define MAX_BITRATE_NDX    4
   static const uint32_t asBitRateControl[5] =
   {
      SPI_FREQUENCY_FREQUENCY_K500,
      SPI_FREQUENCY_FREQUENCY_M1,
      SPI_FREQUENCY_FREQUENCY_M2,
      SPI_FREQUENCY_FREQUENCY_M4,
      SPI_FREQUENCY_FREQUENCY_M8
   };
#endif

static struct
{
   uint8_t padding[3]; // required to store ucMessageSyncByte and stMessageData sequentially for TX while satisfying alignment
   uint8_t ucMessageSyncByte;
   volatile ANT_MESSAGE stMessageData; // volatile required due to polling on size during transmit
} stTxMessage;
static volatile ANT_MESSAGE stRxMessage;

volatile BAUDRATE_TYPE eBaudSelection;

static uint8_t ucRxPtr;
static uint8_t ucTxPtr;

#if !defined(ASYNCHRONOUS_DISABLE)
   static uint32_t ulPinSenseCfg;
   static uint32_t ulPinSenseEnabled;
#endif // ASYNCHRONOUS_DISABLE

#if defined(SERIAL_USE_UARTE)
   static uint8_t aucRxBufferDMA;  // use a single byte buffer to simulate RXD on the normal UART
#endif

/***************************************************************************
 * Private Functions Protoype
 ***************************************************************************/

static void AsyncProc_TxMessage(void);
static void SyncProc_TxMessage(void);
static void Serial_Wakeup(void);

#if !defined (SYNCHRONOUS_DISABLE)
   static void Gpiote_FallingEdge_Enable (void);
   static void Gpiote_FallingEdge_Disable (void);
   static bool Sync_IsMSGRDYAsserted(void);
   static void SyncProc_RxMessage(void);
#endif //!SYNCHRONOUS_DISABLE

/***************************************************************************
 * Public Functions
 ***************************************************************************/
/**
 * @brief Serial interface initialization
 */
void Serial_Init (void)
{
#if defined (SERIAL_PIN_PORTSEL)
   // Set SERIAL_PIN_PORTSEL as input, This determines what type of Serial to use Async or Sync
   NRF_GPIO->PIN_CNF[SERIAL_PIN_PORTSEL] = (GPIO_PIN_CNF_DIR_Input << GPIO_PIN_CNF_DIR_Pos) |
                                           (GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos) |
                                           (GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos) |
                                           (GPIO_PIN_CNF_DRIVE_S0S1 << GPIO_PIN_CNF_DRIVE_Pos) |
                                           (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos);
   System_WriteBufferEmpty(&NRF_GPIO->PIN_CNF[SERIAL_PIN_PORTSEL]);
#endif

   bStartMessage = 0;
   bEndMessage = 0;
   bTransmitting = 0;
   bHold = 0;
   bSyncMode = 0;
   bSRdy = 0;
   bSleep = 0;


#if !defined (SYNCHRONOUS_DISABLE)
   if (IS_SYNC_MODE()) // check which mode is desired
   {
      bSyncMode = true;

      /* Make sure peripheral is on reset, disable first*/
      /*****************************************************************************************************
       * Initializing SPI related registers
       */

      SERIAL_SYNC_SERIAL_DISABLE();
      sd_nvic_DisableIRQ(SPI0_TWI0_IRQn);
      sd_nvic_ClearPendingIRQ(SPI0_TWI0_IRQn);

      /*** Configuring Support PINS ***/
      /*SMSGRDY Serial Message Ready Pin*/
      NRF_GPIO->PIN_CNF[SERIAL_SYNC_PIN_SMSGRDY] = (GPIO_PIN_CNF_DIR_Input << GPIO_PIN_CNF_DIR_Pos) |
                                                   (GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos) |
                                                   (GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos) |
                                                   (GPIO_PIN_CNF_DRIVE_S0S1 << GPIO_PIN_CNF_DRIVE_Pos) |
                                                   (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos);
      /*SRDY Serial Ready Pin*/
      NRF_GPIO->PIN_CNF[SERIAL_SYNC_PIN_SRDY] = (GPIO_PIN_CNF_DIR_Input << GPIO_PIN_CNF_DIR_Pos) |
                                                (GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos) |
                                                (GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos) |
                                                (GPIO_PIN_CNF_DRIVE_S0S1 << GPIO_PIN_CNF_DRIVE_Pos) |
                                                (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos);

      /*SEN Serial Enable Pin*/
      SYNC_SEN_DEASSERT(); // Make sure it is Deasserted not to confuse the Host.
      NRF_GPIO->PIN_CNF[SERIAL_SYNC_PIN_SEN] = (GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos) |
                                               (GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos) |
                                               (GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos) |
                                               (GPIO_PIN_CNF_DRIVE_S0S1 << GPIO_PIN_CNF_DRIVE_Pos) |
                                               (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos);

      #if defined (SERIAL_SYNC_PIN_SFLOW)
      NRF_GPIO->PIN_CNF[SERIAL_SYNC_PIN_SFLOW] = (GPIO_PIN_CNF_DIR_Input << GPIO_PIN_CNF_DIR_Pos) |
                                                 (GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos) |
                                                 (GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos) |
                                                 (GPIO_PIN_CNF_DRIVE_S0S1 << GPIO_PIN_CNF_DRIVE_Pos) |
                                                 (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos);
      #endif //SERIAL_SYNC_PIN_SFLOW
      /*Configuring Serial SPI Pins*/
   #if !defined (SPI_IDLE_LOW_WORKAROUND)
      NRF_GPIO->OUTSET = (1UL << SERIAL_SYNC_PIN_SCLK);
   #endif
      NRF_GPIO->PIN_CNF[SERIAL_SYNC_PIN_SCLK] = (GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos) |
                                                (GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos) |
                                                (GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos) |
                                                (GPIO_PIN_CNF_DRIVE_S0S1 << GPIO_PIN_CNF_DRIVE_Pos) |
                                                (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos);
      NRF_GPIO->PIN_CNF[SERIAL_SYNC_PIN_SOUT] = (GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos) |
                                                (GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos) |
                                                (GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos) |
                                                (GPIO_PIN_CNF_DRIVE_S0S1 << GPIO_PIN_CNF_DRIVE_Pos) |
                                                (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos);
      NRF_GPIO->PIN_CNF[SERIAL_SYNC_PIN_SIN] = (GPIO_PIN_CNF_DIR_Input << GPIO_PIN_CNF_DIR_Pos) |
                                               (GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos) |
                                               (GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos) |
                                               (GPIO_PIN_CNF_DRIVE_S0S1 << GPIO_PIN_CNF_DRIVE_Pos) |
                                               (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos);
      SERIAL_SYNC->PSELSCK = SERIAL_SYNC_PIN_SCLK;    // Assign port pin for Serial Clock
      SERIAL_SYNC->PSELMOSI = SERIAL_SYNC_PIN_SOUT;   // Assign port pin for Serial Out
      SERIAL_SYNC->PSELMISO = SERIAL_SYNC_PIN_SIN;    // Assign port pin for serial In

      SERIAL_SYNC->CONFIG = (SPI_CONFIG_ORDER_LsbFirst << SPI_CONFIG_ORDER_Pos) |
                            (SPI_CONFIG_CPHA_Trailing << SPI_CONFIG_CPHA_Pos) |
                            (SPI_CONFIG_CPOL_ActiveLow << SPI_CONFIG_CPOL_Pos);

   #if defined (SERIAL_SYNC_PIN_BR3)
      NRF_GPIO->PIN_CNF[SERIAL_SYNC_PIN_BR3] = (GPIO_PIN_CNF_DIR_Input << GPIO_PIN_CNF_DIR_Pos) |
                                               (GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos) |
                                               (GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos) |
                                               (GPIO_PIN_CNF_DRIVE_S0S1 << GPIO_PIN_CNF_DRIVE_Pos) |
                                               (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos);


      // BR3 = 0 -> 500kHz SPI clk (default low supported rate), BR3 = 1 -> higher rate
      if (NRF_GPIO->IN & (1UL << SERIAL_SYNC_PIN_BR3))
      {
         SERIAL_SYNC->FREQUENCY = (SERIAL_SYNC_FREQUENCY << SPI_FREQUENCY_FREQUENCY_Pos);
      }
      else
   #endif //SERIAL_SYNC_PIN_BR3
      {
         SERIAL_SYNC->FREQUENCY = (SERIAL_SYNC_FREQUENCY_SLOW << SPI_FREQUENCY_FREQUENCY_Pos);
      }

      SERIAL_SYNC_SERIAL_ENABLE(); // enable and acquire associated serial pins

      while (IS_SRDY_ASSERTED()) // reset might have come from SRDY->MSGRDY assertion sequence. Host is waiting for us to assert SEN
      {
         /****
          * Make sure we are ready to catch SRDY falling edge when we do Assert SEN.
          **/

         /* Setup HiToLo Edge Interrupt ready*/
         sd_nvic_DisableIRQ(GPIOTE_IRQn);
         sd_nvic_ClearPendingIRQ(GPIOTE_IRQn);
         NRF_GPIOTE->CONFIG[EVENT_PIN_SRDY] = (GPIOTE_CONFIG_POLARITY_LoToHi << GPIOTE_CONFIG_POLARITY_Pos) |
                                              (SERIAL_SYNC_PIN_SRDY << GPIOTE_CONFIG_PSEL_Pos) |
                                              (GPIOTE_CONFIG_MODE_Event << GPIOTE_CONFIG_MODE_Pos);
         INT_PIN_SRDY_ENABLE();              // Get SRDY interrupt ready as we may about to assert SEN the HOST may respond quickly
         INT_PIN_SRDY_FLAG_CLEAR();
         sd_nvic_SetPriority(GPIOTE_IRQn, APP_IRQ_PRIORITY_LOWEST);
         sd_nvic_EnableIRQ(GPIOTE_IRQn); // Enable GPIO Active low interrupt (Hi to Low)

         /* Setup SRDY De-Assert level sensing to wakeup when are about to sleep */
         NRF_GPIO->PIN_CNF[SERIAL_SYNC_PIN_SRDY] = (GPIO_PIN_CNF_DIR_Input << GPIO_PIN_CNF_DIR_Pos) |
                                                   (GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos) |
                                                   (GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos) |
                                                   (GPIO_PIN_CNF_DRIVE_S0S1 << GPIO_PIN_CNF_DRIVE_Pos) |
                                                   (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos);
         NRF_GPIOTE->EVENTS_PORT = 0;
         NRF_GPIOTE->INTENSET = GPIOTE_INTENSET_PORT_Enabled << GPIOTE_INTENSET_PORT_Pos;
         NRF_GPIO->PIN_CNF[SERIAL_SYNC_PIN_SRDY] |= (GPIO_PIN_CNF_SENSE_High << GPIO_PIN_CNF_SENSE_Pos);

         /* We're ready. Tell host we are and wait for SRDY to De-Assert first before we start transactions */
         SYNC_SEN_ASSERT();

         (void)sd_app_evt_wait();
         NRF_GPIOTE->EVENTS_PORT = 0;

         /* Run HFCLK to latch in interrupt and auto clear flags.*/
         sd_clock_hfclk_request();
      }

      /* configure timer0 to be used as byte synchronous serial interface inter-byte sleep delay timer. Use 1us (1MHz) resolution 16-bit timer.
         Do not enable interrupts, this functionality uses event polling scheme. */
      sd_nvic_DisableIRQ(TIMER1_IRQn);
      sd_nvic_ClearPendingIRQ(TIMER1_IRQn);
      NRF_TIMER1->INTENCLR = (TIMER_INTENCLR_COMPARE0_Clear << TIMER_INTENCLR_COMPARE0_Pos) |
                             (TIMER_INTENCLR_COMPARE1_Clear << TIMER_INTENCLR_COMPARE1_Pos) |
                             (TIMER_INTENCLR_COMPARE2_Clear << TIMER_INTENCLR_COMPARE2_Pos) |
                             (TIMER_INTENCLR_COMPARE3_Clear << TIMER_INTENCLR_COMPARE3_Pos);
      NRF_TIMER1->MODE = TIMER_MODE_MODE_Timer << TIMER_MODE_MODE_Pos;
      NRF_TIMER1->BITMODE = TIMER_BITMODE_BITMODE_16Bit << TIMER_BITMODE_BITMODE_Pos;
      NRF_TIMER1->PRESCALER = 4 << TIMER_PRESCALER_PRESCALER_Pos;
      usSyncSRdySleepDelay = SYNC_SRDY_SLEEP_DELAY_US;

      INT_PIN_SRDY_DISABLE()

      /*force wakeup*/
      bSleep = 1;
      Serial_Wakeup();
   }
   else
#endif //SYNCHRONOUS_DISABLE
   {
#if !defined (ASYNCHRONOUS_DISABLE)
      bSyncMode = false;

      /* Make sure peripheral is reseted, disable first*/
      SERIAL_ASYNC_SERIAL_DISABLE(); // disable peripheral to release associated peripherals
      #if defined(SERIAL_USE_UARTE)
         sd_nvic_DisableIRQ(UARTE0_UART0_IRQn); // disable UART interrupts
         sd_nvic_ClearPendingIRQ(UARTE0_UART0_IRQn); // clear any pending interrupts
         SERIAL_ASYNC->INTENCLR = (UARTE_INTENCLR_ENDTX_Clear << UARTE_INTENCLR_ENDTX_Pos) | (UARTE_INTENCLR_ENDRX_Clear << UARTE_INTENCLR_ENDRX_Pos); // disable both TX and RX END interrupt
         SERIAL_ASYNC->RXD.PTR = (uint32_t)(&aucRxBufferDMA);
         SERIAL_ASYNC->RXD.MAXCNT = 1; // receives 1 byte at a time
         SERIAL_ASYNC->EVENTS_ENDRX = 0;
         SERIAL_ASYNC->EVENTS_ENDTX = 0;
      #else
         sd_nvic_DisableIRQ(UART0_IRQn); // disable UART interrupts
         sd_nvic_ClearPendingIRQ(UART0_IRQn); // clear any pending interrupts
         SERIAL_ASYNC->INTENCLR = (UART_INTENCLR_TXDRDY_Clear << UART_INTENCLR_TXDRDY_Pos) | (UART_INTENCLR_RXDRDY_Clear << UART_INTENCLR_RXDRDY_Pos); // disable both ready interrupt
      #endif
      SERIAL_ASYNC->TASKS_STOPTX = 1; // stop Transmit Task
      SERIAL_ASYNC->TASKS_STOPRX = 1; // stop Reception Task

   #if defined(SERIAL_BAUDRATE_DEFAULT_NDX)
      ucBaudrateNdx = SERIAL_BAUDRATE_DEFAULT_NDX;   // Default
   #endif

   #if defined (SERIAL_ASYNC_BR1) && (SERIAL_ASYNC_BR2) && (SERIAL_ASYNC_BR3)
      NRF_GPIO->PIN_CNF[SERIAL_ASYNC_BR1] = (GPIO_PIN_CNF_DIR_Input << GPIO_PIN_CNF_DIR_Pos) |
                                            (GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos) |
                                            (GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos) |
                                            (GPIO_PIN_CNF_DRIVE_S0S1 << GPIO_PIN_CNF_DRIVE_Pos) |
                                            (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos);
      NRF_GPIO->PIN_CNF[SERIAL_ASYNC_BR2] = (GPIO_PIN_CNF_DIR_Input << GPIO_PIN_CNF_DIR_Pos) |
                                            (GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos) |
                                            (GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos) |
                                            (GPIO_PIN_CNF_DRIVE_S0S1 << GPIO_PIN_CNF_DRIVE_Pos) |
                                            (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos);
      NRF_GPIO->PIN_CNF[SERIAL_ASYNC_BR3] = (GPIO_PIN_CNF_DIR_Input << GPIO_PIN_CNF_DIR_Pos) |
                                            (GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos) |
                                            (GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos) |
                                            (GPIO_PIN_CNF_DRIVE_S0S1 << GPIO_PIN_CNF_DRIVE_Pos) |
                                            (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos);

      System_WriteBufferEmpty(&NRF_GPIO->PIN_CNF[SERIAL_ASYNC_BR3]); //Make sure everything is flushed before evaluating.

      ucBaudrateNdx  = (NRF_GPIO->IN & (1UL << SERIAL_ASYNC_BR1)) ? 0x01 : 0x00;
      ucBaudrateNdx |= (NRF_GPIO->IN & (1UL << SERIAL_ASYNC_BR2)) ? 0x02 : 0x00;
      ucBaudrateNdx |= (NRF_GPIO->IN & (1UL << SERIAL_ASYNC_BR3)) ? 0x04 : 0x00;
   #endif // SERIAL_ASYNC_BR1, SERIAL_ASYNC_BR2, SERIAL_ASYNC_BR3

   #if !defined(PWRSAVE_DISABLE)
      /* configure but don't arm yet the sleep pin */
      NRF_GPIO->PIN_CNF[SERIAL_ASYNC_PIN_SLEEP] =   (GPIO_PIN_CNF_DIR_Input << GPIO_PIN_CNF_DIR_Pos) |
                                                    (GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos) |
                                                    (GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos) |
                                                    (GPIO_PIN_CNF_DRIVE_S0S1 << GPIO_PIN_CNF_DRIVE_Pos) |
                                                    (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos);

      /* configure but don't arm yet the suspend pin */
      NRF_GPIO->PIN_CNF[SERIAL_ASYNC_PIN_SUSPEND] = (GPIO_PIN_CNF_DIR_Input << GPIO_PIN_CNF_DIR_Pos) |
                                                    (GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos) |
                                                    (GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos) |
                                                    (GPIO_PIN_CNF_DRIVE_S0S1 << GPIO_PIN_CNF_DRIVE_Pos) |
                                                    (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos);
   #endif // PWRSAVE_DISABLE

      NRF_GPIO->PIN_CNF[SERIAL_ASYNC_PIN_RXD] = (GPIO_PIN_CNF_DIR_Input << GPIO_PIN_CNF_DIR_Pos) |
                                                (GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos) |
                                                (GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos) |
                                                (GPIO_PIN_CNF_DRIVE_S0S1 << GPIO_PIN_CNF_DRIVE_Pos) |
                                                (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos);
      NRF_GPIO->PIN_CNF[SERIAL_ASYNC_PIN_TXD] = (GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos) |
                                                (GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos) |
                                                (GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos) |
                                                (GPIO_PIN_CNF_DRIVE_S0S1 << GPIO_PIN_CNF_DRIVE_Pos) |
                                                (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos);
      NRF_GPIO->PIN_CNF[SERIAL_ASYNC_PIN_RTS] = (GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos) |
                                                (GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos) |
                                                (GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos) |
                                                (GPIO_PIN_CNF_DRIVE_S0S1 << GPIO_PIN_CNF_DRIVE_Pos) |
                                                (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos);
      #if defined(SERIAL_USE_UARTE)
         SERIAL_ASYNC->PSEL.RXD = SERIAL_ASYNC_PIN_RXD; // assign port pin for RXD
         SERIAL_ASYNC->PSEL.TXD = SERIAL_ASYNC_PIN_TXD; // assign port pin for TXD
         SERIAL_ASYNC->PSEL.RTS = SERIAL_ASYNC_PIN_RTS; // assign port pin for RTS

         SERIAL_ASYNC->CONFIG = (UARTE_CONFIG_HWFC_Enabled << UARTE_CONFIG_HWFC_Pos) |
                                (UARTE_CONFIG_PARITY_Excluded << UARTE_CONFIG_PARITY_Pos); // flow control and parity
         SERIAL_ASYNC->BAUDRATE = asBaudControl[ucBaudrateNdx] << UARTE_BAUDRATE_BAUDRATE_Pos; // baudrate

         SERIAL_ASYNC_SERIAL_ENABLE(); // enable uart and acquire pins
         SERIAL_ASYNC_INT_ENDTX_ENABLE(); // enable end tx interrupt to monitor the end of transmission
         SERIAL_ASYNC_INT_ENDRX_ENABLE(); // enable end rx interrupt to monitor the end of receiving

         sd_nvic_SetPriority(UARTE0_UART0_IRQn, APP_IRQ_PRIORITY_MID); // set it up on application high priority
         sd_nvic_EnableIRQ(UARTE0_UART0_IRQn); // enable UART interrupt
      #else
         SERIAL_ASYNC->PSELRXD = SERIAL_ASYNC_PIN_RXD; // assign port pin for RXD
         SERIAL_ASYNC->PSELTXD = SERIAL_ASYNC_PIN_TXD; // assign port pin for TXD
         SERIAL_ASYNC->PSELRTS = SERIAL_ASYNC_PIN_RTS; // assign port pin for RTS

         SERIAL_ASYNC->CONFIG = (UART_CONFIG_HWFC_Enabled << UART_CONFIG_HWFC_Pos) |
                             (UART_CONFIG_PARITY_Excluded << UART_CONFIG_PARITY_Pos); // flow control and parity
         SERIAL_ASYNC->BAUDRATE = asBaudControl[ucBaudrateNdx] << UART_BAUDRATE_BAUDRATE_Pos; // baudrate

         SERIAL_ASYNC_SERIAL_ENABLE(); // enable uart and acquire pins
         SERIAL_ASYNC_INT_TXRDY_ENABLE(); // enable tx ready interrupt to chain transmission
         SERIAL_ASYNC_INT_RXRDY_ENABLE(); // enable rx ready interrupt to chain transmission

         sd_nvic_SetPriority(UART0_IRQn, APP_IRQ_PRIORITY_MID); // set it up on application high priority
         sd_nvic_EnableIRQ(UART0_IRQn); // enable UART interrupt
      #endif

      /* Though GPIOTE isn't being used, DETECT can trigger on any SENSE configured pin causing GPIOTE flag to be set.
       * Setting GPIOTE interrupt allows us to enter the ISR and clear the pending flag. */
      sd_nvic_SetPriority(GPIOTE_IRQn, APP_IRQ_PRIORITY_LOWEST);
      sd_nvic_EnableIRQ(GPIOTE_IRQn); // enable GPIOTE interrupt

      /*force wakeup*/
      bSleep = 1;
      Serial_Wakeup();

#endif // !ASYNCHRONOUS_DISABLE
   }

   stRxMessage.ANT_MESSAGE_ucSize = 0;
   stTxMessage.stMessageData.ANT_MESSAGE_ucSize = 0;
}

/**
 * @brief Set byte synchronous serial interface bit rate
 */
uint8_t Serial_SetByteSyncSerialBitRate(uint8_t ucConfig)
{
#if !defined (SYNCHRONOUS_DISABLE)
   if (ucConfig > MAX_BITRATE_NDX)
      return INVALID_PARAMETER_PROVIDED;

   SERIAL_SYNC->FREQUENCY = (asBitRateControl[ucConfig] << SPI_FREQUENCY_FREQUENCY_Pos);
   return RESPONSE_NO_ERROR;
#else
   return INVALID_MESSAGE;
#endif // !SYNCHRONOUS DISABLE
}

/**
 * @brief Set byte synchronous serial interface SRDY sleep delay
 */
uint8_t Serial_SetByteSyncSerialSRDYSleep(uint8_t ucDelay)
{
#if !defined (SYNCHRONOUS_DISABLE)
   usSyncSRdySleepDelay = (uint16_t)ucDelay * SYNC_SRDY_SLEEP_DELAY_STEPS;
   return RESPONSE_NO_ERROR;
#else
   return INVALID_MESSAGE;
#endif // !SYNCHRONOUS DISABLE
}

/**
 * @brief Serial interface sleep handler
 */
void Serial_Sleep(void)
{
#if defined (SERIAL_SLEEP_POLLING_MODE)
   /* NOTE: bSleep disabled to make sure Serial Wakeup is configured
    * everytime system is intending to go to sleep.*/
   //if (!bSleep) // if not in serial sleep mode
   {
      bSleep = 1; // indicate that serial is in sleep mode

   #if !defined (SYNCHRONOUS_DISABLE)
      if (bSyncMode)
      {
         sd_clock_hfclk_release();
         Gpiote_FallingEdge_Disable();
         SERIAL_SYNC_SERIAL_DISABLE();

         /* if serial is on hold (bHold) there is no use on waking up Message Ready request
          * since we should ignoring all incoming messages anyway */
         if (!bHold)
         {
            /*Use SENSE->DETECT on pin SMSGRDY at low*/
            /*SMSGRDY Serial Message Ready Pin*/
            /*from PAN 28 v1.2: 7. GPIO: SENSE mechanism fires under some circumstances when it should not.*/
            NRF_GPIO->PIN_CNF[SERIAL_SYNC_PIN_SMSGRDY] = (GPIO_PIN_CNF_DIR_Input << GPIO_PIN_CNF_DIR_Pos) |
                                                         (GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos) |
                                                         (GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos) |
                                                         (GPIO_PIN_CNF_DRIVE_S0S1 << GPIO_PIN_CNF_DRIVE_Pos) |
                                                         (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos);
            /*ARM IT!*/
            NRF_GPIOTE->EVENTS_PORT = 0;
            NRF_GPIOTE->INTENSET = GPIOTE_INTENSET_PORT_Enabled << GPIOTE_INTENSET_PORT_Pos;

            NRF_GPIO->PIN_CNF[SERIAL_SYNC_PIN_SMSGRDY] |= (GPIO_PIN_CNF_SENSE_Low << GPIO_PIN_CNF_SENSE_Pos);

         }
      }
      else
   #endif //!SYNCHRONOUS_DISABLE
      {
   #if !defined (ASYNCHRONOUS_DISABLE)
         uint8_t i;

         SERIAL_ASYNC_RTS_DISABLE();      // disable RXRDY interrupt and relinquish control of RTS line. Do this before stopping rx task
         SERIAL_ASYNC->TASKS_STOPRX = 1;  // stop reception task
         sd_clock_hfclk_release();        // change power states by disabling hi freq clock
         SERIAL_ASYNC_SERIAL_DISABLE();

         /* go through every GPIO pin and save sense configuration while disabling sense during sleep */
         ulPinSenseEnabled = 0;
         ulPinSenseCfg = 0;
         for (i = 0; i < NUMBER_OF_NRF_GPIO_PINS; i++)
         {
            if (NRF_GPIO->PIN_CNF[i] & GPIO_PIN_CNF_SENSE_Msk)
            {
               ulPinSenseEnabled |= 0x01 << i;        // log pin enabled status
               if (((NRF_GPIO->PIN_CNF[i] & GPIO_PIN_CNF_SENSE_Msk) >> GPIO_PIN_CNF_SENSE_Pos) == GPIO_PIN_CNF_SENSE_High)
                  ulPinSenseCfg |= 0x01 << i;         // log sense level status
            }
            // disable sense on this pin
            NRF_GPIO->PIN_CNF[i] &= ~((~GPIO_PIN_CNF_SENSE_Disabled) << GPIO_PIN_CNF_SENSE_Pos);
         }


      #if !defined(PWRSAVE_DISABLE)
         /* set up sense trigger on suspend signal low */
         /*from PAN 28 v1.2: 7. GPIO: SENSE mechanism fires under some circumstances when it should not.*/
         NRF_GPIO->PIN_CNF[SERIAL_ASYNC_PIN_SUSPEND] = (GPIO_PIN_CNF_DIR_Input << GPIO_PIN_CNF_DIR_Pos) |
                                                       (GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos) |
                                                       (GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos) |
                                                       (GPIO_PIN_CNF_DRIVE_S0S1 << GPIO_PIN_CNF_DRIVE_Pos) |
                                                       (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos);

         /* set up sense wakeup on sleep signal low */
         NRF_GPIO->PIN_CNF[SERIAL_ASYNC_PIN_SLEEP] =   (GPIO_PIN_CNF_DIR_Input << GPIO_PIN_CNF_DIR_Pos) |
                                                       (GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos) |
                                                       (GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos) |
                                                       (GPIO_PIN_CNF_DRIVE_S0S1 << GPIO_PIN_CNF_DRIVE_Pos) |
                                                       (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos);
      #endif // PWRSAVE_DISABLE

         /*ARM IT!*/
         NRF_GPIOTE->EVENTS_PORT = 0;
         NRF_GPIOTE->INTENSET = GPIOTE_INTENSET_PORT_Enabled << GPIOTE_INTENSET_PORT_Pos;

      #if !defined(PWRSAVE_DISABLE)
         NRF_GPIO->PIN_CNF[SERIAL_ASYNC_PIN_SLEEP] |= (GPIO_PIN_CNF_SENSE_Low << GPIO_PIN_CNF_SENSE_Pos);
         NRF_GPIO->PIN_CNF[SERIAL_ASYNC_PIN_SUSPEND] |= (GPIO_PIN_CNF_SENSE_Low << GPIO_PIN_CNF_SENSE_Pos);
      #endif // PWRSAVE_DISABLE


   #endif //!ASYNCHRONOUS_DSIABLE
      }
   }
#endif // SERIAL_SLEEP_POLLING_MODE
}

/**
 * @brief Serial interface wakeup handler
 */
static void Serial_Wakeup(void)
{
#if defined (SERIAL_SLEEP_POLLING_MODE)
   if(bSleep) // if in serial sleep mode
   {
      bSleep = 0; // indicate that serial is not in sleep mode

   #if !defined (SYNCHRONOUS_DISABLE)
      if (bSyncMode)
      {
         /*check if we are waking up from an asserted smsgrdy with srdy asserted.*/
         if(NRF_GPIO->PIN_CNF[SERIAL_SYNC_PIN_SMSGRDY] & (GPIO_PIN_CNF_SENSE_Low << GPIO_PIN_CNF_SENSE_Pos))
         {
            if (IS_SRDY_ASSERTED())
            {
               SYNC_SEN_DEASSERT(); // set the SEN to be high
               NVIC_SystemReset(); // Initiate System reset now.
            }
         }

         NRF_GPIOTE->INTENCLR = GPIOTE_INTENSET_PORT_Enabled << GPIOTE_INTENSET_PORT_Pos;
         /*Made sure SMSGRDY not sensitive*/
         NRF_GPIO->PIN_CNF[SERIAL_SYNC_PIN_SMSGRDY] = (GPIO_PIN_CNF_DIR_Input << GPIO_PIN_CNF_DIR_Pos) |
                                                      (GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos) |
                                                      (GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos) |
                                                      (GPIO_PIN_CNF_DRIVE_S0S1 << GPIO_PIN_CNF_DRIVE_Pos) |
                                                      (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos);
         NRF_GPIO->PIN_CNF[SERIAL_SYNC_PIN_SRDY] = (GPIO_PIN_CNF_DIR_Input << GPIO_PIN_CNF_DIR_Pos) |
                                                      (GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos) |
                                                      (GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos) |
                                                      (GPIO_PIN_CNF_DRIVE_S0S1 << GPIO_PIN_CNF_DRIVE_Pos) |
                                                      (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos);
         NRF_GPIOTE->EVENTS_PORT = 0;
         ucPinSenseSRDY = PIN_SENSE_DISABLED;
         ucPinSenseMRDY = PIN_SENSE_DISABLED;


         sd_clock_hfclk_request();
         SERIAL_SYNC_SERIAL_ENABLE();
         Gpiote_FallingEdge_Enable(); // re-enable GPIOTE sense
      }
      else
   #endif //!SYNCHRONOUS_DISABLE
      {
   #if !defined (ASYNCHRONOUS_DISABLE)
         uint8_t i;

         NRF_GPIOTE->INTENCLR = GPIOTE_INTENSET_PORT_Enabled << GPIOTE_INTENSET_PORT_Pos;

         /* restore pin sense configuration for wakeup */
         for (i = 0; i < NUMBER_OF_NRF_GPIO_PINS; i++)
         {
            if ((ulPinSenseEnabled >> i) & 0x01)
            {
               // pins were all disabled, so we can just OR the values in
               if ((ulPinSenseCfg >> i) & 0x01)
                  NRF_GPIO->PIN_CNF[i] |= (GPIO_PIN_CNF_SENSE_High << GPIO_PIN_CNF_SENSE_Pos);
               else
                  NRF_GPIO->PIN_CNF[i] |= (GPIO_PIN_CNF_SENSE_Low << GPIO_PIN_CNF_SENSE_Pos);
            }
            else // disable sense on this pin
               NRF_GPIO->PIN_CNF[i] &= ~((~GPIO_PIN_CNF_SENSE_Disabled) << GPIO_PIN_CNF_SENSE_Pos);
         }

      #if !defined(PWRSAVE_DISABLE)
         /*Made sure SUSPEND not sensitive*/
         NRF_GPIO->PIN_CNF[SERIAL_ASYNC_PIN_SUSPEND] = (GPIO_PIN_CNF_DIR_Input << GPIO_PIN_CNF_DIR_Pos) |
                                                       (GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos) |
                                                       (GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos) |
                                                       (GPIO_PIN_CNF_DRIVE_S0S1 << GPIO_PIN_CNF_DRIVE_Pos) |
                                                       (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos);

         /* Make sure SLEEP not sensitive */
         NRF_GPIO->PIN_CNF[SERIAL_ASYNC_PIN_SLEEP] =   (GPIO_PIN_CNF_DIR_Input << GPIO_PIN_CNF_DIR_Pos) |
                                                       (GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos) |
                                                       (GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos) |
                                                       (GPIO_PIN_CNF_DRIVE_S0S1 << GPIO_PIN_CNF_DRIVE_Pos) |
                                                       (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos);
      #endif // PWRSAVE_DISABLE

         NRF_GPIOTE->EVENTS_PORT = 0;
         NRF_GPIOTE->INTENSET = GPIOTE_INTENSET_PORT_Enabled << GPIOTE_INTENSET_PORT_Pos;

      #if !defined(PWRSAVE_DISABLE)
         NRF_GPIO->PIN_CNF[SERIAL_ASYNC_PIN_SLEEP] |= (GPIO_PIN_CNF_SENSE_High << GPIO_PIN_CNF_SENSE_Pos);
         NRF_GPIO->PIN_CNF[SERIAL_ASYNC_PIN_SUSPEND] |= (GPIO_PIN_CNF_SENSE_Low << GPIO_PIN_CNF_SENSE_Pos);
      #endif // PWRSAVE_DISABLE


         stRxMessage.ANT_MESSAGE_ucSize = 0; // reset message counter
         sd_clock_hfclk_request();           // change power states by re-enabling hi freq clock
         SERIAL_ASYNC_SERIAL_ENABLE();
         SERIAL_ASYNC->TASKS_STARTRX = 1;    // start Reception as early as now. Do this before releasing RTS
         if (!bHold)
            SERIAL_ASYNC_RTS_ENABLE();       // re-enable UART control of RTS line if it wasn't held to begin with

   #endif //!ASYNCHRONOUS_DISABLE
      }
   }
#endif // SERIAL_SLEEP_POLLING_MODE
}

/**
 * @brief Get input message buffer
 */
ANT_MESSAGE *Serial_GetRxMesgPtr(void)
{
   return ((ANT_MESSAGE *)&stRxMessage);
}

/**
 * @brief Get output message buffer
 */
ANT_MESSAGE *Serial_GetTxMesgPtr(void)
{
   return ((ANT_MESSAGE *)&stTxMessage.stMessageData);
}

/**
 * @brief Set baudrate
 */
uint8_t  Serial_SetAsyncBaudrate(BAUDRATE_TYPE baud)
{
    eBaudSelection = baud;
    // Make sure selected baudrate is supported before triggering bEventSetBaudrate
    if(eBaudSelection < BAUD_BITFIELD_SIZE && (BAUD_SUPPORTED_BITFIELD & (0x1 << eBaudSelection)))
    {
       bEventSetBaudrate = true; //flag set baudrate event
       return RESPONSE_NO_ERROR;
    }
    else
    {
       return INVALID_PARAMETER_PROVIDED;
    }
}

/**
 * @brief Activate previously set baudrate
 */
void Serial_ActivateAsyncBaudrate()
{
    while(bTransmitting) { }
    SERIAL_ASYNC->BAUDRATE = asBaudLookup[eBaudSelection];
}

/**
 * @brief Allow incoming serial communication
 */
void Serial_ReleaseRx(void)
{
   bHold = false; // release it


#if !defined (ASYNCHRONOUS_DISABLE)
   if(!bSyncMode)
   {
      stRxMessage.ANT_MESSAGE_ucSize = 0; // reset the serial receive state machine
      SERIAL_ASYNC_RTS_ENABLE();
   }
#endif // !ASYNCHRONOUS_DISABLE
}

/**
 * @brief Hold incoming serial communication
 */
void Serial_HoldRx(void)
{
   bHold = true; // hold it


#if !defined (ASYNCHRONOUS_DISABLE)
   if(!bSyncMode)
   {
      SERIAL_ASYNC_RTS_DISABLE();
   }
#endif // !ASYNCHRONOUS_DISABLE
}

/**
 * @brief Receive serial message
 */
bool Serial_RxMessage(void)
{
#if !defined (SYNCHRONOUS_DISABLE)
   if(bSyncMode)
   {
      if (Sync_IsMSGRDYAsserted())
      {
         Serial_Wakeup();

         SyncProc_RxMessage();
         return true; // allow serial sleep
      }
   }
   else
#endif // !SYNCHRONOUS_DISABLE
   {
#if (!defined (ASYNCHRONOUS_DISABLE) && !defined(PWRSAVE_DISABLE))
      if (!IS_PIN_SLEEP_ASSERTED())
      {
         Serial_Wakeup();
         return false; // disallow serial sleep
      }
#else
      return false; // disallow serial sleep
#endif // !ASYNCHRONOUS_DISABLE && !PWRSAVE_DISABLE
   }

#if !defined(PWRSAVE_DISABLE)
   return true; // allow serial sleep
#endif // !PWRSAVE_DISABLE
}

/**
 * @brief Send serial message
 */
void Serial_TxMessage(void)
{
   if(stTxMessage.stMessageData.ANT_MESSAGE_ucSize) // if message size is not empty, there should be something to transmit.
   {
      Serial_Wakeup();
   }
   else
   {
   #if !defined (SYNCHRONOUS_DISABLE)
      if(bSyncMode)
      {
         SYNC_SEN_DEASSERT(); // make sure SEN is deasserted if we have nothing to transmit.
      }
   #endif

      return; //buffer is empty, get out of here.
   }

   if (!bTransmitting)
   {
      bStartMessage = true;
      bEndMessage = false;
      bTransmitting = true;

      stTxMessage.stMessageData.ANT_MESSAGE_ucCheckSum = MESG_TX_SYNC; // include MESG_TX_SYNC to checksum calculation

      for (ucTxPtr=0; ucTxPtr<=(stTxMessage.stMessageData.ANT_MESSAGE_ucSize + 1); ucTxPtr++) // have to go two more than size so we include the size and the ID
         stTxMessage.stMessageData.ANT_MESSAGE_ucCheckSum ^= stTxMessage.stMessageData.aucMessage[ucTxPtr]; // calculate the checksum

      stTxMessage.stMessageData.ANT_MESSAGE_aucMesgData[stTxMessage.stMessageData.ANT_MESSAGE_ucSize] = stTxMessage.stMessageData.ANT_MESSAGE_ucCheckSum; // move the calculated checksum to the correct location in the message

      if(bSyncMode)
      {
         SyncProc_TxMessage();
      }
      else
      {
         AsyncProc_TxMessage(); // kick the first transmission.
         do
         {
            (void)sd_app_evt_wait();
         }
         while (stTxMessage.stMessageData.ANT_MESSAGE_ucSize); // wait for the whole message to be transmitted
      }
   }
}

#if !defined (SYNCHRONOUS_DISABLE)
/**
 * @brief Enable active low detection for SMSGRDY and SRDY
 */
static void Gpiote_FallingEdge_Enable (void)
{
   sd_nvic_DisableIRQ(GPIOTE_IRQn);
   sd_nvic_ClearPendingIRQ(GPIOTE_IRQn);

   if (bSyncMode)
   {
      /*SMSGRDY Hi to Lo interrupt*/
      NRF_GPIOTE->CONFIG[EVENT_PIN_SMSGRDY] = (GPIOTE_CONFIG_POLARITY_HiToLo << GPIOTE_CONFIG_POLARITY_Pos) |
                                              (SERIAL_SYNC_PIN_SMSGRDY << GPIOTE_CONFIG_PSEL_Pos) |
                                              (GPIOTE_CONFIG_MODE_Event << GPIOTE_CONFIG_MODE_Pos);
      /*SRDY Lo to Hi interrupt*/
      NRF_GPIOTE->CONFIG[EVENT_PIN_SRDY] = (GPIOTE_CONFIG_POLARITY_LoToHi << GPIOTE_CONFIG_POLARITY_Pos) |
                                           (SERIAL_SYNC_PIN_SRDY << GPIOTE_CONFIG_PSEL_Pos) |
                                           (GPIOTE_CONFIG_MODE_Event << GPIOTE_CONFIG_MODE_Pos);

      INT_PIN_SMSGRDY_ENABLE();
   }

   sd_nvic_SetPriority(GPIOTE_IRQn, APP_IRQ_PRIORITY_LOWEST);
   sd_nvic_EnableIRQ(GPIOTE_IRQn);
}
#endif // !SYNCHRONOUS_DISABLE

#if !defined (SYNCHRONOUS_DISABLE)
/**
 * @brief Disable active low detection for SMSGRDY and SRDY
 */
static void Gpiote_FallingEdge_Disable (void)
{
   sd_nvic_DisableIRQ(GPIOTE_IRQn);
   sd_nvic_ClearPendingIRQ(GPIOTE_IRQn);

   if (bSyncMode)
   {
      NRF_GPIOTE->CONFIG[EVENT_PIN_SMSGRDY] = 0UL;
      NRF_GPIOTE->CONFIG[EVENT_PIN_SRDY] = 0UL;
   }
}
#endif // !SYNCHRONOUS_DISABLE

#if !defined (SYNCHRONOUS_DISABLE)
/**
 * @brief Handle SRDY Sleep
 */
static void SyncSRDYSleep(void)
{
   bool bSleepTimeout;

   if (bStartMessage)
   {
      bStartMessage = false; // clear the start flag
      INT_PIN_SRDY_FLAG_CLEAR(); // clear the interrupt status bit
      INT_PIN_SRDY_ENABLE(); // get SRDY interrupt ready as we may about to assert SEN the HOST may respond quickly
      SYNC_SEN_ASSERT(); // set the serial enable low (active)
   }

   if(!bSRdy) // if sRdy has not already occurred, wait for it
   {
      // setup & start timer for sleep timeout
      NRF_TIMER1->TASKS_CLEAR = 1;
      NRF_TIMER1->CC[0] = usSyncSRdySleepDelay;
      NRF_TIMER1->EVENTS_COMPARE[0] = 0;
      NRF_TIMER1->TASKS_START = 1;
      bSleepTimeout = false;

      do
      {
         // check time and then check SRdy. Ordering is important!
         if (NRF_TIMER1->EVENTS_COMPARE[0] && !bSRdy)
         {
            bSleepTimeout = true;

            //enable sensing first before disarming SRDY interrupt.
            NRF_GPIO->PIN_CNF[SERIAL_SYNC_PIN_SRDY] = (GPIO_PIN_CNF_DIR_Input << GPIO_PIN_CNF_DIR_Pos) |
                                                      (GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos) |
                                                      (GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos) |
                                                      (GPIO_PIN_CNF_DRIVE_S0S1 << GPIO_PIN_CNF_DRIVE_Pos) |
                                                      (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos);
            NRF_GPIOTE->EVENTS_PORT = 0;
            NRF_GPIOTE->INTENSET = GPIOTE_INTENSET_PORT_Enabled << GPIOTE_INTENSET_PORT_Pos;
            NRF_GPIO->PIN_CNF[SERIAL_SYNC_PIN_SRDY] |= (GPIO_PIN_CNF_SENSE_Low << GPIO_PIN_CNF_SENSE_Pos);

            sd_clock_hfclk_release();
            Gpiote_FallingEdge_Disable();

            while (!bSRdy)
            {
               (void)sd_app_evt_wait();

               if(NRF_GPIOTE->EVENTS_PORT || (ucPinSenseSRDY == PIN_SENSE_LOW))// check if SRDY woke us.
               {
                  NRF_GPIOTE->EVENTS_PORT = 0;
                  ucPinSenseSRDY = PIN_SENSE_DISABLED;

                  NRF_GPIO->PIN_CNF[SERIAL_SYNC_PIN_SRDY] = (GPIO_PIN_CNF_DIR_Input << GPIO_PIN_CNF_DIR_Pos) |
                                                            (GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos) |
                                                            (GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos) |
                                                            (GPIO_PIN_CNF_DRIVE_S0S1 << GPIO_PIN_CNF_DRIVE_Pos) |
                                                            (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos);

                  NRF_GPIO->PIN_CNF[SERIAL_SYNC_PIN_SMSGRDY] = (GPIO_PIN_CNF_DIR_Input << GPIO_PIN_CNF_DIR_Pos) |
                                                               (GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos) |
                                                               (GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos) |
                                                               (GPIO_PIN_CNF_DRIVE_S0S1 << GPIO_PIN_CNF_DRIVE_Pos) |
                                                               (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos);
                  NRF_GPIOTE->EVENTS_PORT = 0;
                  sd_nvic_ClearPendingIRQ(GPIOTE_IRQn);
                  NRF_GPIOTE->INTENSET = GPIOTE_INTENSET_PORT_Enabled << GPIOTE_INTENSET_PORT_Pos;

                  NRF_GPIO->PIN_CNF[SERIAL_SYNC_PIN_SRDY] |= (GPIO_PIN_CNF_SENSE_High << GPIO_PIN_CNF_SENSE_Pos);

                  if(!IS_MRDY_ASSERTED())
                  {
                     NRF_GPIO->PIN_CNF[SERIAL_SYNC_PIN_SMSGRDY] |= (GPIO_PIN_CNF_SENSE_Low << GPIO_PIN_CNF_SENSE_Pos);
                  }

                  while (IS_SRDY_ASSERTED())
                  {
                     (void)sd_app_evt_wait();

                     if(NRF_GPIOTE->EVENTS_PORT || (ucPinSenseSRDY == PIN_SENSE_HIGH) || (ucPinSenseMRDY == PIN_SENSE_LOW))
                     {
                        NRF_GPIOTE->EVENTS_PORT = 0;
                        if(IS_MRDY_ASSERTED() && (((NRF_GPIO->PIN_CNF[SERIAL_SYNC_PIN_SMSGRDY] & GPIO_PIN_CNF_SENSE_Msk) >> GPIO_PIN_CNF_SENSE_Pos ) == GPIO_PIN_CNF_SENSE_Low))
                        {
                           SYNC_SEN_DEASSERT(); // set the SEN to be high
                           NVIC_SystemReset(); // initiate System reset now.
                        }
                     }
                  }

                  bSRdy = 1; // yes, SRDY did.
               }
            }
         }
      } while (!bSRdy || IS_SRDY_ASSERTED());

      if (bSleepTimeout) // if we had gone to sleep, perform serial wakeup procedure
      {
         bSleep = 1;
         Serial_Wakeup();
      }

      NRF_TIMER1->TASKS_STOP = 1;
   }
   bSRdy = false; // take out the semaphore

   if (bEndMessage) // handle the end of the message
   {
      bEndMessage = false; // clear the end flag
      INT_PIN_SRDY_DISABLE(); // disable SRDY interrupt since we don't need it.
      SYNC_SEN_DEASSERT(); // set the serial enable high (inactive)
   }
}
#endif // !SYNCHRONOUS_DISABLE

#if !defined (SYNCHRONOUS_DISABLE)
/**
 * @brief Handle synchronous byte transaction
 */
static uint8_t SyncReadWriteByte(uint8_t ucByte)
{
   uint8_t ucReadByte;

   ucReadByte = 0; // initialize the receive byte

   SyncSRDYSleep(); // wait for an SRDY before beginning

   SERIAL_SYNC->TXD = ucByte; // send transmit
   while (!SERIAL_SYNC->EVENTS_READY); // wait for data to be ready
   SERIAL_SYNC->EVENTS_READY = 0; // clear bit
   ucReadByte = SERIAL_SYNC->RXD; // read back byte

   return ucReadByte;
}
#endif // !SYNCHRONOUS_DISABLE

#if !defined (SYNCHRONOUS_DISABLE)
/**
 * @brief Synchronous rx message
 */
static void SyncProc_RxMessage(void)
{
   uint8_t ucRxSize;
   uint8_t ucRxCheckSum;

   bStartMessage = true; // flag the start of a message

   SyncReadWriteByte(MESG_RX_SYNC); // write the read message sync byte

   ucRxSize     = SyncReadWriteByte(0xFF); // read the message size byte
   ucRxCheckSum = MESG_RX_SYNC ^ ucRxSize; // initialize the checksum

   if ((ucRxSize >= SERIAL_RX_BUFFER_SIZE) || (ucRxSize  == 0)) // if the message is too big for our receive buffer or empty
   {
      SYNC_SEN_DEASSERT(); // set the serial enable high (inactive)
      return; // exit
   }

   for (ucRxPtr=0; ucRxPtr<=ucRxSize; ucRxPtr++) // we have to account for the message ID too that's why it's <=
   {
      stRxMessage.ANT_MESSAGE_aucFramedData[ucRxPtr] = SyncReadWriteByte(0xFF); // read the byte
      ucRxCheckSum ^= stRxMessage.ANT_MESSAGE_aucFramedData[ucRxPtr]; // recalculate the checksum
   }

   bEndMessage = true; // flag the end of the message
   ucRxCheckSum ^= SyncReadWriteByte(0xFF); // read the checksum byte and xor it with the calculated checksum

   if (!ucRxCheckSum) // if we passed the checksum
   {
      Serial_HoldRx();
      bEventRXSerialMessageProcess = true; // flag that we have a rx serial message to process
   }

   stRxMessage.ANT_MESSAGE_ucSize = ucRxSize; // save the receive message size
}
#endif // !SYNCHRONOUS_DISABLE

/**
 * @brief Synchronous tx message
 */
static void SyncProc_TxMessage(void)
{
#if !defined (SYNCHRONOUS_DISABLE)
   uint8_t *pucData;
   uint8_t ucTxSize;

   bStartMessage = true; // flag the start of a message
   ucTxSize = stTxMessage.stMessageData.ANT_MESSAGE_ucSize; // read out the transmit size
   stTxMessage.stMessageData.ANT_MESSAGE_ucSize = 0; // clear the transmit size

   SyncReadWriteByte(MESG_TX_SYNC); // send the SYNC byte
   SyncReadWriteByte(ucTxSize); // send the size byte

   ucTxSize += 1; // add 1 more bytes to include MessageID
   pucData = (uint8_t *)&stTxMessage.stMessageData.ANT_MESSAGE_aucFramedData[0];// point the data for transmission

   do
   {
      SyncReadWriteByte(*pucData++); // send all bytes
   }
   while (--ucTxSize);

   bEndMessage = true; // flag the end of the message
   SyncReadWriteByte(*pucData); // send the last byte it should be the checksum

   bTransmitting = false; // transmission done.
#endif // !SYNCHRONOUS_DISABLE
}

#if !defined (SYNCHRONOUS_DISABLE)
/**
 * @brief Synchronous MSGRDY check
 */
static bool Sync_IsMSGRDYAsserted(void)
{
   if(bHold)
      return false;

   if (IS_MRDY_ASSERTED())
   {
      return true;
   }
   else
   {
      return false;
   }
}
#endif // !SYNCHRONOUS_DISABLE

/**
 * @brief Asynchronous tx message
 */
static void AsyncProc_TxMessage(void)
{
#if !defined (ASYNCHRONOUS_DISABLE)
   #if defined (SERIAL_USE_UARTE)
      SERIAL_ASYNC->EVENTS_ENDTX = 0;
      if (stTxMessage.stMessageData.ANT_MESSAGE_ucSize && !bEndMessage) // if we have a message that is ready to send
      {
         if (bStartMessage)
         {
            bStartMessage = false;
            stTxMessage.ucMessageSyncByte = MESG_TX_SYNC;
            SERIAL_ASYNC->TXD.PTR = (uint32_t)(&stTxMessage.ucMessageSyncByte);
            SERIAL_ASYNC->TXD.MAXCNT = stTxMessage.stMessageData.ANT_MESSAGE_ucSize
                                     + MESG_SYNC_SIZE
                                     + MESG_SIZE_SIZE
                                     + MESG_ID_SIZE
                                     + MESG_CHECKSUM_SIZE;
            SERIAL_ASYNC_START_TX();
            bTransmitting = true;
         }
         else
         {
            SERIAL_ASYNC_STOP_TX();
            stTxMessage.stMessageData.ANT_MESSAGE_ucSize = 0;
            bEndMessage = true;
            bTransmitting = false;
         }
      }
   #else
      SERIAL_ASYNC->EVENTS_TXDRDY = 0;  // make sure flag is clear to make way for the setting on empty

      if (stTxMessage.stMessageData.ANT_MESSAGE_ucSize && !bEndMessage) // if we have a message that is ready to send
      {
         if (bStartMessage) // we have to write the SYNC byte as well
         {
            bStartMessage = false;
            ucTxPtr = 0; // initialize the message pointer for below

            SERIAL_ASYNC_START_TX();
            SERIAL_ASYNC->TXD = MESG_TX_SYNC; // write the sync byte
         }
         else if (ucTxPtr <= (stTxMessage.stMessageData.ANT_MESSAGE_ucSize+2)) // send the data and checksum
         {
            SERIAL_ASYNC->TXD = stTxMessage.stMessageData.aucMessage[ucTxPtr++];
         }
         else
         {
            SERIAL_ASYNC_STOP_TX();
            stTxMessage.stMessageData.ANT_MESSAGE_ucSize = 0;
            bEndMessage = true;
            bTransmitting = false;
         }
      }
      else
      {
         SERIAL_ASYNC_STOP_TX();
         stTxMessage.stMessageData.ANT_MESSAGE_ucSize = 0;
         bEndMessage = true;
         bTransmitting = false;
      }
   #endif // SERIAL_USE_UARTE
#endif // !ASYNCHRONOUS_DISABLE
}

/**
 * @brief Asynchronous rx message
 */
#if !defined (ASYNCHRONOUS_DISABLE)
#define MESG_SIZE_READ     ((uint8_t)0x55) // async control flag
static void AsyncProc_RxMessage(void)
{
   uint8_t ucByte;
   uint8_t ucURxStatus;
   #if defined(SERIAL_USE_UARTE)
      SERIAL_ASYNC->EVENTS_ENDRX = 0; // clear the pending interrupt flag
      ucByte = aucRxBufferDMA; // read the incoming char
      SERIAL_ASYNC->RXD.MAXCNT = 1;
      SERIAL_ASYNC->TASKS_STARTRX = 1;
   #else
      SERIAL_ASYNC->EVENTS_RXDRDY = 0;  // clear the pending interrupt flag
      ucByte = SERIAL_ASYNC->RXD;  // read the incoming char
   #endif // SERIAL_USE_UARTE
   ucURxStatus = SERIAL_ASYNC->ERRORSRC; // read the overflow flag

   #if defined(SERIAL_USE_UARTE)
   if (ucURxStatus & UARTE_ERRORSRC_FRAMING_Msk) // if we had a character error
   #else
   if (ucURxStatus & UART_ERRORSRC_FRAMING_Msk)
   #endif
   {
      SERIAL_ASYNC->ERRORSRC = ucURxStatus;
      stRxMessage.ANT_MESSAGE_ucSize = 0;
      #if defined(SERIAL_USE_UARTE)
         SERIAL_ASYNC_RX_RESTART();
      #endif // SERIAL_USE_UARTE
   }
   #if defined(SERIAL_USE_UARTE)
   if (ucURxStatus & (UARTE_ERRORSRC_PARITY_Msk))
   #else
   if (ucURxStatus & (UART_ERRORSRC_PARITY_Msk))
   #endif
   {
      SERIAL_ASYNC->ERRORSRC = ucURxStatus;
      stRxMessage.ANT_MESSAGE_ucSize = 0;
      #if defined(SERIAL_USE_UARTE)
         SERIAL_ASYNC_RX_RESTART();
      #endif // SERIAL_USE_UARTE
   }
   #if defined(SERIAL_USE_UARTE)
   if (ucURxStatus & (UARTE_ERRORSRC_OVERRUN_Msk))
   #else
   if (ucURxStatus & (UART_ERRORSRC_OVERRUN_Msk))
   #endif
   {
      SERIAL_ASYNC->ERRORSRC = ucURxStatus;
      stRxMessage.ANT_MESSAGE_ucSize = 0;
      #if defined(SERIAL_USE_UARTE)
         SERIAL_ASYNC_RX_RESTART();
      #endif // SERIAL_USE_UARTE
   }

   if (!stRxMessage.ANT_MESSAGE_ucSize) // we are looking for the sync byte of a message
   {
      if (ucByte == MESG_TX_SYNC) // this is a valid SYNC byte
      {
         stRxMessage.ANT_MESSAGE_ucCheckSum = MESG_TX_SYNC; // init the checksum
         stRxMessage.ANT_MESSAGE_ucSize     = MESG_SIZE_READ; // set the byte pointer to get the size byte
      }
   }
   else if (stRxMessage.ANT_MESSAGE_ucSize == MESG_SIZE_READ) // if we are processing the size byte of a message
   {
      stRxMessage.ANT_MESSAGE_ucSize = 0; // if the size is invalid we want to reset the rx message

      if (ucByte <= MESG_MAX_SIZE_VALUE) // make sure this is a valid message
      {
         stRxMessage.ANT_MESSAGE_ucSize      = ucByte; // save the size of the message
         stRxMessage.ANT_MESSAGE_ucCheckSum ^= ucByte; // calculate checksum
         ucRxPtr       = 0; // set the byte pointer to start collecting the message
      }
   }
   else
   {
      stRxMessage.ANT_MESSAGE_ucCheckSum ^= ucByte; // calculate checksum

      if (ucRxPtr > stRxMessage.ANT_MESSAGE_ucSize) // we have received the whole message + 1 for the message ID
      {
         #if defined (SERIAL_USE_UARTE)
            SERIAL_ASYNC_RX_RESTART();
         #endif // SERIAL_USE_UARTE
         if (!stRxMessage.ANT_MESSAGE_ucCheckSum) // the checksum passed
         {
            Serial_HoldRx();
            bEventRXSerialMessageProcess = true; // flag that we have a rx serial message to process
         }
         else
         {
            stRxMessage.ANT_MESSAGE_ucSize = 0; // reset the RX message
         }
      }
      else // this is a data byte
      {
         stRxMessage.ANT_MESSAGE_aucFramedData[ucRxPtr++] = ucByte; // save the byte
      }
   }
}
#endif // !ASYNCHRONOUS_DISABLE

/**
 * @brief Interrupt handler for asynchronous serial interface
 */
void Serial_UART0_IRQHandler(void)
{
#if !defined(ASYNCHRONOUS_DISABLE)
   #if defined (SERIAL_USE_UARTE)
   if (SERIAL_ASYNC->EVENTS_ENDRX && (SERIAL_ASYNC->INTENSET & (UARTE_INTENSET_ENDRX_Set << UARTE_INTENSET_ENDRX_Pos)))
   #else
   if (SERIAL_ASYNC->EVENTS_RXDRDY && (SERIAL_ASYNC->INTENSET & (UART_INTENSET_RXDRDY_Set << UART_INTENSET_RXDRDY_Pos)))
   #endif // SERIAL_USE_UARTE
   {
      AsyncProc_RxMessage();
   }
   #if defined (SERIAL_USE_UARTE)
   if (SERIAL_ASYNC->EVENTS_ENDTX && (SERIAL_ASYNC->INTENSET & (UARTE_INTENSET_ENDTX_Set << UARTE_INTENSET_ENDTX_Pos)))
   #else
   if (SERIAL_ASYNC->EVENTS_TXDRDY && (SERIAL_ASYNC->INTENSET & (UART_INTENSET_TXDRDY_Set << UART_INTENSET_TXDRDY_Pos)))
   #endif // SERIAL_USE_UARTE
   {
      AsyncProc_TxMessage();
   }
#endif // !ASYNCHRONOUS_DISABLE
}

/**
 * @brief Interrupt handler for synchronous serial SMSGRDY and SRDY interrupt. Uses GPIOTE 0 and 1
 */
void Serial_GPIOTE_IRQHandler(void)
{
#if !defined (SYNCHRONOUS_DISABLE)
   if (bSyncMode)
   {
      if (IS_INT_PIN_SMSGRDY_ASSERTED()) // MRDY / SLEEP_ENABLE
      {
      #if !defined (SYNCHRONOUS_DISABLE)
         if (bSyncMode && IS_SRDY_ASSERTED()) // if this is sync, and SRDY is low and not pending, then this is a sync reset operation
         {
            SYNC_SEN_DEASSERT(); // set the SEN to be high
            NVIC_SystemReset(); // initiate System reset now.
         }
         INT_PIN_SMSGRDY_FLAG_CLEAR(); // clear the interrupt status bit
      #endif // !SYNCHRONOUS_DISABLE
      }

      if (IS_INT_PIN_SRDY_ASSERTED())// && (bSRdy == false)) // SRDY is pending and semaphore is waiting
      {
         INT_PIN_SRDY_FLAG_CLEAR(); // clear the interrupt status bit
         bSRdy = true; // set the srdy flag
      }

      if(NRF_GPIOTE->EVENTS_PORT)// && (NRF_GPIOTE->INTENSET & (1UL << GPIOTE_INTENSET_PORT_Pos) ))
      {
         /*Detecting SRDY LOW*/
         if(((NRF_GPIO->PIN_CNF[SERIAL_SYNC_PIN_SRDY] & GPIO_PIN_CNF_SENSE_Msk) >> GPIO_PIN_CNF_SENSE_Pos ) == GPIO_PIN_CNF_SENSE_Low)
         {
            ucPinSenseSRDY = PIN_SENSE_LOW;
         }
         /*Detecting SRDY HIGH*/
         if(((NRF_GPIO->PIN_CNF[SERIAL_SYNC_PIN_SRDY] & GPIO_PIN_CNF_SENSE_Msk) >> GPIO_PIN_CNF_SENSE_Pos ) == GPIO_PIN_CNF_SENSE_High)
         {
            ucPinSenseSRDY = PIN_SENSE_HIGH;
         }
         /*Detecting MRDY LOW*/
         if(((NRF_GPIO->PIN_CNF[SERIAL_SYNC_PIN_SMSGRDY] & GPIO_PIN_CNF_SENSE_Msk) >> GPIO_PIN_CNF_SENSE_Pos ) == GPIO_PIN_CNF_SENSE_Low)
         {
            ucPinSenseMRDY = PIN_SENSE_LOW;
         }
         /*Detecting MRDY High*/
         if(((NRF_GPIO->PIN_CNF[SERIAL_SYNC_PIN_SMSGRDY] & GPIO_PIN_CNF_SENSE_Msk) >> GPIO_PIN_CNF_SENSE_Pos ) == GPIO_PIN_CNF_SENSE_High)
         {
            ucPinSenseMRDY = PIN_SENSE_HIGH;
         }
         NRF_GPIOTE->EVENTS_PORT = 0;
      }
   }
   else
#endif //!SYNCHRONOUS_DISABLE
   {
#if !defined (ASYNCHRONOUS_DISABLE) && !defined(PWRSAVE_DISABLE)
      // handle pin sense detection of SUSPEND
      if (IS_PIN_SUSPEND_ASSERTED())
      {
         // handle assertion of suspend pin. Enter deep sleep mode only if sleep is asserted.
         if (!IS_PIN_SLEEP_ASSERTED())
         {
            NRF_GPIOTE->EVENTS_PORT = 0; // clear interrupt flag
            System_WriteBufferEmpty(&NRF_GPIOTE->EVENTS_PORT);
            sd_nvic_ClearPendingIRQ(GPIOTE_IRQn);
            return; // don't suspend
         }

         // reconfigure suspend pin
         NRF_GPIO->PIN_CNF[SERIAL_ASYNC_PIN_SUSPEND] = (GPIO_PIN_CNF_DIR_Input << GPIO_PIN_CNF_DIR_Pos) |
                                                       (GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos) |
                                                       (GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos) |
                                                       (GPIO_PIN_CNF_DRIVE_S0S1 << GPIO_PIN_CNF_DRIVE_Pos) |
                                                       (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos);

         // reconfigure sleep pin
         NRF_GPIO->PIN_CNF[SERIAL_ASYNC_PIN_SLEEP] =   (GPIO_PIN_CNF_DIR_Input << GPIO_PIN_CNF_DIR_Pos) |
                                                       (GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos) |
                                                       (GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos) |
                                                       (GPIO_PIN_CNF_DRIVE_S0S1 << GPIO_PIN_CNF_DRIVE_Pos) |
                                                       (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos);

         NRF_GPIOTE->EVENTS_PORT = 0;
         System_WriteBufferEmpty(&NRF_GPIOTE->EVENTS_PORT);
         sd_nvic_ClearPendingIRQ(GPIOTE_IRQn);
         NRF_GPIOTE->INTENSET = GPIOTE_INTENSET_PORT_Enabled << GPIOTE_INTENSET_PORT_Pos;

         // set pin sense on sleep for wakeup from deep sleep
         NRF_GPIO->PIN_CNF[SERIAL_ASYNC_PIN_SLEEP] |= (GPIO_PIN_CNF_SENSE_Low << GPIO_PIN_CNF_SENSE_Pos);
         System_WriteBufferEmpty(&NRF_GPIO->PIN_CNF[SERIAL_ASYNC_PIN_SLEEP]);

         System_SetSuspendSleep(); // set deep sleep flag
         System_DeepSleep(); // enter deep sleep; reset upon wakeup
         /* RESET from this point */
      }

      // by default, clear any event
      if(NRF_GPIOTE->EVENTS_PORT)
         NRF_GPIOTE->EVENTS_PORT = 0;
#endif //!ASYNCHRONOUS_DISABLE && !PWRSAVE_DISABLE
   }

   sd_nvic_ClearPendingIRQ(GPIOTE_IRQn); // clear interrupt flag
}

/**
 * @brief ANT event handler used by serial interface
 */
void Serial_ANTEventHandler(uint8_t ucEvent, ANT_MESSAGE *pstANTMessage)
{
   if ((ucEvent == EVENT_TRANSFER_NEXT_DATA_BLOCK) || (ucEvent == EVENT_TRANSFER_TX_COMPLETED) || (ucEvent == EVENT_TRANSFER_TX_FAILED) || (ucEvent == EVENT_QUE_OVERFLOW))
   {
      // if a burst transfer process was queued
      if (ucQueuedTxBurstChannel != 0xFF)
      {
         if ((ucQueuedTxBurstChannel == (pstANTMessage->ANT_MESSAGE_ucChannel & CHANNEL_NUMBER_MASK)) || // event and queued channel number matches
             (ucEvent == EVENT_QUE_OVERFLOW)) // queue overflow (channel events might be lost)
         {
            ucQueuedTxBurstChannel = 0xFF;
            Serial_ReleaseRx(); // release input buffer
         }
      }

      if (ucEvent == EVENT_TRANSFER_NEXT_DATA_BLOCK)
      {
         pstANTMessage->ANT_MESSAGE_ucSize = 0; // do not send out this event message
      }
      else if (ucEvent == EVENT_TRANSFER_TX_FAILED)
      {
         ucBurstSequence = 0; // reset input burst sequence upon tx burst failure
      }
   }

}

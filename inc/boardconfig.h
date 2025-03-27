/*
This software is subject to the license described in the LICENSE_A+SS.txt file
included with this software distribution. You may not use this file except in compliance
with this license.

Copyright (c) Garmin Canada Inc. 2019
All rights reserved.
*/

#ifndef BOARDCONFIG_H
#define BOARDCONFIG_H

// Common Board Configuration
#define APP_RTC_CLOCK_FREQ                            32768
#define APP_RTC_MAX_COUNTER                           0xFFFFFF
#define APP_RTC_PRESCALER                             0 // counter resolution 30.517us

#define APP_NRF_RTC                                   NRF_RTC1
#define APP_NRF_RTC_IRQN                              RTC1_IRQn
#define APP_NRF_RTC_PERPOWER_MSK                      POWER_PERPOWER_RTC1_Msk
#define APP_NRF_RTC_PERRDY_MSK                        POWER_PERRDY_RTC1_Msk

// Used for Serial wake-up
#define SERIAL_POLL_NRF_RTC                           APP_NRF_RTC
#define SERIAL_POLL_NRF_RTC_CC_INDEX                  3 // using cc[3]
#define SERIAL_POLL_NRF_RTC_INTENSET_COMPARE_SET      (RTC_INTENSET_COMPARE3_Set << RTC_INTENSET_COMPARE3_Pos)
#define SERIAL_POLL_NRF_RTC_INTENCLR_COMPARE_CLEAR    (RTC_INTENCLR_COMPARE3_Clear << RTC_INTENCLR_COMPARE3_Pos)
#define SERIAL_POLL_NRF_RTC_EVTENSET_COMPARE_SET      (RTC_EVTENSET_COMPARE3_Set << RTC_EVTENSET_COMPARE3_Pos)
#define SERIAL_POLL_NRF_RTC_EVTENCLR_COMPARE_CLEAR    (RTC_EVTENCLR_COMPARE3_Clear << RTC_EVTENCLR_COMPARE3_Pos)

#if defined (NRF52_N548_CONFIG)

   // Initial pin config
       #define PIN_DIR_INIT                      (0UL)       // TODO:
       #define PIN_OUT_INIT                      (0UL)       // TODO:

   // Serial Communication Configuration
//   #define ASYNCHRONOUS_DISABLE                        // disable asynchronous communication to host (UART)
//   #define SYNCHRONOUS_DISABLE                         // disable synchronous communication to host (SPI)
   #if defined (D52Q_MODULE) || defined(D52Q_PREMIUM_MODULE)
      #define SERIAL_PIN_PORTSEL                (8UL)    // serial mode detection. low =  Asynchronous, high = Synchronous
   #elif defined(D52M_MODULE) || defined(D52M_PREMIUM_MODULE)
      #define SERIAL_PIN_PORTSEL                (2UL)    // serial mode detection. low =  Asynchronous, high = Synchronous
   #else
      #define SERIAL_PIN_PORTSEL                (3UL)    // serial mode detection. low =  Asynchronous, high = Synchronous
   #endif

   // Async Serial
   #if !defined (ASYNCHRONOUS_DISABLE)
      #if defined (D52Q_MODULE) || defined(D52Q_PREMIUM_MODULE)
       #define SERIAL_ASYNC_PIN_RTS           (12UL)     // out
       #define SERIAL_ASYNC_PIN_TXD           (17UL)     // out
       #define SERIAL_ASYNC_PIN_RXD           (16UL)     // in
       #define SERIAL_ASYNC_PIN_SLEEP         (7UL)      // in
       #define SERIAL_ASYNC_PIN_SUSPEND       (6UL)      // in
       #define SERIAL_ASYNC_BR1               (15UL)     // baud rate pin 1 selector
       #define SERIAL_ASYNC_BR2               (11UL)     // baud rate pin 2 selector
       #define SERIAL_ASYNC_BR3               (14UL)     // baud rate pin 3 selector
     #elif defined(D52M_MODULE) || defined(D52M_PREMIUM_MODULE)
       #define SERIAL_ASYNC_PIN_RTS           (31UL)     // out
       #define SERIAL_ASYNC_PIN_TXD           (14UL)     // out
       #define SERIAL_ASYNC_PIN_RXD           (8UL)      // in
       #define SERIAL_ASYNC_PIN_SLEEP         (28UL)     // in
       #define SERIAL_ASYNC_PIN_SUSPEND       (22UL)     // in
       #define SERIAL_ASYNC_BR1               (3UL)      // baud rate pin 1 selector
       #define SERIAL_ASYNC_BR2               (24UL)     // baud rate pin 2 selector
       #define SERIAL_ASYNC_BR3               (6UL)      // baud rate pin 3 selector
     #elif defined(XIAO_NRF52840)
       #define SERIAL_ASYNC_NRF_P1
       #define SERIAL_ASYNC_PIN_RTS           (29UL)     // out
       #define SERIAL_ASYNC_PIN_TXD           (11UL)     // out !P1
       #define SERIAL_ASYNC_PIN_RXD           (12UL)     // in  !P1
       #define SERIAL_ASYNC_PIN_SLEEP         (2UL)      // in
       #define SERIAL_ASYNC_PIN_SUSPEND       (28UL)     // in
       #define SERIAL_ASYNC_BR1               (14UL)     // baud rate pin 1 selector !P1
       #define SERIAL_ASYNC_BR2               (13UL)     // baud rate pin 2 selector !P1
       #define SERIAL_ASYNC_BR3               (15UL)     // baud rate pin 3 selector !P1
       #define LED_RED_PIN                    (26UL)     // out
       #define LED_GREEN_PIN                  (30UL)     // out
       #define LED_BLUE_PIN                   (6UL)      // out
     #else // Pins used for the Nordic DK boards
       #define SERIAL_ASYNC_PIN_RTS           (5UL)      // out
       #define SERIAL_ASYNC_PIN_TXD           (15UL)     // out
       #define SERIAL_ASYNC_PIN_RXD           (12UL)     // in
       #define SERIAL_ASYNC_PIN_SLEEP         (2UL)      // in
       #define SERIAL_ASYNC_PIN_SUSPEND       (23UL)     // in
       #define SERIAL_ASYNC_BR1               (6UL)      // baud rate pin 1 selector
       #define SERIAL_ASYNC_BR2               (24UL)     // baud rate pin 2 selector
       #define SERIAL_ASYNC_BR3               (9UL)      // baud rate pin 3 selector
     #endif // D52Q_MODULE
  #endif //!ASYNCHRONOUS_DISABLE

   // Sync Serial
   #if !defined (SYNCHRONOUS_DISABLE)
     #if defined (D52Q_MODULE) || defined(D52Q_PREMIUM_MODULE)
        #define SERIAL_SYNC_PIN_SMSGRDY        (7UL)     // in
        #define SERIAL_SYNC_PIN_SRDY           (6UL)     // in
        #define SERIAL_SYNC_PIN_SEN            (12UL)    // out
        #define SERIAL_SYNC_PIN_SIN            (16UL)    // in
        #define SERIAL_SYNC_PIN_SOUT           (17UL)    // out
        #define SERIAL_SYNC_PIN_SCLK           (11UL)    // out
  //    #define SERIAL_SYNC_PIN_SFLOW          (15UL)    // in. Bit synchronous not supported
        #define SERIAL_SYNC_PIN_BR3            (14UL)    // bit rate pin selector
     #elif defined(D52M_MODULE) || defined(D52M_PREMIUM_MODULE)
        #define SERIAL_SYNC_PIN_SMSGRDY        (28UL)    // in
        #define SERIAL_SYNC_PIN_SRDY           (22UL)    // in
        #define SERIAL_SYNC_PIN_SEN            (31UL)    // out
        #define SERIAL_SYNC_PIN_SIN            (8UL)     // in
        #define SERIAL_SYNC_PIN_SOUT           (14UL)    // out
        #define SERIAL_SYNC_PIN_SCLK           (24UL)    // out
  //    #define SERIAL_SYNC_PIN_SFLOW          (3UL)     // in. Bit synchronous not supported
        #define SERIAL_SYNC_PIN_BR3            (6UL)     // bit rate pin selector
     #elif defined(XIAO_NRF52840)
        #define SERIAL_SYNC_NRF_P1
        #define SERIAL_SYNC_PIN_SMSGRDY        (2UL)     // in
        #define SERIAL_SYNC_PIN_SRDY           (28UL)    // in
        #define SERIAL_SYNC_PIN_SEN            (29UL)    // out
        #define SERIAL_SYNC_PIN_SIN            (14UL)    // in !P1
        #define SERIAL_SYNC_PIN_SOUT           (15UL)    // out !P1
        #define SERIAL_SYNC_PIN_SCLK           (13UL)    // out !P1
  //    #define SERIAL_SYNC_PIN_SFLOW          (11UL)    // in. Bit synchronous not supported !P1
        #define SERIAL_SYNC_PIN_BR3            (12UL)    // bit rate pin selector !P1
     #else // Pins used for the Nordic DK boards
        #define SERIAL_SYNC_PIN_SMSGRDY        (2UL)     // in
        #define SERIAL_SYNC_PIN_SRDY           (23UL)    // in
        #define SERIAL_SYNC_PIN_SEN            (5UL)     // out
        #define SERIAL_SYNC_PIN_SIN            (12UL)    // in
        #define SERIAL_SYNC_PIN_SOUT           (15UL)    // out
        #define SERIAL_SYNC_PIN_SCLK           (24UL)    // out
  //    #define SERIAL_SYNC_PIN_SFLOW          (6UL)     // in. Bit synchronous not supported
        #define SERIAL_SYNC_PIN_BR3            (9UL)     // bit rate pin selector
     #endif // D52Q_MODULE
   #endif //!SYNCHRONOUS_DISABLE

#else
   #error undefined board configuration

#endif

//Baudrate Capabilities
//#define BAUD1200_UNSUPPORTED
//#define BAUD2400_UNSUPPORTED
//#define BAUD4800_UNSUPPORTED
//#define BAUD9600_UNSUPPORTED
//#define BAUD19200_UNSUPPORTED
//#define BAUD38400_UNSUPPORTED
#define BAUD50000_UNSUPPORTED
//#define BAUD57600_UNSUPPORTED
//#define BAUD115200_UNSUPPORTED
//#define BAUD230400_UNSUPPORTED
//#define BAUD460800_UNSUPPORTED
//#define BAUD921600_UNSUPPORTED

//Bit rate Capabilities
//#define BIT_RATE_K500_UNSUPPORTED
//#define BIT_RATE_M1_UNSUPPORTED
//#define BIT_RATE_M2_UNSUPPORTED
//#define BIT_RATE_M4_UNSUPPORTED
//#define BIT_RATE_M8_UNSUPPORTED

#endif // BOARDCONFIG_H

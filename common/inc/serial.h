/*
This software is subject to the license described in the LICENSE_A+SS.txt file
included with this software distribution. You may not use this file except in compliance
with this license.

Copyright (c) Garmin Canada Inc. 2019
All rights reserved.
*/

#ifndef SERIAL_H_
#define SERIAL_H_

#include <stdbool.h>
#include <stdint.h>
#include "ant_parameters.h"
#include "appconfig.h"
#include "boardconfig.h"

#define SERIAL_SLEEP_POLLING_MODE // enable serial sleeping mechanism

#define SERIAL_RX_BUFFER_SIZE        (MESG_MAX_DATA_SIZE + MESG_ID_SIZE)

//////////////////////////////////////////////
/* Supported Async Baudrate Bitfield
*/
//////////////////////////////////////////////
#define BAUD1200_BITFIELD_Pos                 ((uint16_t)0)  // 1200 baud bitfield position.
#define BAUD2400_BITFIELD_Pos                 ((uint16_t)1)  // 2400 baud bitfield position.
#define BAUD4800_BITFIELD_Pos                 ((uint16_t)2)  // 4800 baud bitfield position.
#define BAUD9600_BITFIELD_Pos                 ((uint16_t)3)  // 9600 baud bitfield position.
#define BAUD19200_BITFIELD_Pos                ((uint16_t)4)  // 19200 baud bitfield position.
#define BAUD38400_BITFIELD_Pos                ((uint16_t)5)  // 38400 baud bitfield position.
#define BAUD50000_BITFIELD_Pos                ((uint16_t)6)  // 50000 baud bitfield position.
#define BAUD57600_BITFIELD_Pos                ((uint16_t)7)  // 57600 baud bitfield position.
#define BAUD115200_BITFIELD_Pos               ((uint16_t)8)  // 115200 baud bitfield position.
#define BAUD230400_BITFIELD_Pos               ((uint16_t)9)  // 230400 baud bitfield position.
#define BAUD460800_BITFIELD_Pos               ((uint16_t)10) // 460800 baud bitfield position.
#define BAUD921600_BITFIELD_Pos               ((uint16_t)11) // 921600 baud bitfield position.

#define BAUD_UNSUPPORTED                      ((uint16_t)0x00)
#define BAUD_SUPPORTED                        ((uint16_t)0x01)

#if defined (BAUD1200_UNSUPPORTED)
    #define BAUD1200_SUPPORTED_VALUE          (uint16_t)(BAUD_UNSUPPORTED << BAUD1200_BITFIELD_Pos)
#else
    #define BAUD1200_SUPPORTED_VALUE          (uint16_t)(BAUD_SUPPORTED << BAUD1200_BITFIELD_Pos)
#endif
#if defined (BAUD2400_UNSUPPORTED)
    #define BAUD2400_SUPPORTED_VALUE          (uint16_t)(BAUD_UNSUPPORTED << BAUD2400_BITFIELD_Pos)
#else
    #define BAUD2400_SUPPORTED_VALUE          (uint16_t)(BAUD_SUPPORTED << BAUD2400_BITFIELD_Pos)
#endif
#if defined (BAUD4800_UNSUPPORTED)
    #define BAUD4800_SUPPORTED_VALUE          (uint16_t)(BAUD_UNSUPPORTED << BAUD4800_BITFIELD_Pos)
#else
    #define BAUD4800_SUPPORTED_VALUE          (uint16_t)(BAUD_SUPPORTED << BAUD4800_BITFIELD_Pos)
#endif
#if defined (BAUD9600_UNSUPPORTED)
    #define BAUD9600_SUPPORTED_VALUE          (uint16_t)(BAUD_UNSUPPORTED << BAUD9600_BITFIELD_Pos)
#else
    #define BAUD9600_SUPPORTED_VALUE          (uint16_t)(BAUD_SUPPORTED << BAUD9600_BITFIELD_Pos)
#endif
#if defined (BAUD19200_UNSUPPORTED)
    #define BAUD19200_SUPPORTED_VALUE         (uint16_t)(BAUD_UNSUPPORTED << BAUD19200_BITFIELD_Pos)
#else
    #define BAUD19200_SUPPORTED_VALUE         (uint16_t)(BAUD_SUPPORTED << BAUD19200_BITFIELD_Pos)
#endif
#if defined (BAUD38400_UNSUPPORTED)
    #define BAUD38400_SUPPORTED_VALUE         (uint16_t)(BAUD_UNSUPPORTED << BAUD38400_BITFIELD_Pos)
#else
    #define BAUD38400_SUPPORTED_VALUE         (uint16_t)(BAUD_SUPPORTED << BAUD38400_BITFIELD_Pos)
#endif
#if defined (BAUD50000_UNSUPPORTED)
    #define BAUD50000_SUPPORTED_VALUE         (uint16_t)(BAUD_UNSUPPORTED << BAUD50000_BITFIELD_Pos)
#else
    #define BAUD50000_SUPPORTED_VALUE         (uint16_t)(BAUD_SUPPORTED << BAUD50000_BITFIELD_Pos)
#endif
#if defined (BAUD57600_UNSUPPORTED)
    #define BAUD57600_SUPPORTED_VALUE         (uint16_t)(BAUD_UNSUPPORTED << BAUD57600_BITFIELD_Pos)
#else
    #define BAUD57600_SUPPORTED_VALUE         (uint16_t)(BAUD_SUPPORTED << BAUD57600_BITFIELD_Pos)
#endif
#if defined (BAUD115200_UNSUPPORTED)
    #define BAUD115200_SUPPORTED_VALUE        (uint16_t)(BAUD_UNSUPPORTED << BAUD115200_BITFIELD_Pos)
#else
    #define BAUD115200_SUPPORTED_VALUE        (uint16_t)(BAUD_SUPPORTED << BAUD115200_BITFIELD_Pos)
#endif
#if defined (BAUD230400_UNSUPPORTED)
    #define BAUD230400_SUPPORTED_VALUE        (uint16_t)(BAUD_UNSUPPORTED << BAUD230400_BITFIELD_Pos)
#else
    #define BAUD230400_SUPPORTED_VALUE        (uint16_t)(BAUD_SUPPORTED << BAUD230400_BITFIELD_Pos)
#endif
#if defined (BAUD460800_UNSUPPORTED)
    #define BAUD460800_SUPPORTED_VALUE        (uint16_t)(BAUD_UNSUPPORTED << BAUD460800_BITFIELD_Pos)
#else
    #define BAUD460800_SUPPORTED_VALUE        (uint16_t)(BAUD_SUPPORTED << BAUD460800_BITFIELD_Pos)
#endif
#if defined (BAUD921600_UNSUPPORTED)
    #define BAUD921600_SUPPORTED_VALUE        (uint16_t)(BAUD_UNSUPPORTED << BAUD921600_BITFIELD_Pos)
#else
    #define BAUD921600_SUPPORTED_VALUE        (uint16_t)(BAUD_SUPPORTED << BAUD921600_BITFIELD_Pos)
#endif

#define BAUD_BITFIELD_SIZE                    ((uint8_t)12)
#define BAUD_SUPPORTED_BITFIELD               (uint16_t)(BAUD1200_SUPPORTED_VALUE | BAUD2400_SUPPORTED_VALUE |\
                                              BAUD4800_SUPPORTED_VALUE | BAUD9600_SUPPORTED_VALUE |\
                                              BAUD19200_SUPPORTED_VALUE | BAUD38400_SUPPORTED_VALUE |\
                                              BAUD50000_SUPPORTED_VALUE | BAUD57600_SUPPORTED_VALUE |\
                                              BAUD115200_SUPPORTED_VALUE | BAUD230400_SUPPORTED_VALUE |\
                                              BAUD460800_SUPPORTED_VALUE | BAUD921600_SUPPORTED_VALUE)

//////////////////////////////////////////////
/* Supported Sync Bit rate Bitfield
*/
//////////////////////////////////////////////
#define BIT_RATE_K500_BITFIELD_Pos            ((uint16_t)0)  // K500 bit rate bitfield position.
#define BIT_RATE_M1_BITFIELD_Pos              ((uint16_t)1)  // M1 bit rate bitfield position.
#define BIT_RATE_M2_BITFIELD_Pos              ((uint16_t)2)  // M2 bit rate bitfield position.
#define BIT_RATE_M4_BITFIELD_Pos              ((uint16_t)3)  // M4 bit rate bitfield position.
#define BIT_RATE_M8_BITFIELD_Pos              ((uint16_t)4)  // M8 bit rate bitfield position.

#define BIT_RATE_UNSUPPORTED                  ((uint16_t)0x00)
#define BIT_RATE_SUPPORTED                    ((uint16_t)0x01)

#if defined (BIT_RATE_K500_UNSUPPORTED)
    #define BIT_RATE_K500_SUPPORTED_VALUE     (uint16_t)(BIT_RATE_UNSUPPORTED << BIT_RATE_K500_BITFIELD_Pos)
#else
    #define BIT_RATE_K500_SUPPORTED_VALUE     (uint16_t)(BIT_RATE_SUPPORTED << BIT_RATE_K500_BITFIELD_Pos)
#endif
#if defined (BIT_RATE_M1_UNSUPPORTED)
    #define BIT_RATE_M1_SUPPORTED_VALUE       (uint16_t)(BIT_RATE_UNSUPPORTED << BIT_RATE_M1_BITFIELD_Pos)
#else
    #define BIT_RATE_M1_SUPPORTED_VALUE       (uint16_t)(BIT_RATE_SUPPORTED << BIT_RATE_M1_BITFIELD_Pos)
#endif
#if defined (BIT_RATE_M2_UNSUPPORTED)
    #define BIT_RATE_M2_SUPPORTED_VALUE       (uint16_t)(BIT_RATE_UNSUPPORTED << BIT_RATE_M2_BITFIELD_Pos)
#else
    #define BIT_RATE_M2_SUPPORTED_VALUE       (uint16_t)(BIT_RATE_SUPPORTED << BIT_RATE_M2_BITFIELD_Pos)
#endif
#if defined (BIT_RATE_M4_UNSUPPORTED)
    #define BIT_RATE_M4_SUPPORTED_VALUE       (uint16_t)(BIT_RATE_UNSUPPORTED << BIT_RATE_M4_BITFIELD_Pos)
#else
    #define BIT_RATE_M4_SUPPORTED_VALUE       (uint16_t)(BIT_RATE_SUPPORTED << BIT_RATE_M4_BITFIELD_Pos)
#endif
#if defined (BIT_RATE_M8_UNSUPPORTED)
    #define BIT_RATE_M8_SUPPORTED_VALUE       (uint16_t)(BIT_RATE_UNSUPPORTED << BIT_RATE_M8_BITFIELD_Pos)
#else
    #define BIT_RATE_M8_SUPPORTED_VALUE       (uint16_t)(BIT_RATE_SUPPORTED << BIT_RATE_M8_BITFIELD_Pos)
#endif

#define BIT_RATE_BITFIELD_SIZE               ((uint8_t)5)
#define BIT_RATE_SUPPORTED_BITFIELD          (uint16_t)( BIT_RATE_K500_SUPPORTED_VALUE | BIT_RATE_M1_SUPPORTED_VALUE |\
                                                         BIT_RATE_M2_SUPPORTED_VALUE | BIT_RATE_M4_SUPPORTED_VALUE |\
                                                         BIT_RATE_M8_SUPPORTED_VALUE)

/**
 * @brief Serial interface initialization
 */
void Serial_Init(void);

/**
 * @brief Set byte synchronous serial interface bit rate
 */
uint8_t Serial_SetByteSyncSerialBitRate(uint8_t ucConfig);

/**
 * @brief Set byte synchronous serial interface SRDY sleep delay
 */
uint8_t Serial_SetByteSyncSerialSRDYSleep(uint8_t ucDelay);

/**
 * @brief Get input message buffer
 */
ANT_MESSAGE *Serial_GetRxMesgPtr(void);

/**
 * @brief Get output message buffer
 */
ANT_MESSAGE *Serial_GetTxMesgPtr(void);

/**
 * @brief Set baudrate
 */
uint8_t Serial_SetAsyncBaudrate(BAUDRATE_TYPE baud);

/**
 * @brief Activate previously set baudrate
 */
void Serial_ActivateAsyncBaudrate(void);

/**
 * @brief Hold incoming serial communication
 */
void Serial_HoldRx(void);

/**
 * @brief Allow incoming serial communication
 */
void Serial_ReleaseRx(void);

/**
 * @brief Send serial message
 */
void Serial_TxMessage(void);

/**
 * @brief Receive serial message
 */
bool Serial_RxMessage(void);

/**
 * @brief Serial interface sleep handler
 */
void Serial_Sleep(void);

/**
 * @brief Interrupt handler for synchronous serial SMSGRDY and SRDY interrupt. Uses GPIOTE 0 and 1
 */
#define SERIAL_SYNC_GPIOTE_EVENT_SMSGRDY  0  // assigned GPIOTE 0 for SMSGRDY
#define SERIAL_SYNC_GPIOTE_EVENT_SRDY     1  // assigned GPIOTE 1 for SRDY
void Serial_GPIOTE_IRQHandler(void);

/**
 * @brief Interrupt handler for asynchronous serial interface
 */
void Serial_UART0_IRQHandler(void);

/**
 * @brief ANT event handler used by serial interface
 */
void Serial_ANTEventHandler(uint8_t ucEventType, ANT_MESSAGE *pstANTMessage);

#endif /* SERIAL_H_ */

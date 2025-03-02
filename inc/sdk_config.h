#ifndef SDK_CONFIG_H
#define SDK_CONFIG_H

#ifdef USE_APP_CONFIG
#include "appconfig.h"
#endif

// <e> SPI_ENABLED - nrf_drv_spi - SPI/SPIM peripheral driver
//==========================================================
#ifndef SPI_ENABLED
#define SPI_ENABLED 1
#endif
#if  SPI_ENABLED
// <e> SPI_CONFIG_LOG_ENABLED - Enables logging in the module.
//==========================================================
#ifndef SPI_CONFIG_LOG_ENABLED
#define SPI_CONFIG_LOG_ENABLED 0
#endif
#if  SPI_CONFIG_LOG_ENABLED
// <o> SPI_CONFIG_LOG_LEVEL  - Default Severity level

// <0=> Off
// <1=> Error
// <2=> Warning
// <3=> Info
// <4=> Debug

#ifndef SPI_CONFIG_LOG_LEVEL
#define SPI_CONFIG_LOG_LEVEL 3
#endif

// <o> SPI_CONFIG_INFO_COLOR  - ANSI escape code prefix.

// <0=> Default
// <1=> Black
// <2=> Red
// <3=> Green
// <4=> Yellow
// <5=> Blue
// <6=> Magenta
// <7=> Cyan
// <8=> White

#ifndef SPI_CONFIG_INFO_COLOR
#define SPI_CONFIG_INFO_COLOR 0
#endif

// <o> SPI_CONFIG_DEBUG_COLOR  - ANSI escape code prefix.

// <0=> Default
// <1=> Black
// <2=> Red
// <3=> Green
// <4=> Yellow
// <5=> Blue
// <6=> Magenta
// <7=> Cyan
// <8=> White

#ifndef SPI_CONFIG_DEBUG_COLOR
#define SPI_CONFIG_DEBUG_COLOR 0
#endif

#endif //SPI_CONFIG_LOG_ENABLED
// </e>

// <o> SPI_DEFAULT_CONFIG_IRQ_PRIORITY  - Interrupt priority


// <i> Priorities 0,2 (nRF51) and 0,1,4,5 (nRF52) are reserved for SoftDevice
// <0=> 0 (highest)
// <1=> 1
// <2=> 2
// <3=> 3

#ifndef SPI_DEFAULT_CONFIG_IRQ_PRIORITY
#define SPI_DEFAULT_CONFIG_IRQ_PRIORITY 3
#endif

// <e> SPI0_ENABLED - Enable SPI0 instance
//==========================================================
#ifndef SPI0_ENABLED
#define SPI0_ENABLED 1
#endif
#if  SPI0_ENABLED
// <q> SPI0_USE_EASY_DMA  - Use EasyDMA


#ifndef SPI0_USE_EASY_DMA
#define SPI0_USE_EASY_DMA 0
#endif

#endif //SPI0_ENABLED
// </e>

// <e> SPI1_ENABLED - Enable SPI1 instance
//==========================================================
#ifndef SPI1_ENABLED
#define SPI1_ENABLED 0
#endif
#if  SPI1_ENABLED
// <q> SPI1_USE_EASY_DMA  - Use EasyDMA


#ifndef SPI1_USE_EASY_DMA
#define SPI1_USE_EASY_DMA 0
#endif

#endif //SPI1_ENABLED
// </e>

// <e> SPI2_ENABLED - Enable SPI2 instance
//==========================================================
#ifndef SPI2_ENABLED
#define SPI2_ENABLED 0
#endif
#if  SPI2_ENABLED
// <q> SPI2_USE_EASY_DMA  - Use EasyDMA


#ifndef SPI2_USE_EASY_DMA
#define SPI2_USE_EASY_DMA 0
#endif

#endif //SPI2_ENABLED
// </e>

#endif //SPI_ENABLED
// </e>


#endif //SDK_CONFIG_H


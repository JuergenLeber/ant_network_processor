/*
This software is subject to the license described in the LICENSE_A+SS.txt file
included with this software distribution. You may not use this file except in compliance
with this license.

Copyright (c) Garmin Canada Inc. 2019
All rights reserved.
*/

/*
 * NOTES:
 *
 * version "AAA#.##B##"
 *
 * SW_VER_MAJOR   - Increases on any released applicaion major feature update/changes or new features
 * SW_VER_MINOR   - Increases on any release application minor feature update i.e. Bug fixing and minor features.
 * SW_VER_PREFIX  - Is fixed on this firmware.
 * SW_VER_POSTFIX - Increases on any internal development builds. OR might be used for tagging special builds. OR might be used on branch build
  */

#if defined (NRF52_N548_CONFIG)
      #define     SW_VER_MAJOR      "2."
      #define     SW_VER_MINOR      "00"

      #define     SW_VER_PREFIX     "BLP" // NRF52 ANT module network processor external reference design
      #define     SW_VER_POSTFIX    "B00"
#else
   #error undefined application version

#endif

/***************************************************************************
*/
const char acAppVersion[] = SW_VER_PREFIX SW_VER_MAJOR SW_VER_MINOR SW_VER_POSTFIX;  // Max 11 characters including null


[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
# ant_network_processor 
The ANT Network Processor reference design is provided as a drop-in example for the Nordic nRF5 SDK. You can find the original and unmodified source [here](https://www.thisisant.com/resources/nrf52-network-processor-source/).

Close this repository into the 'examples\ant' folder of the nRF5 SDK obtained from Nordic Semiconductor. Header files for the ANT SoftDevice should be copied to the appropriate folder in 'components\softdevice\'. Detailed instructions below.

SDK Compatibility:
- nRF5 SDK v17.1.0

Supported SoftDevice:
- S212 v7.0.1 (ANT_s212_nrf52_7.0.1)

Supported Modules:
- nRF52832
  - Nordic DK boards (generic)

- nRF52840
  - Nordic DK boards (generic)
  - Seeed Studio XIAO nRF52840 (Sense)

- D52 ANT SoC Module Series
  - D52Q
  - D52M
  - D52Q Premium
  - D52M Premium

## How to compile
### Prerequisites
In order to be able to download the required ANT+ capable SoftDevice, we need to register an account at [thisisant.com](https://www.thisisant.com/register/).

Unfortunately the Keil ARM MDK and especially Keil µVision are Windows-only software. You can use tools like [Wine](https://www.winehq.org) or [Crossover](https://www.codeweavers.com/crossover) to run the software on Mac OS or Linux. If you're using an Apple Silicon Mac you can also use [Whisky](https://github.com/Whisky-App/Whisky) which is free and works perfectly for this purpose.

### Required downloads
- Keil MDK v5.36 (last version with Arm Compiler 5 and 6) [(here)](https://armkeil.blob.core.windows.net/eval/MDK536.EXE)
- Nordic nRF5 SDK v17.1.0 [(here)](https://www.nordicsemi.com/Products/Development-software/nRF5-SDK/Download#infotabs)
- SoftDevice S212 v7.0.1 [(here)](https://www.thisisant.com/developer/components/nrf52832#tab_protocol_stacks_tab)

### Software installation
- Install Keil MDK v5.36 to the default folder `C:\Keil_v5` as it will make problems with longer folder names and/or spaces or special characters in the folder name.
  - After successfull installation it automatically starts the "Pack Installer" which then downloads all the pack information. This may take a while, just let it do. 
  - If it detects any updates you can install them (sort by "Action" and click "Update"). 
- Extract the nRF5 SDK v17.1.0 (extracts to a folder `nRF5_SDK_17.1.0_ddde560`) and move it to the folder `C:\Nordic\SDK`.  
- Extract the SoftDevice S212 v7.0.1 (extracts to a folder `ANT_s212_nrf52_7.0.1`). Besides other files it contains:
  - `ANT_s212_nrf52_7.0.1.hex` -> Move this file to the folder `C:\Nordic\SDK\nRF5_SDK_17.1.0_ddde560\components\softdevice\s212\hex` (create the folder before)
  - The folder `ANT_s212_nrf52_7.0.1.API\include` -> Move the **contents** (not the 'include' folder itself!) including subfolders to the folder `C:\Nordic\SDK\nRF5_SDK_17.1.0_ddde560\components\softdevice\s212\headers`  
- Clone this repository into the `examples\ant` folder of the nRF5 SDK so it will reside in `C:\Nordic\SDK\nRF5_SDK_17.1.0_ddde560\examples\ant\ant_network_processor`.
- Edit the file `C:\Nordic\SDK\nRF5_SDK_17.1.0_ddde560\components\softdevice\s212\headers\nrf_sdm.h` and uncomment the line #191 to use the evaluation ANT_LICENSE_KEY.

### Compilation
- Open Keil µVision5
- Go to Project -> Open Project and open the project file `application_n5x.uvprojx` in `C:\Nordic\SDK\nRF5_SDK_17.1.0_ddde560\examples\ant\ant_network_processor`
- Keil µVision5 will complain that at least one device wasn't found and that we should update our device selection. We just click 'OK' here and it will launch the 'Pack Installer' with the necessary information. Just follow the instructions and it will install everything that is needed.
- If you have updated CMSIS in one of the steps before: Go to Project -> Manage -> 'Select Software Packs' and uncheck 'Use latest versions of all installed Software Packs'. Then select ARM::CMSIS Version 5.8.0 instead of 6.1.0. You have to do this for every target configuration.
- Select the corresponding target in the drop down box in the menu ('d52q module', 'd52m_module', etc.)
- Finally go to Project -> 'Build target' and it should compile everything without errors.

### Packaging
- Install nrfutil as described [here](https://docs.nordicsemi.com/bundle/nrfutil/page/guides/installing.html#installing-nrf-util-from-the-web-default)
- Install the 'nrf5sdk-tools' by executing `nrfutil install nrf5sdk-tools`
- Build the desired target and write down the full path to the `application_{target}.hex` file
- Additionally, write down the full path to the SoftDevice S212 file `ANT_s212_nrf52_7.0.1.hex`
- Create the DFU package by executing e.g. ```nrfutil nrf5sdk-tools pkg generate --hw-version 52 --sd-req 0xEE --sd-id=0xEE --softdevice ../../../../components/softdevice/s212/hex/ANT_s212_nrf52_7.0.1.hex --application application_{target}.hex --application-version 2 application_{target}.zip``` (of course adjust the file names and paths accordingly)

## [Copyright notice](LICENSE_A+SS.txt)
```
This software is subject to the ANT+ Shared Source License
www.thisisant.com/swlicenses
Copyright (c) Garmin Canada Inc. 2019
All rights reserved.
```

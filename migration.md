# Migration of the source code 
The original and unmodified [source](https://www.thisisant.com/resources/nrf52-network-processor-source/) of the `ant_network_processor` unfortunately only builds on Windows operating systems with Keil µVision5 which is part of the [Keil MDK](https://www.keil.arm.com/keil-mdk/). This document shows how the migration from Keil µVision5 to Arm Keil Studio has been done.

## Prerequisites
In order to be able to download the required ANT+ capable SoftDevices, you need to register an account at [thisisant.com](https://www.thisisant.com/register/). After one business day you will receive access to the resources there.

Besides that you need to have [Visual Studio Code](https://code.visualstudio.coma) with the [Arm Keil Studio Pack (MDK v6)](https://marketplace.visualstudio.com/items?itemName=Arm.keil-studio-pack) Extension 


## How compiling the original source code works
As Keil µVision is a Windows-only software you need to use tools like [Wine](https://www.winehq.org) or [Crossover](https://www.codeweavers.com/crossover) to run the software on Mac OS or Linux. If you're using an Apple Silicon Mac you can also use [Whisky](https://github.com/Whisky-App/Whisky) which is free and works perfectly for this purpose.

### Required downloads
- Keil MDK v5.36 (last version with Arm Compiler 5 and 6) [(here)](https://armkeil.blob.core.windows.net/eval/MDK536.EXE)
- Nordic nRF5 SDK v17.1.0 [(here)](https://www.nordicsemi.com/Products/Development-software/nRF5-SDK/Download#infotabs) 
- SoftDevice S212 v7.0.1 [(here)](https://www.thisisant.com/developer/components/nrf52832#tab_protocol_stacks_tab)
- ant_network_processor source code [(here)](https://www.thisisant.com/resources/nrf52-network-processor-source/)

### Software installation
- Install Keil MDK v5.36 to the default folder `C:\Keil_v5` as it will make problems with longer folder names and/or spaces or special characters in the folder name.
  - After successfull installation it automatically starts the "Pack Installer" which then downloads all the pack information. This may take a while, just let it do. 
  - If it detects any updates you can install them (sort by "Action" and click "Update"). 
- Extract the nRF5 SDK v17.1.0 (extracts to a folder `nRF5_SDK_17.1.0_ddde560`) and move it to the folder `C:\Nordic\SDK`.  
- Extract the SoftDevice S212 v7.0.1 (extracts to a folder `ANT_s212_nrf52_7.0.1`). Besides other files it contains:
  - `ANT_s212_nrf52_7.0.1.hex` -> Move this file to the folder `C:\Nordic\SDK\nRF5_SDK_17.1.0_ddde560\components\softdevice\s212\hex` (create the folder before)
  - The folder `ANT_s212_nrf52_7.0.1.API\include` -> Move the **contents** (not the 'include' folder itself!) including subfolders to the folder `C:\Nordic\SDK\nRF5_SDK_17.1.0_ddde560\components\softdevice\s212\headers`  
- Extract the `ant_network_processor` source code from the downloaded zip file into the `examples\ant` folder of the nRF5 SDK so it will reside in `C:\Nordic\SDK\nRF5_SDK_17.1.0_ddde560\examples\ant\ant_network_processor`
- Edit the file `C:\Nordic\SDK\nRF5_SDK_17.1.0_ddde560\components\softdevice\s212\headers\nrf_sdm.h` and uncomment the line #191 to use the evaluation ANT_LICENSE_KEY or alternatively put in your own ANT license key

### Compilation
- Open Keil µVision5
- Go to Project -> Open Project and open the project file `application_n5x.uvprojx` in `C:\Nordic\SDK\nRF5_SDK_17.1.0_ddde560\examples\ant\ant_network_processor`
- Keil µVision5 will complain that at least one device wasn't found and that we should update our device selection. We just click 'OK' here and it will launch the 'Pack Installer' with the necessary information. Just follow the instructions and it will install everything that is needed
- If you have updated CMSIS in one of the steps before: Go to Project -> Manage -> 'Select Software Packs' and uncheck 'Use latest versions of all installed Software Packs'. Then select ARM::CMSIS Version 5.8.0 instead of 6.1.0. You have to do this for every target configuration
- Select the corresponding target in the drop down box in the menu ('d52q module', 'd52m_module', etc.)
- Finally go to Project -> 'Build target' and it should compile everything without errors

## Migration to Arm Keil Studio
To be able to convert the project from µVision to CMSIS solution you need to switch to ARM compiler V6 first as V5 isn't supported. You have to do the following steps **for every target configuration** in your project:
- Go to Project -> Manage -> 'Select Software Packs' and check 'Use latest versions of all installed Software Packs'
- Click the 'Options for Target...' button to open the options dialog
  - In the tab 'Target' change the 'ARM Compiler' to 'Use default compiler version 6'
  - In the tab 'C/C++ (AC6)' change the value in 'Misc Controls' from `--c99` to `--std=c99`
- Now go to Project -> 'Build target' and it should compile everything without errors
- Open Visual Studio Code and change to the CMSIS view and click the 'three dots' besides the small 'gear' and select 'Convert µVision project to CMSIS solution'
- You have to select the .uvprojx file and then a CMSIS solution will be created
- Click the small 'gear' to manage the solution settings. There you can select the desired target that you want to build
- Click the 'three dots' and select 'Rebuild solution' to build


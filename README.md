[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
# ant_network_processor 
The ANT Network Processor reference design is provided as a drop-in example for the Nordic nRF5 SDK. You can find the original and unmodified source [here](https://www.thisisant.com/resources/nrf52-network-processor-source/). 

This version has been extended and updated to the newest SDKs and compilers. The project can be built either with Keil µVision5 or with the [Arm Keil Studio Pack (MDK v6)](https://marketplace.visualstudio.com/items?itemName=Arm.keil-studio-pack) in Visual Studio Code on Windows, Mac OS and Linux. It additionally supports building for Seeed XIAO nRF52840 boards with a special pinout configuration.

Close this repository into the 'examples\ant' folder of the nRF5 SDK (see [below](#required-downloads)) from Nordic Semiconductor. Header files for the ANT SoftDevice should be copied to the appropriate folder in 'components\softdevice\'. Detailed instructions below.

SDK Compatibility:
- nRF5 SDK v17.1.0

Supported SoftDevice:
- S212 v7.0.1 (ANT_s212_nrf52_7.0.1)
- S340 v7.0.1 (ANT_s340_nrf52_7.0.1)

Supported Modules:
- nRF52832 (S212)
  - Nordic DK boards (generic)

- nRF52840 (S212/S340)
  - Nordic DK boards (generic)
  - Seeed Studio XIAO nRF52840 (Sense)

- D52 ANT SoC Module Series (S212)
  - D52Q
  - D52M
  - D52Q Premium
  - D52M Premium

## Prerequisites
In order to be able to download the required ANT+ capable SoftDevices, you need to register an account at [thisisant.com](https://www.thisisant.com/register/). After one business day you will receive access to the resources there.

### SoftDevices
Depending on the board you want to build for you have to download the appropriate SoftDevice(s) as they are **NOT** included in this repository:
- SoftDevice S212 v7.0.1 [(here)](https://www.thisisant.com/developer/components/nrf52832#tab_protocol_stacks_tab)
- SoftDevice S340 v7.0.1 [(here)](https://www.thisisant.com/developer/components/nrf52832#tab_protocol_stacks_tab)

## How to build
For the build you'll need to have Visual Studio Code with the [Arm Keil Studio Pack (MDK v6)](https://marketplace.visualstudio.com/items?itemName=Arm.keil-studio-pack) installed. 

### Required downloads
- The SoftDevices as described [above](#softdevices)
- Nordic nRF5 SDK v17.1.0 [(here)](https://www.nordicsemi.com/Products/Development-software/nRF5-SDK/Download#infotabs)

### Build
- Extract the nRF5 SDK v17.1.0 (extracts to a folder `nRF5_SDK_17.1.0_ddde560`)   
- Extract the SoftDevice S212 v7.0.1 (extracts to a folder `ANT_s212_nrf52_7.0.1`). Besides other files it contains:
  - `ANT_s212_nrf52_7.0.1.hex` -> Move this file to the folder `nRF5_SDK_17.1.0_ddde560/components/softdevice/s212/hex` (create the folder before)
  - The folder `ANT_s212_nrf52_7.0.1.API/include` -> Move the **contents** (not the 'include' folder itself!) including subfolders to the folder `nRF5_SDK_17.1.0_ddde560/components/softdevice/s212/headers`  
- Clone this repository into the `examples/ant` folder of the nRF5 SDK so it will reside in `nRF5_SDK_17.1.0_ddde560/examples/ant/ant_network_processor`.
- **Important:** Edit the file `nRF5_SDK_17.1.0_ddde560/components/softdevice/s212/headers/nrf_sdm.h` and uncomment the line #191 to use the evaluation ANT_LICENSE_KEY.
- Repeat the same step with the SoftDevice S340 v7.0.1 (to folder `nRF5_SDK_17.1.0_ddde560/components/softdevice/s340` then)
- Open the folder `nRF5_SDK_17.1.0_ddde560/examples/ant/ant_network_processor` in Visual Studio Code and it will automatically detect the CMSIS solution
- Change to the CMSIS view and click the small 'gear' to manage the solution settings. There you can select the desired target that you want to build, e.g. `xiao_nRF52840_s340`
- Click the 'three dots' and select 'Rebuild solution' to build
- The binaries will then be in the folder `nRF5_SDK_17.1.0_ddde560/examples/ant/ant_network_processor/out/application_n5x` with an additional subfolder for the specified target

In case you're experiencing problems with the automatic download of the nRF Device Family Pack you can manually download it [here](https://www.nordicsemi.com/Products/Development-tools/nRF-MDK/Download#infotabs). The version 8.44.1 can directly be downloaded [here](https://nsscprodmedia.blob.core.windows.net/prod/software-and-other-downloads/desktop-software/nrf-mdk/sw/8-44-1/nordicsemiconductor.nrf_devicefamilypack.8.44.1.pack).
To install this pack just open a new terminal **from inside Visual Studio Code** and execute `cpackget add nordicsemiconductor.nrf_devicefamilypack.8.44.1.pack`

### Packaging
- Install nrfutil as described [here](https://docs.nordicsemi.com/bundle/nrfutil/page/guides/installing.html#installing-nrf-util-from-the-web-default)
- Install the 'nrf5sdk-tools' by executing `nrfutil install nrf5sdk-tools`
- Build the desired target and write down the full path to the `application_{target}.hex` file
- Additionally, write down the full path to the SoftDevice S212 file `ANT_s212_nrf52_7.0.1.hex`
- Create the DFU package by executing e.g. ```nrfutil nrf5sdk-tools pkg generate --hw-version 52 --sd-req 0xEE --sd-id=0xEE --softdevice ../../../../components/softdevice/s212/hex/ANT_s212_nrf52_7.0.1.hex --application application_{target}.hex --application-version 2 application_{target}.zip``` (of course adjust the file names and paths accordingly)

### Packaging for UF2 (Seeed XIAO nRF52840 boards with special bootloader)
- Instructions about uf2conv.py [here](https://github.com/microsoft/uf2/blob/master/utils/uf2conv.md)
- Make sure you have Python installed and working
- Clone the [Microsoft uf2 repository](https://github.com/microsoft/uf2.git)
- Go to the `uf2/utils` folder and do `chmod +x uf2conv.py` to make the script executable
- Take the package zip file `application_xiao_nRF52840.zip` created in the last step, extract its contents and go to this folder
- Execute `uf2conv.py ANT_s212_nrf52_7.0.1.bin --convert --base 0x1000 --family 0x28860045 --output softdevice.uf2` to create the uf2 file with the SoftDevice
- Execute `uf2conv.py application_xiao_nRF52840.bin --convert --base 0x12000 --family 0x28860045 --output application.uf2` to create the uf2 file with the application itself
- Now connect the Seeed XIAO (Sense) board to your computer and double press the reset button quickly to activate the upload mode
- Copy the `softdevice.uf2` file to the board. Probably the copy process will end with an error because the board will immediately reboot after the file is written. To check the correct upload just have a look into the `INFO_UF2.TXT` file which should now read `SoftDevice: S212 version 7.0.1` in line four.
- If everything went fine in the last steps, continue with copying the `application.uf2` file to the board.
- Now we have to create a `.hex` file with the contents of the SoftDevice and the application itself **BUT WITHOUT** the initial 4096 bytes which can't be overwritten as they are the MBR:
  - A convenient way is to use the [SRecord](https://srecord.sourceforge.net) tools. If you're using a Mac with Homebrew they can easily be installed by typing `brew install srecord`
  - Go to the folder where the SoftDevice S212 file `ANT_s212_nrf52_7.0.1.hex` resides
  - Execute `srec_info ANT_s212_nrf52_7.0.1.hex -Intel` and you can see that it consists of two data blocks:
     ```
      Format: Intel Hexadecimal (MCS-86)
      Data:   0000 - 0AFF
              1000 - F283
     ```
  - Execute `uf2conv.py ANT_s212_nrf52_7.0.1.bin --convert --base 0x1000 --output softdevice.uf2` to create the uf2 file with the SoftDevice

## How compiling the original source code works
Unfortunately the Keil ARM MDK and especially Keil µVision are Windows-only software. You can use tools like [Wine](https://www.winehq.org) or [Crossover](https://www.codeweavers.com/crossover) to run the software on Mac OS or Linux. If you're using an Apple Silicon Mac you can also use [Whisky](https://github.com/Whisky-App/Whisky) which is free and works perfectly for this purpose.

### Required downloads
- Keil MDK v5.36 (last version with Arm Compiler 5 and 6) [(here)](https://armkeil.blob.core.windows.net/eval/MDK536.EXE)

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

## [Copyright notice](LICENSE_A+SS.txt)
```
This software is subject to the ANT+ Shared Source License
www.thisisant.com/swlicenses
Copyright (c) Garmin Canada Inc. 2019
All rights reserved.
```

# XENSIV&trade; CMOS radar presence detection and tracking for PSoC&trade; 6 MCUs

This code example demonstrates the capabilities of Infineon's XENSIV&trade; radar by using the cmos radar library for presence detection and tracking.

**Platform:** This is the connected sensor kit (CSK) bare-metal version for CYSBSYSKIT-DEV-01. The key features are:

- Uses SPI frequency of 18.75 MHz
- DEBUG UART pins for communication
- Includes LED initialization and control
- Includes a compile switch to select between on-chip and off-chip feature
- Radar configuration optimized for presence detection and tracking
- Additional radar config for indoor presence localization with post-processing (clustering and clutter removal)

For further information, please visit the website <https://www.infineon.com/design-resources/development-tools>. On this website you will also find the Xensiv CMOS Radar Visualization Tool, which can be used to visualize the radar data and detections from this code example.

Detailed information about the software package and its structure can be found in [System Documentation](docs/html/index.html)

To switch between the different radar use cases, enable or disable the compile switch `RADAR_MOTION_DETECTION` in the *Makefile*.

When `RADAR_MOTION_DETECTION` is enabled, the application will be configured for using the on-chip hardware module *motion detection*. Then the target processing is performed on the radar chip, and the application only receives a target list from the radar sensor. This target list includes the detected targets with their corresponding range, angle, and velocity information. After receiving the target list, the application applies a clutter and clustering algorithm to filter out clutter and group the detected targets into clusters, which are then reported as presence detection events.

When `RADAR_MOTION_DETECTION` is disabled, the application will be configured for using the off-chip software module *presence detection and tracking*.

## Radar configuration

Within this software package, radar configuration header files for Japanese market compliance are included. To use this configuration, select the corresponding header file in *main.c*.

Following radar configurations are provided:

- Configuration for presence detection and tracking (src/radar_config/presence_detection_tracking.h)
- Configuration for presence detection and localization (src/radar_config/presence_detection_localization.h)

[View this README on GitHub.](https://github.com/Infineon/mtb-example-psoc6-cmos-radar-people-tracking)

## Algo configurations

Presence detection and tracking (PDT) algorithm offers few parameters that can be adjusted by user according to usecase and environment. These parameters are defined in `def_var_config_default.c` file under the C struct `ifx_var_config_t`. The parameters which user can change include:

- Mounting parameters like height and tilt angle
- Field of view config to restrict algo for a given environment
- Radar orientation for Tx antenna (by default top left)
- Angle correction values

Make sure CSK wing board has flashed the correct presence detection tracking firmware version (v1.4.0 or above) before proceeding. Once connected, algo configuration tab can be accessed to adjust the parameters. Whenever changes are made, remember to export the updated configuration to ensure consistency. Click Apply to make sure settings are updated in the GUI and firmware.
![Post angle correction feature](docs/images/post_angle_corr_feature.png)

Once parameters are okay for a usecase, then user cna export them to a JSON file for later use in the embedded project.

![Post angle correction export changed parameters](docs/images/post_angle_corr_export.png)

### Mounting parameters

`tilt_angle_deg` rotates each detection into the mounted radar's coordinate system, while `mounting_height_m` offsets
its vertical position before ground-range filtering. The related ground-range settings can also reject detections by
elevation, minimum radar range, or calculated ground position.

### Field of view

`min_max_ground_range` and `min_max_azimuth` define the valid output area. After processing, tracks and chirpwise
detections outside either interval are cleared from the result.

### Radar orientation

`orientation_tx` identifies which corner of the radar antenna layout contains the transmitting antenna. The selected
orientation determines the AoA channels used for azimuth and elevation and whether either angle needs its sign inverted;
the default `TopLeft` orientation uses positive scaling for both angles.

### Angle correction

When enabled, `measured_deg` supplies the sensor-measured angles at evenly spaced reference positions defined by
`azimuth_min_deg` and `step_deg`. The tracker linearly interpolates the correction between these nodes, applies it to
each track azimuth, and handles values outside the calibration range according to `outside_policy` and `edge_handling`
(for example, clamping or dropping them).

### Updating algo configuration at runtime

The algorithm parameters can also be updated at runtime by sending an algorithm user-configuration JSON payload to the
firmware. Use terminal command ID `3` (`TERMINAL_CMD_TYPE_APPLY_ALGO_USER_CONFIGURATION`). The angle-correction settings
are placed under `angle_correction`, and the `measured_deg` values are the sensor-reported angles at the fixed reference
positions from -70 to 70 degrees in 10-degree steps.

The firmware first reads the current configuration and then updates only the fields present in the JSON object, so a
complete object is not required. The following complete user-configuration object contains all settings supported by
this command. `orientation_tx` uses `TopLeft = 0`, `BottomLeft = 1`, `TopRight = 2`, and `BottomRight = 3`;
`outside_policy` uses `Clamp = 0` and `None = 1`; and `edge_handling` uses `Clip = 0`, `Drop = 1`, and `None = 2`.

```json
{
   "sensor_mounting": {
      "orientation_tx": 0,
      "tilt_angle_deg": 0,
      "mounting_height_m": 1.5
   },
   "fov": {
      "range_m": {
         "min": 0,
         "max": 10
      },
      "angle_deg": {
         "min": -90,
         "max": 90
      }
   },
   "angle_correction": {
      "enable": true,
      "outside_policy": 0,
      "edge_handling": 2,
      "edge_clip_deg": 75,
      "measured_deg": {
         "measured_at_m70": -70,
         "measured_at_m60": -60,
         "measured_at_m50": -50,
         "measured_at_m40": -40,
         "measured_at_m30": -30,
         "measured_at_m20": -20,
         "measured_at_m10": -10,
         "measured_at_0": 0,
         "measured_at_p10": 10,
         "measured_at_p20": 20,
         "measured_at_p30": 30,
         "measured_at_p40": 40,
         "measured_at_p50": 50,
         "measured_at_p60": 60,
         "measured_at_p70": 70
      }
   },
   "tracker": {
      "macro_sensitivity": 1,
      "micro_sensitivity": 1,
      "safe_mode_as_default": true,
      "room_size_detection": true
   }
}
```

To enable angle correction, set `angle_correction.enable` to `true`. The
calibration node positions are fixed at -70 to 70 degrees in 10-degree steps
in the user-settings interface; `measured_deg` contains the corresponding
sensor readings.

The configuration is applied without rebuilding the firmware. The current
algorithm user configuration can be queried with terminal command ID `7`
(`TERMINAL_CMD_TYPE_GET_ALGO_USER_CONFIGURATION`).

#### Applying modified configurations to firmware at build time

Once you have modified the above algorithm user configuration, you need to rebuild the firmware to apply the changes at build time. This ensures that the updated settings are incorporated into the firmware and used by the algorithm during operation.

#### Steps to rebuild the firmware

1. Open your development environment and load the project for presence detection and tracking code example in ModusToolbox (MTB) Eclipse IDE or in VS Code.
2. Make sure the modified algorithm user configurations are exported as JSON file from Xensiv CMOS Visualization tool. This JSON file looks like image shown below.
![Post angle correction exported json](docs/images/post_angle_corr_json.png)

3. Copy the desired parameters from the exported JSON file parameters to the `process_task_init()`function in `process_task.c` file. Following C code snippet hint at the user settings, which user can apply to the embedded project. Only uncomment the lines, which parameters are different then the default settings.

```c
void process_task_init(void)  // src/common/process_task.c
{
   ...
   /* Optionally adjust user settings (e.g. sensor mounting, fov, angle correction) before init() from usecasesetting.h file */
   ifx_usecase_settings settings;
   int status = ifx_usecase_get_settings(usecase_handle, &settings);

   settings.sensor_mounting.mounting_height_m = 1.5f;
   settings.sensor_mounting.tilt_angle_deg = 0.0f;

   settings.fov.angle_deg.min = -90.0f;
   settings.fov.angle_deg.max = +90.0f;

   // ifx_usecase_settings_angle_correction_t settings
   settings.angle_correction.edge_handling = IFX_USECASE_SETTINGS_ANGLE_CORRECTION_EDGE_HANDLING_CLIP;
   settings.angle_correction.outside_policy = IFX_USECASE_SETTINGS_ANGLE_CORRECTION_OUTSIDE_POLICY_CLAMP;

   settings.angle_correction.enable = false;
   settings.angle_correction.measured_deg.measured_at_m70 = -70;
   settings.angle_correction.measured_deg.measured_at_m60 = -60;
   settings.angle_correction.measured_deg.measured_at_m50 = -50;
   settings.angle_correction.measured_deg.measured_at_m40 = -40;
   settings.angle_correction.measured_deg.measured_at_m30 = -30;
   settings.angle_correction.measured_deg.measured_at_m20 = -20;
   settings.angle_correction.measured_deg.measured_at_m10 = -10;
   settings.angle_correction.measured_deg.measured_at_0 = 0;
   settings.angle_correction.measured_deg.measured_at_p10 = 10;
   settings.angle_correction.measured_deg.measured_at_p20 = 20;
   settings.angle_correction.measured_deg.measured_at_p30 = 30;
   settings.angle_correction.measured_deg.measured_at_p40 = 40;
   settings.angle_correction.measured_deg.measured_at_p50 = 50;
   settings.angle_correction.measured_deg.measured_at_p60 = 60;
   settings.angle_correction.measured_deg.measured_at_p70 = 70;

   // ifx_usecase_settings_tracker_t settings
   settings.tracker.macro_sensitivity = 1;
   settings.tracker.micro_sensitivity = 1;
   settings.tracker.safe_mode_as_default = true;
   settings.tracker.room_size_detection = true;

   status = ifx_usecase_set_settings(usecase_handle, &settings);
   if (status != 0)
   {
      CY_ASSERT(0);
   }
}
```

4. Clean the previous build artifacts to ensure a fresh build.
5. Build the firmware project.
6. Flash the newly built firmware onto the target hardware.

After completing these steps, the updated algorithm user configuration will be applied at build time.

### Calibration matrix update

Imperfections from the transmitter (TX) to receiver (RX) path on antenna side, especially for antenna in package, have a non-negligible impact on the phase information. Furthermore, reflections coming from very short distances like housings or covers of the radar sensor have an influence on the phase. To compensate such impacts, it is inevitable to apply a calibration matrix prior to the angle calculation. The calibration parameters are specific to the hardware and in some cases, for example, when the radar sensor is used in a new  housing, the calibration parameters may need to be updated. In such cases, you can contact Infineon support to get the updated calibration matrix.

Presence detection and tracking algorithm uses calibration matrix as 3x3 complex matrix for H-plane and E-plane, which is applied to the raw radar data before the angle calculation. To update the calibration matrix, please open `def_var_config_default.c` file and update the struct `ifx_var_config_t` (used for variable configurations of algorithm) as shown below. This C struct is populated as row-major order, and the real and imaginary parts of the complex numbers are represented as two separate float numbers in the struct.

```c
.calib_mat = {
      .E = {
         { (float)1, (float)2.8965e-18 }, // E11
         { (float)0, (float)0 }, // E12
         { (float)-0.051597, (float)-0.0053787 },  // E13
         { (float)0, (float)0 },  // E21
         { (float)1, (float)0 },  // E22
         { (float)0, (float)0 },  // E23
         { (float)-1.0244e-17, (float)-7.9666e-18 },  // E31
         { (float)0, (float)0 },  // E32
         { (float)-0.29519, (float)-0.94878 }  // E33
      },
      .H = {
         { (float)1, (float)9.1422e-18 },  // H11
         { (float)0.052308, (float)0.24889 },  // H12
         { (float)0, (float)0 },  // H13
         { (float)-3.7177e-17, (float)3.5505e-18 },  // H21
         { (float)-0.84646, (float)0.39313 },  // H22
         { (float)0, (float)0 },  // H22
         { (float)0, (float)0 },  // H23
         { (float)0, (float)0 },  // H31
         { (float)1, (float)0 }  // H32
      }
```

## Requirements

- [ModusToolbox&trade;](https://www.infineon.com/modustoolbox) v3.4 or later (tested with v3.4)
- Board support package (BSP) minimum required version: 5.0.0
- Programming language: C
- Associated parts: All [PSOC&trade; 6 MCU](https://www.infineon.com/cms/en/product/microcontroller/32-bit-psoc-arm-cortex-microcontroller/psoc-6-32-bit-arm-cortex-m4-mcu) parts

## Supported toolchains (make variable 'TOOLCHAIN')

- GNU Arm&reg; Embedded Compiler v11.3.1 (`GCC_ARM`) – Default value of `TOOLCHAIN`

## Supported kits (make variable 'TARGET')

- [Rapid IoT Connect Developer Kit](https://github.com/Infineon/TARGET_CYSBSYSKIT-DEV-01) (`CYSBSYSKIT-DEV-01`) – Default value of `TARGET`

## Hardware setup

### For the Rapid IoT Connect Developer Kit

This code example requires the XENSIV&trade; BGT60CUTR13AIP radar wing board as part of the connected sensor kit.

1. Connect the radar shield to the CYSBSYSKIT-DEV-01 kit through the pin headers

   ![Radar shield connected to CYSBSYSKIT-DEV-01 kit](docs/images/csk_wing.jpg)

   **Figure 1. Radar board**

2. Connect the CYSBSYSKIT-DEV-01 kit to the PC with the USB cable

   ![Rapid IoT Connect Developer Kit](docs/images/csk_evl_kit.jpg)

   **Figure 2. Rapid IoT Connect Developer Kit**

3. Place the CYSBSYSKIT-DEV-01 kit at a fixed location (for example, the corner of a room) to ensure optimal performance of the presence detection application

4. Ensure that the horizontal orientation of the radar shield is on E-plane, which is indicated on the PCB of the radar shield close to the antenna array with an "E" marking

> **Note:** The PSOC&trade; 6 Bluetooth&reg; LE Pioneer Kit (CY8CKIT-062-BLE) and the PSOC&trade; 6 Wi-Fi Bluetooth&reg; Pioneer Kit (CY8CKIT-062-WIFI-BT) ship with KitProg2 installed. ModusToolbox&trade; requires KitProg3. Before using this code example, make sure that the board is upgraded to KitProg3. The tool and instructions are available in the [Firmware Loader](https://github.com/Infineon/Firmware-loader) GitHub repository. If you do not upgrade, you will see an error like "unable to find CMSIS-DAP device" or "KitProg firmware is out of date".

## Software setup

See the [ModusToolbox&trade; tools package installation guide](https://www.infineon.com/ModusToolboxInstallguide) for information about installing and configuring the tools package.

Install a terminal emulator if you do not have one. Instructions in this document use [Tera Term](https://teratermproject.github.io/index-en.html).

This example requires no additional software or tools.

## Using the code example

### Create the project

The ModusToolbox&trade; tools package provides the Project Creator as both a GUI tool and a command line tool.

<details><summary><b>Use Project Creator GUI</b></summary>

1. Open the Project Creator GUI tool

   There are several ways to do this, including launching it from the dashboard or from inside the Eclipse IDE. For more details, see the [Project Creator user guide](https://www.infineon.com/ModusToolboxProjectCreator) (locally available at *{ModusToolbox&trade; install directory}/tools_{version}/project-creator/docs/project-creator.pdf*)

2. On the **Choose Board Support Package (BSP)** page, select a kit supported by this code example. See [Supported kits](#supported-kits-make-variable-target)

   > **Note:** To use this code example for a kit not listed here, you may need to update the source files. If the kit does not have the required resources, the application may not work

3. On the **Select Application** page:

   a. Select the **Applications(s) Root Path** and the **Target IDE**

      > **Note:** Depending on how you open the Project Creator tool, these fields may be pre-selected for you

   b. Select this code example from the list by enabling its check box

      > **Note:** You can narrow the list of displayed examples by typing in the filter box

   c. (Optional) Change the suggested **New Application Name** and **New BSP Name**

   d. Click **Create** to complete the application creation process

</details>

<details><summary><b>Use Project Creator CLI</b></summary>

The 'project-creator-cli' tool can be used to create applications from a CLI terminal or from within batch files or shell scripts. This tool is available in the *{ModusToolbox&trade; install directory}/tools_{version}/project-creator/* directory.

Use a CLI terminal to invoke the 'project-creator-cli' tool. On Windows, use the command-line 'modus-shell' program provided in the ModusToolbox&trade; installation instead of a standard Windows command-line application. This shell provides access to all ModusToolbox&trade; tools. You can access it by typing "modus-shell" in the search box in the Windows menu. In Linux and macOS, you can use any terminal application.

The following example clones the "[Radar Application](https://github.com/Infineon/mtb-example-psoc6-cmos-radar-people-tracking)" application with the desired name "XENSIVPresenceTracking" configured for the *CYSBSYSKIT-DEV-01* BSP into the specified working directory, *C:/mtb_projects*:

   ```shell
   project-creator-cli --board-id CYSBSYSKIT-DEV-01 --app-id mtb-example-psoc6-cmos-radar-people-tracking --user-app-name XENSIVPresenceTracking --target-dir "C:/mtb_projects"
   ```

The 'project-creator-cli' tool has the following arguments:

Argument | Description | Required/optional
--------- | ------------- | -----------
`--board-id` | Defined in the `<id>` field of the [BSP](https://github.com/Infineon?q=bsp-manifest&type=&language=&sort=) manifest | Required
`--app-id` | Defined in the `<id>` field of the [CE](https://github.com/Infineon?q=ce-manifest&type=&language=&sort=) manifest | Required
`--target-dir` | Specify the directory in which the application is to be created if you prefer not to use the default current working directory | Optional
`--user-app-name` | Specify the name of the application if you prefer to have a name other than the example's default name | Optional

<br>

> **Note:** The project-creator-cli tool uses the `git clone` and `make getlibs` commands to fetch the repository and import the required libraries. For details, see the "Project creator tools" section of the [ModusToolbox&trade; tools package user guide](https://www.infineon.com/ModusToolboxUserGuide) (locally available at {ModusToolbox&trade; install directory}/docs_{version}/mtb_user_guide.pdf).

</details>

### Open the project

After the project has been created, you can open it in your preferred development environment.

<details><summary><b>Eclipse IDE</b></summary>

If you opened the Project Creator tool from the included Eclipse IDE, the project will open in Eclipse automatically.

For more details, see the [Eclipse IDE for ModusToolbox&trade; user guide](https://www.infineon.com/MTBEclipseIDEUserGuide) (locally available at *{ModusToolbox&trade; install directory}/docs_{version}/mt_ide_user_guide.pdf*).

</details>

<details><summary><b>Visual Studio (VS) Code</b></summary>

Launch VS Code manually, and then open the generated *{project-name}.code-workspace* file located in the project directory.

For more details, see the [Visual Studio Code for ModusToolbox&trade; user guide](https://www.infineon.com/MTBVSCodeUserGuide) (locally available at *{ModusToolbox&trade; install directory}/docs_{version}/mt_vscode_user_guide.pdf*).

</details>

<details><summary><b>Arm&reg; Keil&reg; µVision&reg;</b></summary>

Double-click the generated *{project-name}.cprj* file to launch the Keil&reg; µVision&reg; IDE.

For more details, see the [Arm&reg; Keil&reg; µVision&reg; for ModusToolbox&trade; user guide](https://www.infineon.com/MTBuVisionUserGuide) (locally available at *{ModusToolbox&trade; install directory}/docs_{version}/mt_uvision_user_guide.pdf*).

</details>

<details><summary><b>IAR Embedded Workbench</b></summary>

Open IAR Embedded Workbench manually, and create a new project. Then select the generated *{project-name}.ipcf* file located in the project directory.

For more details, see the [IAR Embedded Workbench for ModusToolbox&trade; user guide](https://www.infineon.com/MTBIARUserGuide) (locally available at *{ModusToolbox&trade; install directory}/docs_{version}/mt_iar_user_guide.pdf*).

</details>

<details><summary><b>Command line</b></summary>

If you prefer to use the CLI, open the appropriate terminal, and navigate to the project directory. On Windows, use the command-line 'modus-shell' program; on Linux and macOS, you can use any terminal application. From there, you can run various `make` commands.

For more details, see the [ModusToolbox&trade; tools package user guide](https://www.infineon.com/ModusToolboxUserGuide) (locally available at *{ModusToolbox&trade; install directory}/docs_{version}/mtb_user_guide.pdf*).

</details>

## Operation

If using a PSOC&trade; 64 "Secure" MCU kit (like CY8CKIT-064B0S2-4343W), the PSOC&trade; 64 device must be provisioned with keys and policies before being programmed. Follow the instructions in the ["Secure Boot" SDK user guide](https://www.infineon.com/dgdlac/Infineon-PSoC_64_Secure_MCU_Secure_Boot_SDK_User_Guide-Software-v07_00-EN.pdf?fileId=8ac78c8c7d0d8da4017d0f8c361a7666) to provision the device. If the kit is already provisioned, copy-paste the keys and policy folder to the application folder.

1. Connect the board to your PC using the provided USB cable through the KitProg3 USB connector

2. Open a terminal program and select the KitProg3 COM port. Set the serial port parameters to 8N1 and 2000000 baud

3. Program the board using one of the following:

   <details><summary><b>Using Eclipse IDE</b></summary>

      1. Select the application project in the Project Explorer

      2. In the **Quick Panel**, scroll down, and click **\<Application Name> Program (KitProg3_MiniProg4)**
   </details>

   <details><summary><b>In other IDEs</b></summary>

   Follow the instructions in your preferred IDE

   </details>

   <details><summary><b>Using CLI</b></summary>

     From the terminal, execute the `make program` command to build and program the application using the default toolchain to the default target. The default toolchain is specified in the application's Makefile but you can override this value manually:

      ```shell
      make program TOOLCHAIN=<toolchain>
      ```

      Example:

      ```shell
      make program TOOLCHAIN=GCC_ARM
      ```

   </details>

4. After programming, the application starts automatically. The output on the terminal should look like:

  ![Application initialization output on terminal](docs/images/app_init.png)

## Debugging

You can debug the example to step through the code.

<details><summary><b>In Eclipse IDE</b></summary>

Use the **\<Application Name> Debug (KitProg3_MiniProg4)** configuration in the **Quick Panel**. For details, see the "Program and debug" section in the [Eclipse IDE for ModusToolbox&trade; user guide](https://www.infineon.com/MTBEclipseIDEUserGuide).

> **Note:** **(Only while debugging)** On the CM4 CPU, some code in `main()` may execute before the debugger halts at the beginning of `main()`. This means that some code executes twice – once before the debugger stops execution, and again after the debugger resets the program counter to the beginning of `main()`. See [PSOC&trade; 6 MCU: Code in main() executes before the debugger halts at the first line of main()](https://community.infineon.com/docs/DOC-21143) to learn about this and for the workaround.

</details>

<details><summary><b>In other IDEs</b></summary>

Follow the instructions in your preferred IDE.

</details>

<br>

## Related resources

Resources | Links
---------- | ----------------------------------
Application notes | [AN228571](https://www.infineon.com/AN228571) – Getting started with PSOC&trade; 6 MCU on ModusToolbox&trade; <br>  [AN215656](https://www.infineon.com/AN215656) – PSOC&trade; 6 MCU: Dual-CPU system design <br> [AN79953](https://www.infineon.com/AN79953) – Getting started with PSOC&trade; 4 MCU <br>  [AN85951](https://www.infineon.com/AN85951) – PSOC&trade; 4 and PSOC&trade; 6 MCU CAPSENSE&trade; design guide
Code examples | [Using ModusToolbox&trade;](https://github.com/Infineon/Code-Examples-for-ModusToolbox-Software) on GitHub
Device documentation | [PSOC&trade; 6 MCU datasheets](https://documentation.infineon.com/html/psoc6/bnm1651211483724.html) <br> [PSOC&trade; 6 technical reference manuals](https://documentation.infineon.com/html/psoc6/zrs1651212645947.html)
Development kits | Select your kits from the [Evaluation board finder](https://www.infineon.com/cms/en/design-support/finder-selection-tools/product-finder/evaluation-board)
Libraries on GitHub | [mtb-pdl-cat1](https://github.com/Infineon/mtb-pdl-cat1) – PSOC&trade; 6 Peripheral Driver Library (PDL)  <br> [mtb-hal-cat1](https://github.com/Infineon/mtb-hal-cat1) – Hardware Abstraction Layer (HAL) library <br> [retarget-io](https://github.com/Infineon/retarget-io) – Utility library to retarget STDIO messages to a UART port <br>  [mtb-pdl-cat2](https://github.com/Infineon/mtb-pdl-cat2) – PSOC&trade; 4 Peripheral Driver Library (PDL) <br>  [mtb-hal-cat2](https://github.com/Infineon/mtb-hal-cat2) – Hardware Abstraction Layer (HAL) library
Middleware on GitHub | [sensor-xensiv-bgt60cutrxx](https://github.com/Infineon/sensor-xensiv-bgt60cutrxx) – Driver library for Xensiv CMOS radar sensor and documents <br> [psoc6-middleware](https://github.com/Infineon/modustoolbox-software#psoc-6-middleware-libraries) – Links to all PSOC&trade; 6 MCU middleware
Tools | [ModusToolbox&trade;](https://www.infineon.com/modustoolbox) – ModusToolbox&trade; software is a collection of easy-to-use libraries and tools enabling rapid development with Infineon MCUs for applications ranging from wireless and cloud-connected systems, edge AI/ML, embedded sense and control, to wired USB connectivity using PSOC&trade; Industrial/IoT MCUs, AIROC&trade; Wi-Fi and Bluetooth&reg; connectivity devices, XMC&trade; Industrial MCUs, and EZ-USB&trade;/EZ-PD&trade; wired connectivity controllers. ModusToolbox&trade; incorporates a comprehensive set of BSPs, HAL, libraries, configuration tools, and provides support for industry-standard IDEs to fast-track your embedded application development

<br>

## Other resources

Infineon provides a wealth of data at [www.infineon.com](https://www.infineon.com) to help you select the right device, and quickly and effectively integrate it into your design.

For more information about the connected sensor kit, see [IoT Sensors Platform](https://www.infineon.com/connectedsensorkit).

## Document history

Document title: *PSOC&trade; 6 MCU: Presence detection and tracking using XENSIV&trade; 60 GHz radar*

 Version | Description of change
 ------- | ---------------------
 1.0.0   | New code example

<br>

All referenced product or service names and trademarks are the property of their respective owners.

The Bluetooth&reg; word mark and logos are registered trademarks owned by Bluetooth SIG, Inc., and any use of such marks by Infineon is under license.

PSOC&trade;, formerly known as PSoC&trade;, is a trademark of Infineon Technologies. Any references to PSoC&trade; in this document or others shall be deemed to refer to PSOC&trade;.

---------------------------------------------------------

(c) 2025, Infineon Technologies AG, or an affiliate of Infineon Technologies AG. All rights reserved.
This software, associated documentation and materials ("Software") is owned by Infineon Technologies AG or one of its affiliates ("Infineon") and is protected by and subject to worldwide patent protection, worldwide copyright laws, and international treaty provisions. Therefore, you may use this Software only as provided in the license agreement accompanying the software package from which you obtained this Software. If no license agreement applies, then any use, reproduction, modification, translation, or compilation of this Software is prohibited without the express written permission of Infineon.
<br>
Disclaimer: UNLESS OTHERWISE EXPRESSLY AGREED WITH INFINEON, THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, ALL WARRANTIES OF NON-INFRINGEMENT OF THIRD-PARTY RIGHTS AND IMPLIED WARRANTIES SUCH AS WARRANTIES OF FITNESS FOR A SPECIFIC USE/PURPOSE OR MERCHANTABILITY. Infineon reserves the right to make changes to the Software without notice. You are responsible for properly designing, programming, and testing the functionality and safety of your intended application of the Software, as well as complying with any legal requirements related to its use. Infineon does not guarantee that the Software will be free from intrusion, data theft or loss, or other breaches (“Security Breaches”), and Infineon shall have no liability arising out of any Security Breaches. Unless otherwise explicitly approved by Infineon, the Software may not be used in any application where a failure of the Product or any consequences of the use thereof can reasonably be expected to result in personal injury.

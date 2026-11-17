# Changelog for Code Example for the on-chip and off-chip feature based on the XENSIV&trade; BGT60CUTRxx Radar Sensor

## Overview

The Presence Detection and Tracking (PDT) library converts raw data from the XENSIV&trade; 60 GHz CMOS radar sensor into real-time insights on indoor human activity. Because it operates on a frequency spectrum independent of visible light, the system ensures consistent performance in total darkness or low-visibility conditions without requiring optical clarity.

### 1.2.0 (public release)

Update of documentation to include details about the algorithm implementation and link to the documentation.

### 1.1.6

#### Improved
- Update XENSIV&trade; BGT60CUTR13AIP driver version to 1.1.0

### 1.1.5

#### Fixed
- Fixed firmware file-name and firmware-name mismatch.

### 1.1.4

#### Change
- Removed obsolete release job definitions.

### 1.1.3

#### Fixed
- Fixed broken CI release jobs.

#### Improved

### 1.1.2

#### Add
- Board name is now included in the info message.
- Embedded application naming is generated from use case and target hardware.

#### Change
- Presence localization was set as the default use case.
- Hex and protocol firmware naming was aligned to a unified convention.
- Build and task execution migrated to pixi environments.
- CI pipeline paths and runner setup were updated for the current infrastructure.

#### Fixed
- Resolved CI logic for selecting the correct hex artifact path per use case.
- Added compile-time guard for unsupported combination of Japan ISM band and on-chip motion detection.

#### Improved
- Cross-compilation flow for Windows artifacts was improved.
- MTB task handling and stack-usage checks were refactored for better cross-platform behavior.

### 1.1.1

#### Add
- Event information is included in protocol info packages.
- Detection type mapping for event information was introduced.

#### Change
- UART command naming and compatibility documentation were updated.
- Clutter event handling now follows the documented event type definitions.

#### Improved
- Protocol documentation was expanded with `event_info` examples.
- Source formatting was harmonized with uncrustify.

### 1.1.0

#### Add
- Integration of the use-case implementation from released artifacts instead of local duplicated sources.

#### Change
- Updated to the latest available `ifxalgo` release artifact.

### 1.0.0 (public release)

This is the initial release of the library to convert raw data from Infineon's cmos radar sensor

#### Add
- **Macro Detection**: Identifies large-scale movements and displacements, such as walking or running
- **Micro Detection**: Detects smaller motions without significant displacement, like hand gestures or posture changes
- **Static Detection**: Captures very subtle movements, including breathing or minor vibrations
- **on-chip feature**: Utilizes the radar sensor's built-in motion detection capabilities to identify movement without the need for complex signal processing algorithms.
- Configures GPIOs and the SPI interface for radar sensor communication.
- Compile switch to select between on-chip and off-chip feature.


#### Change

#### Fixed

#### Improved

## Supported software and tools

This version of the XENSIV&trade; BGT60CUTRxx 60GHz radar sensor code example was validated for compatibility with the following software and tools:

| Software and tools                        | Version |
| :---                                      | :----:  |
| ModusToolbox&trade; software environment        | 3.4.0   |
| GCC compiler                              | 10.3.1  |

Minimum required ModusToolbox&trade; software environment: v3.4


© Infineon Technologies AG, 2026.

# Upstream components and notices

This repository contains an adaptation of Infineon software for Infineon hardware. Existing copyright notices, license files and evaluation terms are retained. This file does not replace or extend those licenses, and no blanket open-source license is applied to the complete firmware bundle.

| Component | Source / revision | Notices |
| --- | --- | --- |
| Radar application | [mtb-example-psoc6-cmos-radar-people-tracking](https://github.com/Infineon/mtb-example-psoc6-cmos-radar-people-tracking), `ba88746a13831f0f20d87ff3e6b748445b5838df` | [firmware/radar-app/LICENSE.txt](firmware/radar-app/LICENSE.txt) and per-file headers |
| Bluetooth example used during integration | [mtb-example-btstack-freertos-findme](https://github.com/Infineon/mtb-example-btstack-freertos-findme), `5d892da1432517b8a20a82846843ef26a8769475` | [licenses/ble-findme](licenses/ble-findme) and generated-file notices |
| Application BSP | CYSBSYSKIT-DEV-01, adapted as APP_CYSBSYSKIT-DEV-01 | [BSP LICENSE](firmware/radar-app/bsps/TARGET_APP_CYSBSYSKIT-DEV-01/LICENSE), [BSP EULA](firmware/radar-app/bsps/TARGET_APP_CYSBSYSKIT-DEV-01/EULA) |
| Tracking library, radar driver, RTOS, Bluetooth stack/radio firmware and device support | Versions selected by [deps](firmware/radar-app/deps), asset locks and BSP dependency manifests | [licenses](licenses); original notices also accompany downloaded libraries |
| Original recovery HEX and board guide | XENSIV CMOS Radar Visualization Tool 1.6.0 and the Infineon KIT_CSK_BGT60CUTR13 user manual | Infineon notices and terms remain applicable |

The dependency license copies accompany the prepared HEX files. Full third-party source checkouts and their Git histories are not vendored here; `scripts/build-ble.ps1 -GetLibraries` obtains dependencies from their upstream repositories. The required local interrupt-restoration fix is reapplied by `scripts/patch-btstack.py`.

The active source is version 0.2.0. The older 0.1.0 HEX is retained as a fallback image, rather than as a second source tree. The browser documentation's capture fixture and test protocol fixture are small measurements collected with the development board; they are intentionally retained instead of the full diagnostic history.

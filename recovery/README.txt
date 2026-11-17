Original Infineon firmware recovery
===================================

Image: presence_detection_and_tracking_csk_v1_4_1.hex
Application: presence_detection_and_tracking 1.4.1, build eaf479a
Algorithm: 1.2.1-6253c6e+common.indoorSmartar17
Board: CYSBSYSKIT-DEV-01 / CY8C624AFNI-S2D43
SHA-256: eeb8011ed0270eca709189a441f238a72993a094d24e0ce4784de2ae40270732
Source: XENSIV CMOS Radar Visualization Tool 1.6.0

Run ../Restore-Original.cmd with KitProg3 USB connected. Use -CheckOnly
for a files/tools check without touching hardware. See ../README.md.

The original recovery path was tested on hardware: 246516 image bytes
matched the read-back, OpenOCD program/verify succeeded, firmware identity
and settings matched, and tracking frames arrived after restoration.
The new double-click launcher was tested with mock OpenOCD and CheckOnly.

This restores the application, not all device memories or future persistent
settings. No SFlash, fuse or external QSPI restoration is provided.
original-settings.json contains the settings read from the original firmware.

Full flash snapshots, source-history archives and verification logs remain
local and are excluded from Git. scripts/check-recovery.py requires those
local evidence files and is not part of the clean-clone test suite.

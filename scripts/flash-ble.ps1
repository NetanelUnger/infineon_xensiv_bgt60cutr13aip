$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$openOcdRoot = 'C:/Infineon/Tools/XENSIV-CMOS-Radar-Visualization-Tool/1.6.0/lib/ifxapp/utils/firmware_flashing/tools/openocd/windows/openocd'
$imageRelative = 'firmware/radar-app/build/APP_CYSBSYSKIT-DEV-01/Release/presence_detection_and_tracking_ble.hex'
if (-not (Test-Path -LiteralPath (Join-Path $projectRoot $imageRelative))) { throw 'Build the BLE firmware first.' }
Push-Location $projectRoot
try {
    New-Item -ItemType Directory -Path 'logs' -Force | Out-Null
    & "$openOcdRoot/bin/openocd.exe" -s "$openOcdRoot/scripts" -f interface/kitprog3.cfg `
        -c 'adapter serial 110D0E8300023400' -c 'transport select swd' -f target/infineon/cy8cxxa.cfg `
        -c "program $imageRelative verify reset exit" 2>&1 | Tee-Object -FilePath logs/flash-ble.log
    if ($LASTEXITCODE -ne 0) { throw "BLE flash failed: $LASTEXITCODE" }
} finally { Pop-Location }

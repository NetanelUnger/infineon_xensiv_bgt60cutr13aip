param(
    [string]$OpenOcdRoot = 'C:/Infineon/Tools/XENSIV-CMOS-Radar-Visualization-Tool/1.6.0/lib/ifxapp/utils/firmware_flashing/tools/openocd/windows/openocd',
    [string]$ProbeSerial = '110D0E8300023400'
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$firmware = Join-Path $projectRoot 'recovery/presence_detection_and_tracking_csk_v1_4_1.hex'
$expectedHash = 'EEB8011ED0270ECA709189A441F238A72993A094D24E0CE4784DE2AE40270732'
if ((Get-FileHash -LiteralPath $firmware -Algorithm SHA256).Hash -ne $expectedHash) {
    throw 'Original firmware checksum mismatch. Nothing was flashed.'
}
if ($ProbeSerial -notmatch '^[A-Za-z0-9]+$') { throw 'Invalid probe serial' }
$openocd = Join-Path $OpenOcdRoot 'bin/openocd.exe'
if (-not (Test-Path -LiteralPath $openocd)) { throw "OpenOCD not found: $openocd" }
Push-Location $projectRoot
try {
    & $openocd -s "$OpenOcdRoot/scripts" -f interface/kitprog3.cfg `
        -c "adapter serial $ProbeSerial" -c 'transport select swd' `
        -f target/infineon/cy8cxxa.cfg `
        -c 'program recovery/presence_detection_and_tracking_csk_v1_4_1.hex verify reset exit' `
        2>&1 | Tee-Object -FilePath recovery/restore-latest.log
    if ($LASTEXITCODE -ne 0) { throw "OpenOCD failed: $LASTEXITCODE. See recovery/restore-latest.log" }
    Write-Host 'Original tracking firmware 1.4.1 restored and flash verified.'
    Write-Host 'Wait at least 5 seconds, then reconnect XENSIV on CSK (UART), COM11.'
    Write-Host 'If the GUI retains the previous identity, disconnect/reconnect USB power and refresh it.'
}
finally { Pop-Location }

param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('Original', 'BLE')]
    [string]$Firmware,
    [switch]$CheckOnly,
    [string]$OpenOcdRoot = 'C:/Infineon/Tools/XENSIV-CMOS-Radar-Visualization-Tool/1.6.0/lib/ifxapp/utils/firmware_flashing/tools/openocd/windows/openocd'
)

# Double-click launchers use Windows PowerShell 5.1, which treats native stderr
# as ErrorRecords. OpenOCD normally writes progress to stderr, even on success.
# Check the native exit code AND verify marker; do not treat progress as failure.
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$probeSerial = '110D0E8300023400'
$images = @{
    Original = @{
        Name = 'Infineon presence_detection_and_tracking 1.4.1'
        File = 'recovery/presence_detection_and_tracking_csk_v1_4_1.hex'
        Hash = 'EEB8011ED0270ECA709189A441F238A72993A094D24E0CE4784DE2AE40270732'
    }
    BLE = @{
        Name = 'Radar BLE 0.2.0 development (with continuous advertising)'
        File = 'release/radar-ble-v0.2.0-development.hex'
        Hash = '44E5D328B9091602C6C429158C09525E8B423C577394BE2B0DE82DDF76C34610'
    }
}

try {
    $image = $images[$Firmware]
    $imagePath = Join-Path $projectRoot $image.File
    $openocd = Join-Path $OpenOcdRoot 'bin/openocd.exe'
    $scriptsPath = Join-Path $OpenOcdRoot 'scripts'
    Write-Host "Firmware: $($image.Name)"
    Write-Host "Image:    $($image.File)"
    Write-Host "KitProg3: $probeSerial"
    if ($Firmware -eq 'BLE') {
        Write-Host 'Known issue: the 10-second recording download can stall. See KNOWN_ISSUES.md.' -ForegroundColor Yellow
    }
    if (-not (Test-Path -LiteralPath $imagePath -PathType Leaf)) { throw "Firmware image missing: $imagePath" }
    if ((Get-FileHash -LiteralPath $imagePath -Algorithm SHA256).Hash -ne $image.Hash) {
        throw 'Firmware SHA256 mismatch. Nothing was flashed.'
    }
    foreach ($required in @($openocd, (Join-Path $scriptsPath 'interface/kitprog3.cfg'), (Join-Path $scriptsPath 'target/infineon/cy8cxxa.cfg'))) {
        if (-not (Test-Path -LiteralPath $required -PathType Leaf)) { throw "Required programming tool file missing: $required" }
    }
    Write-Host 'Firmware SHA256 and programming tool files: OK.'
    if ($CheckOnly) {
        Write-Host 'CHECK ONLY completed. No hardware connection or flash operation was attempted.'
        exit 0
    }

    $logs = Join-Path $projectRoot 'logs'
    New-Item -ItemType Directory -Path $logs -Force | Out-Null
    $logPath = Join-Path $logs ("install-{0}-{1}.log" -f $Firmware.ToLowerInvariant(), (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
    @("Firmware: $($image.Name)", "Image: $($image.File)", "SHA256: $($image.Hash)", "KitProg3: $probeSerial") | Set-Content -LiteralPath $logPath -Encoding UTF8
    Write-Host 'Programming via USB/KitProg3. Keep USB connected until verification finishes.'
    Write-Host "Log: $logPath"
    Push-Location $projectRoot
    try {
        $ErrorActionPreference = 'Continue'
        $output = & $openocd -s $scriptsPath -f interface/kitprog3.cfg `
            -c "adapter serial $probeSerial" -c 'transport select swd' `
            -f target/infineon/cy8cxxa.cfg `
            -c "program $($image.File) verify reset exit" 2>&1 |
            ForEach-Object { $_.ToString() } |
            Tee-Object -FilePath $logPath -Append |
            ForEach-Object { Write-Host $_; $_ }
        $nativeExit = $LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = 'Stop'
        Pop-Location
    }
    if ($nativeExit -ne 0) { throw "OpenOCD failed (exit $nativeExit). See $logPath" }
    if (-not ($output -match '\*\* Verified OK \*\*')) { throw "OpenOCD did not report successful flash verification. See $logPath" }
    Write-Host 'SUCCESS: firmware programmed, flash verified, and board reset.' -ForegroundColor Green
    if ($Firmware -eq 'Original') {
        Write-Host 'Wait at least 5 seconds, then connect XENSIV: Evalkit CSK (UART), COM11.'
    }
    else {
        Write-Host 'Wait at least 5 seconds, open Start-Radar.cmd, then connect to Radar-CSK.'
    }
    Write-Host 'If the application still shows the old firmware identity, reconnect USB power and refresh it.'
    exit 0
}
catch {
    Write-Host "ERROR: $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}

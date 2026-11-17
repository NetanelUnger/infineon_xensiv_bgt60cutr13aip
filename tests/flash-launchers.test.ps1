# Run with Windows PowerShell 5.1. All programming calls use a compiled mock in
# an isolated temporary workspace. This test never opens a hardware programmer.
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
New-Item -ItemType Directory -Path (Join-Path $projectRoot 'logs') -Force | Out-Null
$testRoot = Join-Path ([IO.Path]::GetTempPath()) ('radar launcher test ' + [Guid]::NewGuid().ToString('N'))
$mockRoot = Join-Path $testRoot 'mock programmer'
foreach ($folder in @('scripts', 'recovery', 'release', 'mock programmer/bin', 'mock programmer/scripts/interface', 'mock programmer/scripts/target/infineon')) {
    New-Item -ItemType Directory -Path (Join-Path $testRoot $folder) -Force | Out-Null
}
foreach ($relative in @('Restore-Original.cmd', 'Install-Radar-BLE.cmd', 'scripts/install-firmware.ps1', 'recovery/presence_detection_and_tracking_csk_v1_4_1.hex', 'release/radar-ble-v0.2.0-development.hex')) {
    Copy-Item -LiteralPath (Join-Path $projectRoot $relative) -Destination (Join-Path $testRoot $relative)
}
Set-Content -LiteralPath (Join-Path $mockRoot 'scripts/interface/kitprog3.cfg') -Value '# fixture'
Set-Content -LiteralPath (Join-Path $mockRoot 'scripts/target/infineon/cy8cxxa.cfg') -Value '# fixture'
$mockSource = @'
using System;
public class MockOpenOcd {
    public static int Main(string[] args) {
        string command = String.Join(" ", args);
        if (!command.Contains("adapter serial 110D0E8300023400") ||
            !command.Contains("verify reset exit") || !command.Contains("transport select swd")) return 12;
        Console.Error.WriteLine("Info : MOCK OpenOCD - no hardware access");
        string mode = Environment.GetEnvironmentVariable("RADAR_MOCK_MODE");
        if (mode == "fail") { Console.Error.WriteLine("Error: simulated programming failure"); return 7; }
        if (mode != "no-verify") Console.Error.WriteLine("** Verified OK **");
        Console.WriteLine("** Resetting Target **");
        return 0;
    }
}
'@
Add-Type -TypeDefinition $mockSource -OutputAssembly (Join-Path $mockRoot 'bin/openocd.exe') -OutputType ConsoleApplication
$results = @()
$oldMockMode = $env:RADAR_MOCK_MODE
function Check-Launcher([string]$Name, [string]$File, [string]$Mode, [int]$ExpectedExit, [string]$ExpectedText, [string]$Extra = '') {
    $env:RADAR_MOCK_MODE = $Mode
    $launchPath = Join-Path $testRoot $File
    $command = '"{0}" -OpenOcdRoot "{1}" {2} <nul' -f $launchPath, $mockRoot, $Extra
    $output = & $env:ComSpec /d /c $command 2>&1
    $exitCode = $LASTEXITCODE
    $text = $output -join "`n"
    if ($exitCode -ne $ExpectedExit -or $text -notmatch [regex]::Escape($ExpectedText)) {
        throw "Test failed: $Name, exit $exitCode`n$text"
    }
    if ($ExpectedExit -ne 0 -and $text.Contains('SUCCESS:')) { throw "False success: $Name" }
    [pscustomobject]@{name=$Name; passed=$true; exit_code=$exitCode}
}
try {
    $results += Check-Launcher 'Original: native stderr progress succeeds' 'Restore-Original.cmd' 'ok' 0 'SUCCESS:'
    $results += Check-Launcher 'BLE: pinned development image succeeds' 'Install-Radar-BLE.cmd' 'ok' 0 'SUCCESS:'
    $results += Check-Launcher 'Nonzero programmer exit is rejected' 'Install-Radar-BLE.cmd' 'fail' 1 'OpenOCD failed (exit 7)'
    $results += Check-Launcher 'Exit zero without verify is rejected' 'Restore-Original.cmd' 'no-verify' 1 'did not report successful flash verification'
    $results += Check-Launcher 'CheckOnly never invokes programmer' 'Install-Radar-BLE.cmd' 'fail' 0 'CHECK ONLY completed' '-CheckOnly'
    [IO.File]::AppendAllText((Join-Path $testRoot 'release/radar-ble-v0.2.0-development.hex'), 'corrupted fixture')
    $results += Check-Launcher 'Modified image is rejected before programming' 'Install-Radar-BLE.cmd' 'ok' 1 'Firmware SHA256 mismatch'
    @{hardware_access=$false; temp_workspace=$testRoot; results=$results} | ConvertTo-Json -Depth 5 |
        Set-Content -LiteralPath (Join-Path $projectRoot 'logs/flash-launchers-tests.json') -Encoding UTF8
    $results | Format-Table -AutoSize
}
finally { $env:RADAR_MOCK_MODE = $oldMockMode }

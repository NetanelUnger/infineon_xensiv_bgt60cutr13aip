param([switch]$GetLibraries)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$bash = Join-Path $env:USERPROFILE 'ModusToolbox/tools_3.9/modus-shell/bin/bash.exe'
if (-not (Test-Path -LiteralPath $bash)) { throw "ModusToolbox 3.9 shell not found: $bash" }
Push-Location $projectRoot
try {
    if ($GetLibraries) {
        & $bash --noprofile --norc -c 'export PATH=/usr/bin:$PATH; cd firmware/radar-app; make getlibs'
        if ($LASTEXITCODE -ne 0) { throw 'Library download failed' }
    }
    python scripts/patch-btstack.py
    if ($LASTEXITCODE -ne 0) { throw 'Bluetooth dependency patch failed' }
    & $bash --noprofile --norc -c 'export PATH=/usr/bin:$PATH; cd firmware/radar-app; make -j8 build'
    if ($LASTEXITCODE -ne 0) { throw 'Firmware build failed' }
} finally { Pop-Location }

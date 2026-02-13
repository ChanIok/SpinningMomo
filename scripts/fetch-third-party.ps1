$ErrorActionPreference = "Stop"

Set-Location (Split-Path -Parent $PSScriptRoot)

$header = "third_party/ViGEmClient/include/ViGEm/Client.h"

if (Test-Path $header) {
    Write-Host "ViGEmClient already exists, skip."
    exit 0
}

New-Item -ItemType Directory -Force "third_party" | Out-Null
git clone --depth 1 https://github.com/nefarius/ViGEmClient.git third_party/ViGEmClient

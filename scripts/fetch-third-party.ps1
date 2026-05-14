$ErrorActionPreference = "Stop"

Set-Location (Split-Path -Parent $PSScriptRoot)

New-Item -ItemType Directory -Force "third_party" | Out-Null

$dkmHeader = "third_party/dkm/include/dkm.hpp"
if (Test-Path $dkmHeader) {
    Write-Host "DKM already exists, skip."
} else {
    Write-Host "Cloning DKM..."
    git clone --depth 1 https://github.com/genbattle/dkm.git third_party/dkm
}

$libUltraHdrPath = "third_party/libultrahdr"
$libUltraHdrHeader = "$libUltraHdrPath/ultrahdr_api.h"
$libUltraHdrRef = "v1.4.0"

if (Test-Path $libUltraHdrHeader) {
    Write-Host "libultrahdr already exists, syncing to $libUltraHdrRef..."
    Push-Location $libUltraHdrPath
    try {
        if (Test-Path ".git") {
            git fetch origin tag $libUltraHdrRef --depth 1
            git checkout $libUltraHdrRef
        } else {
            Write-Warning "libultrahdr present but not a git clone; skip version sync. Remove the folder to re-fetch $libUltraHdrRef."
        }
    } finally {
        Pop-Location
    }
} else {
    Write-Host "Cloning libultrahdr $libUltraHdrRef..."
    git clone --depth 1 --branch $libUltraHdrRef https://github.com/google/libultrahdr.git $libUltraHdrPath
}

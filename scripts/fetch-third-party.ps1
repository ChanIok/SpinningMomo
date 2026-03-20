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

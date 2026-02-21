$ErrorActionPreference = "Stop"

Set-Location (Split-Path -Parent $PSScriptRoot)

New-Item -ItemType Directory -Force "third_party" | Out-Null

$vigemHeader = "third_party/ViGEmClient/include/ViGEm/Client.h"
if (Test-Path $vigemHeader) {
    Write-Host "ViGEmClient already exists, skip."
} else {
    Write-Host "Cloning ViGEmClient..."
    git clone --depth 1 https://github.com/nefarius/ViGEmClient.git third_party/ViGEmClient
}

$harmonyFont = "third_party/HarmonyOS Sans/HarmonyOS_Sans_SC/HarmonyOS_Sans_SC_Regular.ttf"
if (Test-Path $harmonyFont) {
    Write-Host "HarmonyOS Sans already exists, skip."
} else {
    $harmonyZipUrl = "https://developer.huawei.com/images/download/general/HarmonyOS-Sans.zip"
    $harmonyZipPath = "third_party/HarmonyOS-Sans.zip"

    Write-Host "Downloading HarmonyOS Sans from official source..."
    Invoke-WebRequest -Uri $harmonyZipUrl -OutFile $harmonyZipPath -Headers @{ "User-Agent" = "Mozilla/5.0" }

    Write-Host "Extracting HarmonyOS Sans..."
    Expand-Archive -Path $harmonyZipPath -DestinationPath "third_party" -Force

    if (-not (Test-Path $harmonyFont)) {
        throw "HarmonyOS Sans download/extract finished but expected font file is missing: $harmonyFont"
    }

    Write-Host "HarmonyOS Sans ready."
}

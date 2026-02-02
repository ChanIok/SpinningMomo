# Build MSI and Bundle installer locally
# Prerequisites: 
#   - .NET SDK 8.0+
#   - WiX v5+: dotnet tool install --global wix --version 5.*
#   - WiX extensions:
#       wix extension add WixToolset.UI.wixext
#       wix extension add WixToolset.BootstrapperApplications.wixext

param(
    [string]$Version = "",
    [switch]$MsiOnly  # Only build MSI, skip Bundle
)

$ErrorActionPreference = "Stop"
$ProjectDir = Split-Path -Parent $PSScriptRoot
Set-Location $ProjectDir

# Extract version from version.hpp if not provided
if ([string]::IsNullOrEmpty($Version)) {
    $versionFile = Get-Content "src/version.hpp" -Raw
    if ($versionFile -match 'VERSION_STR\s+"([^"]+)"') {
        $fullVersion = $matches[1]
        $versionParts = $fullVersion.Split('.')
        $Version = "$($versionParts[0]).$($versionParts[1]).$($versionParts[2])"
    } else {
        Write-Error "Could not extract version from version.hpp"
        exit 1
    }
}

Write-Host "Building SpinningMomo v$Version MSI..." -ForegroundColor Cyan

# Check if WiX is installed
if (-not (Get-Command wix -ErrorAction SilentlyContinue)) {
    Write-Error "WiX v5+ not found. Install with: dotnet tool install --global wix --version 5.*"
    exit 1
}

# Build project if dist doesn't exist or is outdated
if (-not (Test-Path "dist/SpinningMomo.exe")) {
    Write-Host "Building project..." -ForegroundColor Yellow
    npm run build
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

# Build MSI (WiX v5+ uses Files element - no need for heat harvesting)
Write-Host "Building MSI..." -ForegroundColor Yellow
$distDir = Join-Path $ProjectDir "dist"
$outputMsi = Join-Path $distDir "SpinningMomo-$Version-x64.msi"

wix build `
    -arch x64 `
    -d ProductVersion=$Version `
    -d ProjectDir=$ProjectDir `
    -d DistDir=$distDir `
    -ext WixToolset.UI.wixext `
    -culture en-US `
    -loc installer/Package.en-us.wxl `
    -out $outputMsi `
    installer/Package.wxs

if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "Success! Created: $outputMsi" -ForegroundColor Green

if ($MsiOnly) {
    exit 0
}

# Build Bundle (.exe with modern UI)
Write-Host "`nBuilding Bundle installer..." -ForegroundColor Yellow
$outputExe = Join-Path $distDir "SpinningMomo-$Version-x64-Setup.exe"

wix build `
    -arch x64 `
    -d ProductVersion=$Version `
    -d ProjectDir=$ProjectDir `
    -d MsiPath=$outputMsi `
    -ext WixToolset.BootstrapperApplications.wixext `
    -culture en-US `
    -out $outputExe `
    installer/Bundle.wxs

if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "`nSuccess! Created:" -ForegroundColor Green
Write-Host "  MSI:    $outputMsi" -ForegroundColor Green
Write-Host "  Bundle: $outputExe" -ForegroundColor Green

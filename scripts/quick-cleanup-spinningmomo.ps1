# powershell -NoProfile -ExecutionPolicy Bypass -File "./scripts/quick-cleanup-spinningmomo.ps1"

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Test-IsAdministrator {
  $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
  $principal = New-Object Security.Principal.WindowsPrincipal($identity)
  return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Convert-GuidToPackedCode {
  param([Parameter(Mandatory = $true)][string]$GuidText)

  $guid = [Guid]$GuidText
  $n = $guid.ToString('N').ToUpperInvariant()
  $a = $n.Substring(0, 8)
  $b = $n.Substring(8, 4)
  $c = $n.Substring(12, 4)
  $d = $n.Substring(16, 4)
  $e = $n.Substring(20, 12)

  $reverseText = {
    param([string]$s)
    $chars = $s.ToCharArray()
    [array]::Reverse($chars)
    -join $chars
  }

  $swapNibblePairs = {
    param([string]$s)
    $chunks = for ($i = 0; $i -lt $s.Length; $i += 2) { $s.Substring($i, 2) }
    ($chunks | ForEach-Object { $_.Substring(1, 1) + $_.Substring(0, 1) }) -join ''
  }

  "$(& $reverseText $a)$(& $reverseText $b)$(& $reverseText $c)$(& $swapNibblePairs $d)$(& $swapNibblePairs $e)"
}

function Convert-PackedCodeToGuid {
  param([Parameter(Mandatory = $true)][string]$PackedCode)

  if ($PackedCode -notmatch '^[0-9A-Fa-f]{32}$') {
    throw "Invalid packed code: $PackedCode"
  }

  $p = $PackedCode.ToUpperInvariant()
  $a = $p.Substring(0, 8)
  $b = $p.Substring(8, 4)
  $c = $p.Substring(12, 4)
  $d = $p.Substring(16, 4)
  $e = $p.Substring(20, 12)

  $reverseText = {
    param([string]$s)
    $chars = $s.ToCharArray()
    [array]::Reverse($chars)
    -join $chars
  }

  $swapNibblePairs = {
    param([string]$s)
    $chunks = for ($i = 0; $i -lt $s.Length; $i += 2) { $s.Substring($i, 2) }
    ($chunks | ForEach-Object { $_.Substring(1, 1) + $_.Substring(0, 1) }) -join ''
  }

  $guidN = "$(& $reverseText $a)$(& $reverseText $b)$(& $reverseText $c)$(& $swapNibblePairs $d)$(& $swapNibblePairs $e)"
  return ([Guid]::ParseExact($guidN, 'N').ToString('D').ToUpperInvariant())
}

function Remove-KeyIfExists {
  param([Parameter(Mandatory = $true)][string]$RegPath)
  if (Test-Path "Registry::$RegPath") {
    Remove-Item -Path "Registry::$RegPath" -Recurse -Force -ErrorAction SilentlyContinue
    Write-Host "Removed key: $RegPath"
  }
}

function Remove-PathIfExists {
  param([Parameter(Mandatory = $true)][string]$Path)
  if (Test-Path $Path) {
    Remove-Item -Path $Path -Recurse -Force -ErrorAction SilentlyContinue
    Write-Host "Removed path: $Path"
  }
}

if (-not (Test-IsAdministrator)) {
  throw "Please run this script in an elevated PowerShell (Run as Administrator)."
}

Write-Host "Quick cleanup started..." -ForegroundColor Cyan

# SpinningMomo key component GUIDs (for occupancy detection)
$componentGuids = @(
  '{CA8D282A-3752-4E74-9252-76AB6F280997}', # DesktopShortcut
  '{81DD2135-CFFF-4EAD-902C-D25BD1C5612B}', # DesktopShortcutRemove
  '{7808BA3C-C658-440F-976C-838362DD1FFF}', # StartMenuShortcut
  '{5E694D68-9DFD-4510-9EA1-698F8A09739D}', # StartMenuShortcutRemove
  '{7DF1D902-6FF4-43EF-B58A-837AE33B4494}'  # CleanupUserData
)

$componentPacked = @{}
foreach ($g in $componentGuids) {
  $normalized = ([Guid]$g).ToString('D').ToUpperInvariant()
  $componentPacked[$normalized] = Convert-GuidToPackedCode $normalized
}

$userDataRoot = 'Registry::HKEY_LOCAL_MACHINE\Software\Microsoft\Windows\CurrentVersion\Installer\UserData'
$detectedProductPacked = New-Object System.Collections.Generic.HashSet[string]

# 1) Detect all product packed codes occupying our components.
if (Test-Path $userDataRoot) {
  foreach ($sidRoot in (Get-ChildItem -Path $userDataRoot -ErrorAction SilentlyContinue)) {
    foreach ($packedComp in $componentPacked.Values) {
      $ck = Join-Path $sidRoot.PSPath "Components\$packedComp"
      if (-not (Test-Path $ck)) { continue }
      $item = Get-ItemProperty -Path $ck -ErrorAction SilentlyContinue
      if ($null -eq $item) { continue }

      foreach ($prop in $item.PSObject.Properties) {
        if ($prop.Name -in @('PSPath', 'PSParentPath', 'PSChildName', 'PSDrive', 'PSProvider')) { continue }
        if ($prop.Name -notmatch '^[0-9A-Fa-f]{32}$') { continue }
        if ($prop.Name -eq '00000000000000000000000000000000') { continue }
        $null = $detectedProductPacked.Add($prop.Name.ToUpperInvariant())
      }
    }
  }
}

$detectedProductGuids = @()
foreach ($pp in $detectedProductPacked) {
  try {
    $detectedProductGuids += "{$(Convert-PackedCodeToGuid $pp)}"
  }
  catch {
    # ignore malformed packed code
  }
}

Write-Host "Detected product codes:"
if ($detectedProductGuids.Count -eq 0) {
  Write-Host " - (none)" -ForegroundColor Yellow
} else {
  $detectedProductGuids | Sort-Object -Unique | ForEach-Object { Write-Host " - $_" }
}

# 2) Remove all matching product client values from Installer\UserData\*\Components\*
if ((Test-Path $userDataRoot) -and $detectedProductPacked.Count -gt 0) {
  foreach ($sidRoot in (Get-ChildItem -Path $userDataRoot -ErrorAction SilentlyContinue)) {
    $componentsRoot = Join-Path $sidRoot.PSPath 'Components'
    if (-not (Test-Path $componentsRoot)) { continue }
    foreach ($ck in (Get-ChildItem -Path $componentsRoot -ErrorAction SilentlyContinue)) {
      $keyPath = $ck.PSPath
      $item = Get-ItemProperty -Path $keyPath -ErrorAction SilentlyContinue
      if ($null -eq $item) { continue }
      $regPath = $keyPath -replace '^Microsoft\.PowerShell\.Core\\Registry::', ''
      foreach ($pp in $detectedProductPacked) {
        if ($item.PSObject.Properties.Name -contains $pp) {
          Remove-ItemProperty -Path "Registry::$regPath" -Name $pp -Force -ErrorAction SilentlyContinue
          Write-Host "Removed component client: $pp @ $regPath"
        }
      }
    }
  }
}

# 3) Remove uninstall entries by detected product codes + display name fallback.
foreach ($pg in ($detectedProductGuids | Sort-Object -Unique)) {
  Remove-KeyIfExists "HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Uninstall\$pg"
  Remove-KeyIfExists "HKEY_LOCAL_MACHINE\Software\Microsoft\Windows\CurrentVersion\Uninstall\$pg"
  Remove-KeyIfExists "HKEY_LOCAL_MACHINE\Software\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\$pg"
}

$uninstallRoots = @(
  'HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall',
  'HKLM:\Software\Microsoft\Windows\CurrentVersion\Uninstall',
  'HKLM:\Software\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall'
)
foreach ($root in $uninstallRoots) {
  if (-not (Test-Path $root)) { continue }
  foreach ($k in (Get-ChildItem -Path $root -ErrorAction SilentlyContinue)) {
    $p = Get-ItemProperty -Path $k.PSPath -ErrorAction SilentlyContinue
    if ($null -eq $p) { continue }
    $hasDisplayName = $p.PSObject.Properties.Name -contains 'DisplayName'
    if ($hasDisplayName -and $p.DisplayName -eq 'SpinningMomo') {
      Remove-Item -Path $k.PSPath -Recurse -Force -ErrorAction SilentlyContinue
      Write-Host "Removed uninstall entry by DisplayName: $($k.PSChildName)"
    }
  }
}

# 4) Remove app-specific registry key.
Remove-KeyIfExists 'HKEY_CURRENT_USER\Software\SpinningMomo'

# 5) Remove package cache folders related to detected product codes and bundle code entries.
$packageCacheRoot = "$env:LOCALAPPDATA\Package Cache"
if (Test-Path $packageCacheRoot) {
  foreach ($pg in ($detectedProductGuids | Sort-Object -Unique)) {
    $prefix = Join-Path $packageCacheRoot ($pg + 'v*')
    foreach ($m in (Get-ChildItem -Path $prefix -ErrorAction SilentlyContinue)) {
      Remove-PathIfExists $m.FullName
    }
  }
  foreach ($m in (Get-ChildItem -Path (Join-Path $packageCacheRoot '{*}') -ErrorAction SilentlyContinue)) {
    $setupExe = Join-Path $m.FullName 'SpinningMomo-*-Setup.exe'
    $hasSetup = Get-ChildItem -Path $setupExe -ErrorAction SilentlyContinue
    if ($hasSetup) {
      Remove-PathIfExists $m.FullName
    }
  }
}

# 6) Remove file leftovers (does not touch repo files like version.json).
Remove-PathIfExists "$env:USERPROFILE\Desktop\SpinningMomo.lnk"
Remove-PathIfExists "$env:APPDATA\Microsoft\Windows\Start Menu\Programs\SpinningMomo\SpinningMomo.lnk"
Remove-PathIfExists "$env:LOCALAPPDATA\Programs\SpinningMomo"
Remove-PathIfExists "$env:LOCALAPPDATA\SpinningMomo"

Write-Host "Quick cleanup finished." -ForegroundColor Green

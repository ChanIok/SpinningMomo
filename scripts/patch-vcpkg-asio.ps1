param(
    [string]$VcpkgRoot = $env:VCPKG_ROOT
)

$ErrorActionPreference = "Stop"

function Fail([string]$Message) {
    Write-Error $Message
    exit 1
}

function Get-FileText([string]$Path) {
    return [System.IO.File]::ReadAllText($Path)
}

function Test-GitApply([string[]]$Arguments) {
    & git @Arguments *> $null
    return ($LASTEXITCODE -eq 0)
}

function Apply-AsioPatch([string]$ResolvedVcpkgRoot, [string]$Triplet, [string]$PatchFileName) {
    $patchPath = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot "../patches/$PatchFileName"))
    $asioRoot = Join-Path $ResolvedVcpkgRoot "installed/$Triplet/include/asio"
    $versionPath = Join-Path $asioRoot "version.hpp"
    $configPath = Join-Path $asioRoot "detail/config.hpp"
    $awaitablePath = Join-Path $asioRoot "use_awaitable.hpp"

    if (-not (Test-Path -LiteralPath $asioRoot)) {
        Write-Host "skip triplet ${Triplet}: Asio not installed."
        return $false
    }

    if (-not (Test-Path -LiteralPath $patchPath)) {
        Fail "未找到 patch 文件：$patchPath"
    }

    if (-not (Test-Path -LiteralPath $versionPath)) {
        Fail "未找到文件：$versionPath"
    }

    if (-not (Test-Path -LiteralPath $configPath)) {
        Fail "未找到文件：$configPath"
    }

    if (-not (Test-Path -LiteralPath $awaitablePath)) {
        Fail "未找到文件：$awaitablePath"
    }

    $versionContent = Get-FileText $versionPath
    $expectedVersionLine = "#define ASIO_VERSION 103200 // 1.32.0"
    if (-not $versionContent.Contains($expectedVersionLine)) {
        Fail "triplet ${Triplet} 的 Asio 版本不是 1.32.0，脚本拒绝继续。请先更新 patch。"
    }

    $configContent = Get-FileText $configPath
    $awaitableContent = Get-FileText $awaitablePath
    $alreadyApplied =
        $configContent.Contains("ASIO_INLINE_VARIABLE_EXTERNAL") -and
        $awaitableContent.Contains("ASIO_INLINE_VARIABLE_EXTERNAL constexpr use_awaitable_t<> use_awaitable")

    if ($alreadyApplied) {
        Write-Host "already applied: ${Triplet}"
        return $true
    }

    $forwardCheckArgs = @("-C", $ResolvedVcpkgRoot, "apply", "--check", "--ignore-whitespace", $patchPath)
    $canApply = Test-GitApply $forwardCheckArgs
    if (-not $canApply) {
        Fail "patch 无法应用到 triplet ${Triplet}。Asio 内容可能已变化。"
    }

    & git -C $ResolvedVcpkgRoot apply --ignore-whitespace $patchPath
    if ($LASTEXITCODE -ne 0) {
        Fail "git apply 执行失败，triplet: ${Triplet}"
    }

    Write-Host "patched        : ${Triplet}"
    Write-Host "patch file     : $patchPath"
    return $true
}

if ([string]::IsNullOrWhiteSpace($VcpkgRoot)) {
    Fail "未找到 VCPKG_ROOT。请先设置环境变量 VCPKG_ROOT。"
}

$resolvedVcpkgRoot = [System.IO.Path]::GetFullPath($VcpkgRoot)
$patchedAny = $false
$patchedAny = (Apply-AsioPatch $resolvedVcpkgRoot "x64-windows-static-md" "asio-msvc-header-unit-x64-windows-static-md.patch") -or $patchedAny
$patchedAny = (Apply-AsioPatch $resolvedVcpkgRoot "x64-windows-static" "asio-msvc-header-unit-x64-windows-static.patch") -or $patchedAny

if (-not $patchedAny) {
    Fail "未找到可处理的 Asio 安装目录。预期目录：installed/x64-windows-static-md/include/asio 或 installed/x64-windows-static/include/asio"
}

Write-Host ""
Write-Host "Asio patch check complete."
Write-Host "VCPKG_ROOT: $resolvedVcpkgRoot"

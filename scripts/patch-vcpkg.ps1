#
# vcpkg 依赖 patch 统一入口
#
# 当前已自动化：
# 1. Asio
#    当项目使用 `import <asio.hpp>;` 以头文件单元方式导入 Asio 时，
#    MSVC + header unit 会让一批 namespace-scope constexpr / inline 变量
#    在链接期丢失稳定定义，典型表现为：
#      - `asio::use_awaitable`
#      - `asio::this_coro::executor`
#      - `asio::execution::allocator`
#      - `asio::execution::context`
#      - `asio::execution::blocking`
#    最终报大量 `LNK2001`。
#    这里通过 patch 把这些变量改成带外部链接语义的定义，
#    规避 header unit 导入时的链接问题。
#
# 2. WIL / strsafe / memcpy_s
#    `import <wil/com.h>;` 目前也会触发 header unit 兼容问题，但性质不同：
#    它会先后炸在 Windows SDK / UCRT 的 `strsafe.h`、`corecrt_memcpy_s.h`
#    这类老式 static inline 实现上，典型报错包括：
#      - `StringValidateDestW` 的 `C2129`
#      - `memcpy_s` 的 `C2129`
#    这说明 WIL 的问题不只是单个头，而是其依赖的 SDK / UCRT 头链在
#    header unit 路径下并不稳定。
#    这里的 patch 只收敛到 WIL 自身：
#      - 在 WIL 头链内部启用 `STRSAFE_LIB`
#      - 内聚 `strsafe.lib` / `legacy_stdio_definitions.lib`
#      - 在导入 `Windows.h` 时把 UCRT 的 `_CRT_MEMCPY_S_INLINE` 调整为 `inline`
#      - 把 WIL 内部少量 `memcpy_s` 调用改成显式边界前提下的 `CopyMemory`
#    目标是修 `import <wil/com.h>;`，而不是改整个工程的 CRT / SDK 默认行为。
#
# 3. uWebSockets / libuSockets
#    `import <uwebsockets/App.h>;` 在 MSVC header unit 下目前会遇到几类问题：
#      - `libusockets.h` 没有显式前置声明 `us_listen_socket_t`
#      - `ChunkedEncoding.h` 的 `static getNextChunk(...)` 在导入路径下报 `C2129`
#      - `LoopData.h` / `CachingApp.h` 里的 `time(0)` 依赖 UCRT `time.h` 的
#        `static __inline time(...)`，在导入路径下也会报 `C2129`
#      - `LoopData.h` 里的 `gmtime_s(...)` 依赖 UCRT `time.h` 的
#        `static __inline gmtime_s(...)`，在导入路径下也会报 `C2129`
#      - `Loop.h` 的 `static thread_local LoopCleaner` 会在链接期炸 `__tlregdtor`
#      - `HttpParser.h` 的 `MINIMUM_HTTP_POST_PADDING` 会在链接期炸 `LNK2001`
#    这里的 patch 先只修已经确认的最小问题面：
#      - 为 `us_listen_socket_t` 补显式前置声明
#      - 把 `getNextChunk(...)` 从 `static` 改成 `inline`
#      - 把头内 `time(0)` 改成 `_time64(nullptr)`
#      - 把头内 `gmtime_s(...)` 改成 `_gmtime64_s(...)`
#      - 把 `LoopCleaner` 改成无析构的 TLS 状态，避免 `__tlregdtor`
#      - 把 `MINIMUM_HTTP_POST_PADDING` 改成 `inline constexpr`
#    这只能作为最小推进 patch，不代表 `uwebsockets` 已完全 header-unit ready。
#
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

function Apply-AsioHeaderUnitPatch([string]$ResolvedVcpkgRoot, [string]$Triplet, [string]$PatchFileName) {
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
        Fail "Patch file not found: $patchPath"
    }

    if (-not (Test-Path -LiteralPath $versionPath)) {
        Fail "File not found: $versionPath"
    }

    if (-not (Test-Path -LiteralPath $configPath)) {
        Fail "File not found: $configPath"
    }

    if (-not (Test-Path -LiteralPath $awaitablePath)) {
        Fail "File not found: $awaitablePath"
    }

    $versionContent = Get-FileText $versionPath
    $expectedVersionLine = "#define ASIO_VERSION 103200 // 1.32.0"
    if (-not $versionContent.Contains($expectedVersionLine)) {
        Fail "Asio for triplet ${Triplet} is not version 1.32.0; aborting. Update the patch first."
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
        Fail "Patch cannot be applied to triplet ${Triplet}; Asio sources may have changed."
    }

    & git -C $ResolvedVcpkgRoot apply --ignore-whitespace $patchPath
    if ($LASTEXITCODE -ne 0) {
        Fail "git apply failed for triplet: ${Triplet}"
    }

    Write-Host "patched        : ${Triplet}"
    Write-Host "patch file     : $patchPath"
    return $true
}

function Apply-WilHeaderUnitPatch([string]$ResolvedVcpkgRoot, [string]$Triplet, [string]$PatchFileName) {
    $patchPath = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot "../patches/$PatchFileName"))
    $wilRoot = Join-Path $ResolvedVcpkgRoot "installed/$Triplet/include/wil"
    $spdxPath = Join-Path $ResolvedVcpkgRoot "installed/$Triplet/share/wil/vcpkg.spdx.json"
    $resultMacrosPath = Join-Path $wilRoot "result_macros.h"
    $resultPath = Join-Path $wilRoot "result.h"
    $resourcePath = Join-Path $wilRoot "resource.h"

    if (-not (Test-Path -LiteralPath $wilRoot)) {
        Write-Host "skip triplet ${Triplet}: WIL not installed."
        return $false
    }

    if (-not (Test-Path -LiteralPath $patchPath)) {
        Fail "Patch file not found: $patchPath"
    }

    if (-not (Test-Path -LiteralPath $spdxPath)) {
        Fail "File not found: $spdxPath"
    }

    if (-not (Test-Path -LiteralPath $resultMacrosPath)) {
        Fail "File not found: $resultMacrosPath"
    }

    if (-not (Test-Path -LiteralPath $resultPath)) {
        Fail "File not found: $resultPath"
    }

    if (-not (Test-Path -LiteralPath $resourcePath)) {
        Fail "File not found: $resourcePath"
    }

    $spdxContent = Get-FileText $spdxPath
    $expectedVersionLine = '"versionInfo": "1.0.260126.7"'
    if (-not $spdxContent.Contains($expectedVersionLine)) {
        Fail "WIL for triplet ${Triplet} is not version 1.0.260126.7; aborting. Update the patch first."
    }

    $resultMacrosContent = Get-FileText $resultMacrosPath
    $resultContent = Get-FileText $resultPath
    $resourceContent = Get-FileText $resourcePath
    $alreadyApplied =
        $resultMacrosContent.Contains('WIL header unit compatibility patch for MSVC') -and
        $resultMacrosContent.Contains('#define _CRT_MEMCPY_S_INLINE inline') -and
        $resultMacrosContent.Contains('CopyMemory(pCopyRefCount + 1, pData, cbData)') -and
        $resultContent.Contains('CopyMemory(callContextString, context.contextName, copyBytes)') -and
        $resourceContent.Contains('CopyMemory(result, source, bytesToCopy)')

    if ($alreadyApplied) {
        Write-Host "already applied: ${Triplet}"
        return $true
    }

    $forwardCheckArgs = @("-C", $ResolvedVcpkgRoot, "apply", "--check", "--ignore-whitespace", $patchPath)
    $canApply = Test-GitApply $forwardCheckArgs
    if (-not $canApply) {
        Fail "Patch cannot be applied to triplet ${Triplet}; WIL sources may have changed."
    }

    & git -C $ResolvedVcpkgRoot apply --ignore-whitespace $patchPath
    if ($LASTEXITCODE -ne 0) {
        Fail "git apply failed for triplet: ${Triplet}"
    }

    Write-Host "patched        : ${Triplet}"
    Write-Host "patch file     : $patchPath"
    return $true
}

function Apply-UWebSocketsHeaderUnitPatch([string]$ResolvedVcpkgRoot, [string]$Triplet, [string]$PatchFileName) {
    $patchPath = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot "../patches/$PatchFileName"))
    $sharePath = Join-Path $ResolvedVcpkgRoot "installed/$Triplet/share/uwebsockets/vcpkg.spdx.json"
    $chunkedPath = Join-Path $ResolvedVcpkgRoot "installed/$Triplet/include/uwebsockets/ChunkedEncoding.h"
    $libusocketsPath = Join-Path $ResolvedVcpkgRoot "installed/$Triplet/include/libusockets.h"
    $loopDataPath = Join-Path $ResolvedVcpkgRoot "installed/$Triplet/include/uwebsockets/LoopData.h"
    $loopPath = Join-Path $ResolvedVcpkgRoot "installed/$Triplet/include/uwebsockets/Loop.h"
    $cachingAppPath = Join-Path $ResolvedVcpkgRoot "installed/$Triplet/include/uwebsockets/CachingApp.h"
    $httpParserPath = Join-Path $ResolvedVcpkgRoot "installed/$Triplet/include/uwebsockets/HttpParser.h"

    if (-not (Test-Path -LiteralPath $chunkedPath)) {
        Write-Host "skip triplet ${Triplet}: uWebSockets not installed."
        return $false
    }

    if (-not (Test-Path -LiteralPath $patchPath)) {
        Fail "Patch file not found: $patchPath"
    }

    if (-not (Test-Path -LiteralPath $sharePath)) {
        Fail "File not found: $sharePath"
    }

    if (-not (Test-Path -LiteralPath $libusocketsPath)) {
        Fail "File not found: $libusocketsPath"
    }

    if (-not (Test-Path -LiteralPath $loopDataPath)) {
        Fail "File not found: $loopDataPath"
    }

    if (-not (Test-Path -LiteralPath $loopPath)) {
        Fail "File not found: $loopPath"
    }

    if (-not (Test-Path -LiteralPath $cachingAppPath)) {
        Fail "File not found: $cachingAppPath"
    }

    if (-not (Test-Path -LiteralPath $httpParserPath)) {
        Fail "File not found: $httpParserPath"
    }

    $spdxContent = Get-FileText $sharePath
    $expectedVersionLine = '"name": "uwebsockets:' + $Triplet + '@20.77.0'
    if (-not $spdxContent.Contains($expectedVersionLine)) {
        Fail "uwebsockets for triplet ${Triplet} is not version 20.77.0; aborting. Update the patch first."
    }

    $chunkedContent = Get-FileText $chunkedPath
    $libusocketsContent = Get-FileText $libusocketsPath
    $loopDataContent = Get-FileText $loopDataPath
    $loopContent = Get-FileText $loopPath
    $cachingAppContent = Get-FileText $cachingAppPath
    $httpParserContent = Get-FileText $httpParserPath
    $alreadyApplied =
        $chunkedContent.Contains('inline std::optional<std::string_view> getNextChunk') -and
        $libusocketsContent.Contains('struct us_listen_socket_t;') -and
        $loopDataContent.Contains('cacheTimepoint = _time64(nullptr);') -and
        $loopDataContent.Contains('_gmtime64_s(&tstruct, &cacheTimepoint);') -and
        $loopContent.Contains('static thread_local LazyLoopState lazyLoop;') -and
        $httpParserContent.Contains('inline constexpr unsigned int MINIMUM_HTTP_POST_PADDING = 32;') -and
        $cachingAppContent.Contains('created = _time64(nullptr);')

    if ($alreadyApplied) {
        Write-Host "already applied: ${Triplet}"
        return $true
    }

    $forwardCheckArgs = @("-C", $ResolvedVcpkgRoot, "apply", "--check", "--ignore-whitespace", $patchPath)
    $canApply = Test-GitApply $forwardCheckArgs
    if (-not $canApply) {
        Fail "Patch cannot be applied to triplet ${Triplet}; uWebSockets sources may have changed."
    }

    & git -C $ResolvedVcpkgRoot apply --ignore-whitespace $patchPath
    if ($LASTEXITCODE -ne 0) {
        Fail "git apply failed for triplet: ${Triplet}"
    }

    Write-Host "patched        : ${Triplet}"
    Write-Host "patch file     : $patchPath"
    return $true
}

if ([string]::IsNullOrWhiteSpace($VcpkgRoot)) {
    Fail "VCPKG_ROOT is not set. Set the VCPKG_ROOT environment variable first."
}

$resolvedVcpkgRoot = [System.IO.Path]::GetFullPath($VcpkgRoot)
$patchedAny = $false
$patchedAny = (Apply-AsioHeaderUnitPatch $resolvedVcpkgRoot "x64-windows-static-md" "asio-msvc-header-unit-x64-windows-static-md.patch") -or $patchedAny
$patchedAny = (Apply-AsioHeaderUnitPatch $resolvedVcpkgRoot "x64-windows-static" "asio-msvc-header-unit-x64-windows-static.patch") -or $patchedAny
$patchedAny = (Apply-WilHeaderUnitPatch $resolvedVcpkgRoot "x64-windows-static-md" "wil-msvc-header-unit-x64-windows-static-md.patch") -or $patchedAny
$patchedAny = (Apply-WilHeaderUnitPatch $resolvedVcpkgRoot "x64-windows-static" "wil-msvc-header-unit-x64-windows-static.patch") -or $patchedAny
$patchedAny = (Apply-UWebSocketsHeaderUnitPatch $resolvedVcpkgRoot "x64-windows-static-md" "uwebsockets-msvc-header-unit-x64-windows-static-md.patch") -or $patchedAny
$patchedAny = (Apply-UWebSocketsHeaderUnitPatch $resolvedVcpkgRoot "x64-windows-static" "uwebsockets-msvc-header-unit-x64-windows-static.patch") -or $patchedAny

if (-not $patchedAny) {
    Fail "No applicable vcpkg install directories found. This script expects at least one of Asio, WIL, or uWebSockets to be installed."
}

Write-Host ""
Write-Host "vcpkg patch check complete."
Write-Host "VCPKG_ROOT: $resolvedVcpkgRoot"

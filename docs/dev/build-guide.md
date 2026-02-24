# 构建指南

## 环境要求

| 工具 | 要求 | 说明 |
|------|------|------|
| **Visual Studio 2022+** | 含「使用 C++ 的桌面开发」工作负载 | 需在工作负载中额外勾选「**C++ 模块（针对标准库的 MSVC v143）**」|
| **Windows SDK** | 10.0.22621.0+（Windows 11 SDK） | |
| **xmake** | 最新版 | C++ 构建系统，管理 vcpkg 依赖 |
| **Node.js** | v20+ | Web 前端构建及 npm 脚本 |
| **Python 3** | 含 `pip` | 用于生成 HarmonyOS 字体子集 |

### 安装 xmake

```powershell
# PowerShell（推荐）
iwr -useb https://xmake.io/psget.txt | iex

# 或前往官网下载安装包
# https://xmake.io/#/getting_started?id=installation
```

> xmake 会通过 `xmake-requires.lock` 自动调用 vcpkg 下载和编译 C++ 依赖，**无需手动安装 vcpkg**。

---

## 依赖准备

### 1. 获取第三方依赖

```powershell
.\scripts\fetch-third-party.ps1
```

### 2. 安装 npm 依赖

```bash
# 根目录（构建脚本依赖）
npm install

# Web 前端依赖
cd web && npm ci
```

### 3. 生成字体子集（可选，用于内嵌 HarmonyOS 字体）

```powershell
pip install fonttools brotli zopfli
npm run font:build:harmonyos
```

---

## 构建

### 完整构建（推荐）

```bash
# 一键完成：C++ Release + Web 前端 + 打包 dist/
npm run build:ci
```

产物位于 `dist/` 目录。

### 分步构建

```bash
# C++ 后端 - Debug（日常开发）
xmake config -m debug
xmake build

# C++ 后端 - Release
xmake release    # 构建 release 后自动恢复 debug 配置

# Web 前端
cd web && npm run build

# 打包 dist/（汇总 exe + web 资源）
npm run build:prepare
```

### 构建输出路径

| 构建类型 | 路径 |
|----------|------|
| Debug | `build\windows\x64\debug\` |
| Release | `build\windows\x64\release\` |
| 打包产物 | `dist\` |

---

## 打包发布产物

### 便携版（ZIP）

```bash
npm run build:portable
```

### MSI 安装包

需要额外安装 WiX Toolset v6：

```bash
dotnet tool install --global wix --version 6.0.2
wix extension add WixToolset.UI.wixext/6.0.2 --global
wix extension add WixToolset.BootstrapperApplications.wixext/6.0.2 --global
```

然后运行：

```powershell
.\scripts\build-msi.ps1 -Version "x.y.z"
```

---

## Web 前端开发

启动开发服务器（需 C++ 后端同时运行）：

```bash
cd web && npm run dev
```

Vite 开发服务器会将 `/rpc` 和 `/static` 代理到 C++ 后端（`localhost:51206`）。

---

## 代码生成脚本

修改以下源文件后需重新运行对应脚本：

| 修改内容 | 需运行的脚本 |
|----------|-------------|
| `src/migrations/*.sql` | `node scripts/generate-migrations.js` |
| `src/locales/*.json` | `node scripts/generate-embedded-locales.js` |

---
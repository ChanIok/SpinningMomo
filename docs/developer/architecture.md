# 架构与构建

> [!NOTE]
> 本项目更推荐先通过 Issue 讨论需求和方向，再提交 Pull Request。
>
> 对于新功能、行为调整或较大的重构，请先提交 Issue，说明要解决的问题、使用场景和预期效果。这样可以在开始编码前确认它是否符合项目定位。
>
> 已确认范围的 Issue、明确的 Bug 修复、文档改进，以及经过讨论的技术难题，都非常欢迎通过 PR 贡献。未经讨论的功能性 PR 可能会因为方向不一致而无法合并。

## 架构与代码规范说明

本项目核心采用 C++23 Modules 与 Vue 3 混合双端架构。
关于详细的设计哲学、C++ 组件系统划分以及所有的模块依赖关系，已在此仓库根目录维护了最新的 **[`AGENTS.md`](https://github.com/ChanIok/SpinningMomo/blob/main/AGENTS.md)**。

## 环境要求

| 工具 | 要求 | 说明 |
|------|------|------|
| **Visual Studio 2022** | 含「使用 C++ 的桌面开发」工作负载 | 需勾选「**C++ 模块（针对标准库的 MSVC v143）**」；**不要使用 VS 2026**，会出现编译错误 |
| **Windows SDK** | 10.0.22621.0+（Windows 11 SDK） | |
| **Git** | 最新版 | 克隆 vcpkg 与 `fetch-third-party.ps1` |
| **xmake** | 最新版 | C++ 构建系统 |
| **Node.js** | v20+ | Web 前端构建及 npm 脚本 |

### 安装 xmake

```powershell
# PowerShell（推荐）
irm https://xmake.io/psget.text | iex

# 或前往官网下载安装包
# https://xmake.io/#/getting_started?id=installation
```

### 准备 vcpkg

```powershell
git clone https://github.com/microsoft/vcpkg.git D:\dev\vcpkg  # 路径自定
cd D:\dev\vcpkg
.\bootstrap-vcpkg.bat
.\vcpkg.exe integrate install
```

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

```bash
npm run build:installer
```

---

## Web 前端开发

启动开发服务器（需 C++ 后端同时运行）：

```bash
npm run dev:web
```

Vite 开发服务器会将 `/rpc` 和 `/static` 代理到 C++ 后端（`localhost:51206`）。

---

## 代码生成脚本

修改以下源文件后需重新运行对应脚本：

| 修改内容 | 需运行的脚本 |
|----------|-------------|
| `src/migrations/*.sql` | `node scripts/generate-migrations.js` |
| `src/locales/*.json` | `node scripts/generate-embedded-locales.js` |

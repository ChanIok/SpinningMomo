# 架构与构建

> [!NOTE]
> 本项目更推荐先通过 Issue 讨论需求和方向，再提交 Pull Request。
>
> 对于新功能、行为调整或较大的重构，请先提交 Issue，说明要解决的问题、使用场景和预期效果。这样可以在开始编码前确认它是否符合项目定位。
>
> 已确认范围的 Issue、明确的 Bug 修复、文档改进，以及经过讨论的技术难题，都非常欢迎通过 PR 贡献。未经讨论的功能性 PR 可能会因为方向不一致而无法合并。

## 架构与代码规范说明

本项目核心采用 C++23 原生后端与 Vue 3 Web 前端的混合双端架构。关于详细的设计哲学、C++ 组件划分以及依赖关系，已在此仓库根目录维护了最新的 **[`AGENTS.md`](https://github.com/ChanIok/SpinningMomo/blob/main/AGENTS.md)**。

## 环境要求

C++ 后端默认使用 `clang-cl[llvm]`（Clang + LLD）进行日常开发，正式发布使用 MSVC。

| 工具 | 要求 | 说明 |
|------|------|------|
| **Visual Studio 2026 / Build Tools** | 安装「使用 C++ 的桌面开发」及 C++ Clang 工具 | Visual Studio IDE 可选 |
| **Windows SDK** | 10.0.22621.0+（Windows 11 SDK） | |
| **Git** | 最新版 | 克隆 vcpkg 与获取第三方依赖 |
| **xmake** | 3.1.0 | C++ 构建系统 |
| **Node.js** | v22.13+ | Web 前端构建及 pnpm 脚本 |

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

```bash
pnpm run fetch:third-party
```

### 2. 安装 pnpm 依赖

```bash
# 安装根项目及所有 workspace 项目依赖
pnpm install
```

### 3. 初始化 xmake 依赖并应用补丁

```bash
node scripts/patch-xmake-clang-cl-cxx23.js

# Clang-cl + LLD（默认）
xmake f --toolchain="clang-cl[llvm]" -y

# 或使用 MSVC
# xmake f --toolchain=msvc -y

xmake f -m release -y && xmake f -m debug -y
node scripts/patch-vcpkg.js
```

---

## 使用 Visual Studio IDE 开发（可选）

如需使用 Visual Studio 浏览、编辑和调试 C++ 代码，可生成由 Xmake 管理的解决方案：

```powershell
xmake vs
```

生成后打开：

```text
vsxmake2026\SpinningMomo.sln
```

---

## 构建

> [!TIP]
> 如果在本地搭建或构建过程中遇到工具链、依赖或环境问题，建议参考 GitHub CI 的 [Build Release 工作流](https://github.com/ChanIok/SpinningMomo/blob/main/.github/workflows/build-release.yml)，它记录了当前最新且自动化跑通的标准环境配置与构建顺序。

### 完整构建（推荐）

```bash
# 一键完成：C++ Release + Web 前端 + 打包 dist/
pnpm run build
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
pnpm --filter web run build

# 打包 dist/（汇总 exe + web 资源）
pnpm run build:dist
```

### 构建输出路径

| 构建类型 | 路径 |
|----------|------|
| Debug | `build\windows\x64\debug\` |
| Release | `build\windows\x64\release\` |
| 打包产物 | `dist\` |

### 后端自动化测试

后端回归测试使用 doctest，并由独立的 `SpinningMomoTests` 目标承载：

```bash
xmake test -v
```

测试只保护确定性的稳定行为和已记录不变量，不以覆盖率为目标。涉及窗口、显卡、
音频设备和其他 Windows 桌面环境的行为仍需运行应用进行手工验证。

---

## 打包发布产物

### 便携版（ZIP）

```bash
pnpm run build:portable
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
pnpm run build:installer
```

---

## Web 前端开发

启动开发服务器（需 C++ 后端同时运行）：

```bash
pnpm run dev:web
```

Vite 开发服务器会将 `/rpc` 和 `/static` 代理到 C++ 后端（`localhost:51206`）。

---

## 代码生成脚本

修改以下源文件后需重新运行对应脚本：

| 修改内容 | 需运行的脚本 |
|----------|-------------|
| `src/migrations/*.sql` | `node scripts/generate-migrations.js` |
| `src/locales/*.json` | `node scripts/generate-embedded-locales.js` |

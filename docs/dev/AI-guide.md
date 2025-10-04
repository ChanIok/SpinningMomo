# AI-guide.md

本文件为AI在处理此代码库中的代码时提供指导。

**系统环境**: Windows 11 + PowerShell

## 项目概述

SpinningMomo (旋转吧大喵) 正在升级为一款游戏照片管理工具，主要针对游戏"无限暖暖"的截图进行管理和操作。该项目由一个现代 C++23 原生 Windows 应用程序和一个 React/TypeScript 基于 Web 的 UI 组成，Web前端用于管理照片等操作。

## 构建说明

### C++ 后端

```powershell
# 构建 C++ 应用程序
xmake build
```

### Web 前端

```powershell
cd web

npm run build
```

## 架构概述

### 代码风格

**重要**: 本项目C++ 后端不使用面向对象编程(OOP)，采用以下设计模式：
- **POD结构体 + 自由函数**: 使用纯数据(Plain Old Data)结构体配合自由函数
- **集中式状态管理**: 使用 `AppState` 进行集中式状态管理
- **模块化设计**: 使用现代 C++23 模块系统（`.ixx` 接口文件、`.cpp` 实现）

### 高层结构

应用程序采用模块化 C++23 架构，后端和前端分离清晰：

**C++ 后端 (`src/`)**：
- **核心系统**：事件处理、HTTP 服务器、RPC 桥接、数据库管理和 WebView2 集成
- **功能**：图库管理、覆盖渲染、预览窗口、截图捕获、设置和窗口控制
- **UI 组件**：原生 Win32 窗口（应用程序窗口、上下文菜单、托盘图标）、WebView 包装器
- **工具**：文件操作、图形捕获、日志记录、系统集成

**React 前端 (`web/src/`)**：
- **布局组件**：活动栏、内容区域、支持主题的响应式布局
- **功能模块**：图库（带自适应视图）、设置管理、关于页面
- **共享库**：RPC 通信、资源管理、i18n 系统、Zustand 存储

### 关键架构模式

- **状态管理**：集中式 `AppState` 管理所有应用程序状态
- **事件驱动**：连接 UI 交互与后端操作的事件系统
- **RPC 通信**：C++ 后端和 React 前端之间基于 HTTP 的 RPC 桥接
- **插件系统**：可扩展的插件架构，用于游戏特定集成

### 技术栈

- **后端**：C++23、xmake、WebView2、Direct3D 11、Windows Graphics Capture API
- **前端**：React 19、TypeScript、Vite、Tailwind CSS、Radix UI 组件
- **通信**：基于 HTTP 的自定义 RPC、用于实时更新的服务器发送事件
- **构建工具**：Visual Studio 2022、xmake

### 关键依赖

**C++ 依赖**（通过 xmake 管理）：
- `spdlog`、`asio`、`reflectcpp`、`webview2`、`uwebsockets`、`SQLiteCpp`、`WebP`

**Web 依赖**：
- 核心：React 19、TypeScript、Vite 构建系统
- UI：Radix UI 基础组件、Tailwind CSS、Lucide 图标
- 状态：用于客户端状态管理的 Zustand

### 开发环境要求

- Visual Studio 2022+ 支持 C++23 模块
- Windows SDK 10.0.22621.0+（Windows 11 SDK）
- 用于前端开发的 Node.js
# NikkiLens

NikkiLens 是一个为《无限暖暖》游戏开发的实时画面旋转工具，旨在帮助玩家更方便地进行竖构图拍摄。

## 功能特性

- 实时捕获游戏窗口画面
- 90度画面旋转
- 可拖拽的悬浮窗口
- 置顶显示
- 性能优化，低资源占用
- 快捷键支持

## 技术方案

### 核心技术栈
- 开发语言：C++
- GUI框架：Dear ImGui
- 图形API：DirectX/GDI+
- 构建工具：Visual Studio 2022
- 依赖管理：vcpkg

### 主要模块
1. **窗口捕获模块**
   - Windows GDI/GDI+ 捕获
   - DirectX Desktop Duplication API

2. **图像处理模块**
   - 实时画面旋转
   - 图像缩放
   - SIMD 优化

3. **GUI渲染模块**
   - Dear ImGui 界面
   - 半透明悬浮窗
   - 拖拽支持

4. **配置管理模块**
   - 窗口位置记忆
   - 快捷键配置
   - 性能设置

## 开发环境配置

### 必需软件
1. **Visual Studio 2022**
   - Community 版本即可
   - [下载链接](https://visualstudio.microsoft.com/vs/community/)

### Visual Studio 组件
必须安装以下工作负载和组件：

1. **工作负载**
   - 使��� C++ 的桌面开发
   - 通用 Windows 平台开发

2. **组件详情**
   - Windows 10/11 SDK（最新版本）
   - 适用于最新 v143 生成工具的 C++ ATL
   - 适用于最新 v143 生成工具的 C++ MFC
   - C++ 核心功能
   - MSVC v143 - VS 2022 C++ x64/x86 生成工具

### 项目结构
```
NikkiLens/
├── src/              # 源代码
├── include/          # 头文件
├── resources/        # 资源文件
├── libs/             # 第三方库
└── build/            # 构建输出
```

## 性能优化策略

1. **CPU优化**
   - SIMD 指令集优化图像处理
   - 多线程处理
   - 帧率限制

2. **内存优化**
   - 内存池管理
   - 图像缓存机制
   - 资源复用

3. **渲染优化**
   - Direct2D 硬件加速
   - 局部更新策略
   - 垂直同步控制

## 开发计划

1. **Phase 1: 基础框架**
   - 项目初始化
   - 基本窗口捕获
   - GUI框架搭建

2. **Phase 2: 核心功能**
   - 实时画面旋转
   - 窗口管理
   - 基础交互

3. **Phase 3: 功能完善**
   - 配置系统
   - 快捷键支持
   - 性能优化

4. **Phase 4: 优化与测试**
   - 性能测试
   - 稳定性优化
   - 用户反馈改进

## 贡献指南

1. Fork 本仓库
2. 创建特性分支
3. 提交更改
4. 发起 Pull Request

## 许可证

[MIT License](LICENSE) 
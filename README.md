# 旋转吧大喵

旋转吧大喵（NikkiLens） 是一个为 《无限暖暖》 游戏开发的实时画面旋转工具，旨在帮助玩家更方便地进行竖构图拍摄。

## 功能特点

- 使用 Windows Graphics Capture API 实现高性能窗口捕获
- 支持捕获 DirectX 游戏画面
- 可调节窗口大小和缩放比例
- 支持水平和垂直镜像
- 全局热键快速切换显示/隐藏
- 系统托盘支持
- 可配置的帧率显示
- 所有设置自动保存

## 使用说明

1. 基本操作：
   - 启动程序后，会自动查找并捕获游戏窗口
   - 使用鼠标滚轮调整窗口大小
   - 拖动窗口任意位置可移动窗口
   - 按 ESC 键退出程序

2. 右键菜单功能：
   - 水平翻转：切换水平镜像
   - 垂直翻转：切换垂直镜像
   - 显示帧率：切换 FPS 显示
   - 退出程序

3. 全局热键：
   - 默认使用 Ctrl + Shift + P 快速显示/隐藏窗口
   - 可通过配置文件自定义组合键

4. 系统托盘：
   - 左键点击：切换窗口显示/隐藏
   - 右键菜单：显示/隐藏窗口、退出程序

5. 配置文件：
   程序会在同目录下自动创建 `config.json` 配置文件，可以手动修改以下设置：
   ```json
   {
     "hotkey": {
       "ctrl": true,    // 是否使用 Ctrl 键
       "shift": true,   // 是否使用 Shift 键
       "alt": false,    // 是否使用 Alt 键
       "key": 80        // 热键代码（80 为 P 键）
     },
     "window": {
       "mirrorX": false,     // 水平镜像
       "mirrorY": false,     // 垂直镜像
       "scaleRatio": 0.4,    // 缩放比例
       "showFPS": false      // 显示帧率
     }
   }
   ```

## 系统要求

- Windows 10 1809 或更高版本
- 支持 DirectX 11 的显卡
- 管理员权限（用于捕获游戏窗口）

## 构建指南

### 环境要求

- Windows 10 1809 或更高版本
- Visual Studio 2022（需要安装"使用 C++ 的桌面开发"工作负载）
- CMake 3.20 或更高版本

### 准备工作

1. **安装 Visual Studio 2022**
   - 下载并安装 [Visual Studio 2022](https://visualstudio.microsoft.com/vs/)
   - 在安装程序中选择"使用 C++ 的桌面开发"工作负载
   - 确保安装了 CMake 工具

2. **获取代码和依赖**（选择以下任一方式）

   方式一：在项目目录下安装vcpkg（推荐）
   ```batch
   git clone https://github.com/yourusername/NikkiLens.git
   cd NikkiLens
   git clone https://github.com/Microsoft/vcpkg.git
   cd vcpkg
   bootstrap-vcpkg.bat
   cd ..
   vcpkg\vcpkg install imgui:x64-windows
   vcpkg\vcpkg install directx-headers:x64-windows
   vcpkg\vcpkg install directxtk:x64-windows
   vcpkg\vcpkg install nlohmann-json:x64-windows
   ```

   方式二：使用全局vcpkg
   ```batch
   # 1. 安装vcpkg（如果尚未安装）
   git clone https://github.com/Microsoft/vcpkg.git D:\vcpkg
   cd D:\vcpkg
   bootstrap-vcpkg.bat
   
   # 2. 安装依赖
   vcpkg install imgui:x64-windows
   vcpkg install directx-headers:x64-windows
   vcpkg install directxtk:x64-windows
   vcpkg install nlohmann-json:x64-windows
   
   # 3. 克隆项目
   git clone https://github.com/yourusername/NikkiLens.git
   ```

### 使用 Visual Studio 2022 构建

1. 使用 Visual Studio 2022 打开项目：
   - 启动 Visual Studio 2022
   - 选择"打开本地文件夹"
   - 选择克隆的 NikkiLens 文件夹

2. 配置 CMake（仅当使用方式二时需要）：
   - 在 Visual Studio 中，右键点击 CMakeLists.txt
   - 选择"CMake 设置"
   - 在 CMake 命令参数中添加：
     ```
     -DCMAKE_TOOLCHAIN_FILE=D:/vcpkg/scripts/buildsystems/vcpkg.cmake
     ```

3. 生成解决方案：
   - 选择 x64-Release 配置
   - 点击"生成">"全部生成"

编译完成后，可执行文件将位于 `out/build/x64-Release/bin` 目录下。

## 注意事项

- 首次运行时会自动创建默认配置文件
- 修改配置文件后需要重启程序才能生效
- 如果无法捕获游戏窗口，请尝试以管理员身份运行

## 许可证

[待定]

## 致谢

- 99.9% 的代码由 Cursor AI 编写（不愧是你）
- 剩下的 0.1% 代码也是在 AI 的指导下完成的
- 项目所有者只是个调试工程师和项目经理 😂
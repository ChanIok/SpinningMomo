# NikkiLens

一个用于捕获和显示游戏窗口的工具，专为《无限暖暖》设计。使用 Windows Graphics Capture API 实现高性能窗口捕获。

## 功能特点

- 使用 Windows Graphics Capture API 实现高性能窗口捕获
- 支持捕获 DirectX 游戏画面
- 可调节窗口大小和缩放比例
- 支持水平和垂直镜像
- 全局热键快速切换显示/隐藏
- 系统托盘支持
- 可配置的帧率和缓冲设置

## 构建指南

### 环境要求

- Windows 10 1809 或更高版本
- Visual Studio 2022（需要安装"使用 C++ 的桌面开发"工作负载）

### 准备工作

1. **安装 Visual Studio 2022**
   - 下载并安装 [Visual Studio 2022](https://visualstudio.microsoft.com/vs/)
   - 在安装程序中选择"使用 C++ 的桌面开发"工作负载
   - 确保安装了 CMake 工具（Visual Studio 安装程序中默认包含）

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
   - 选择 x64-Debug 或 x64-Release 配置
   - 点击"生成">"全部生成"

编译完成后，可执行文件将位于 `out/build/x64-[配置类型]/bin` 目录下。

## 使用说明

1. 运行程序后，会自动查找并捕获游戏窗口
2. 使用鼠标滚轮调整窗口大小
3. 右键菜单可以：
   - 切换水平/垂直镜像
   - 退出程序
4. 使用全局热键 Ctrl + Shift + P 快速显示/隐藏窗口
5. 可以通过系统托盘图标管理程序

## 注意事项

- 程序需要管理员权限才能捕获部分游戏窗口
- 确保游戏窗口标题与配置文件中的设置匹配
- 如果遇到性能问题，可以尝试调整帧率和缓冲设置

## 许可证

[待定]

## 致谢

- 99.9% 的代码由 Cursor AI 编写（不愧是你）
- 剩下的 0.1% 代码也是在 AI 的指导下完成的
- 项目所有者只是个调试工程师和项目经理 😂
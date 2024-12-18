<div align="center">
  <h1>
    <img src="./docs/logo.png" width="200" alt="SpinningMomo Logo">
    <br/>
    🎮 旋转吧大喵
    <br/>
    <sup>优雅的 《无限暖暖》 窗口比例和尺寸调整工具</sup>
  </h1>

  <p>
    <img alt="Platform" src="https://img.shields.io/badge/platform-Windows-blue?style=flat-square" />
    <img alt="Release" src="https://img.shields.io/github/v/release/ChanIok/SpinningMomo?style=flat-square&color=brightgreen" />
    <img alt="License" src="https://img.shields.io/badge/license-MIT-orange?style=flat-square" />
  </p>

  <p>
    <b>
      <a href="#✨-特色功能">特色功能</a> •
      <a href="#📖-使用指南">使用指南</a> •
      <a href="#🛠️-构建指南">构建指南</a> 
    </b>
  </p>

  <img src="./docs/README.jpg" alt="Screenshot" >
</div>

## 🎯 项目简介

旋转吧大喵（SpinningMomo）最初是为了解决《无限暖暖》游戏拍摄竖构图的问题而开发的工具。通过实时调整游戏窗口大小来适配不同的拍摄比例，确保您能获得完美的构图效果。

## ✨ 特色功能

<div align="center">
  <table>
    <tr>
      <td align="center">🎮 <b>完美竖拍支持</b><br/>支持游戏竖拍UI和相册功能</td>
      <td align="center">📸 <b>超高分辨率</b><br/>支持突破游戏原生分辨率的照片输出</td>
    </tr>
    <tr>
      <td align="center">📐 <b>灵活调整</b><br/>多种预设和自定义比例和尺寸</td>
      <td align="center">⌨️ <b>快捷键支持</b><br/>可自定义热键(默认Ctrl+Alt+R)</td>
    </tr>
    <tr>
      <td align="center">🚀 <b>轻量运行</b><br/>占用极低，性能优先</td>
      <td align="center">⚙️ <b>多种拍摄模式</b><br/>支持窗口和全屏窗口模式</td>
    </tr>
  </table>
</div>

## 📖 使用指南

### 下载安装

- **GitHub Release**：[点击下载最新版本](https://github.com/ChanIok/SpinningMomo/releases/latest)
- **蓝奏云**：[点击下载](https://wwf.lanzoul.com/b0sxagp0d)（密码：momo）

### 拍照模式选择

1. **窗口模式（推荐日常拍摄）**
   - 游戏设置：
     - 显示模式：窗口模式
     - 拍照画质：4K
   - 使用方法：
     - 使用程序的比例选项调整构图
     - 可输出任意比例的4K质量照片
     - 适合日常拍摄和构图调整

2. **全屏窗口模式（适合超高分辨率输出）**
   - 游戏设置：
     - 显示模式：全屏窗口模式
     - 拍照画质：窗口分辨率
   - 使用方法：
     - 先在游戏中调整好构图和参数
     - 使用程序的固定尺寸选项（如8K）
     - 窗口会溢出屏幕，此时按空格连拍
     - 拍摄完成后点击重置窗口
     - 可获得8K甚至12K以上超高分辨率照片

### 快捷操作

1. 以**管理员身份**运行程序
2. 按下热键（默认Ctrl+Alt+R）打开调整菜单
3. 选择需要的比例或固定尺寸
4. 拍摄完成后可使用重置选项恢复窗口

### 托盘功能

右键或左键点击托盘图标可以：

- 🎯 选择目标窗口：从子菜单中选择要调整的窗口
- 📐 窗口比例：选择预设比例或添加自定义比例
- 📏 固定尺寸：选择预设的高分辨率尺寸
- ⌨️ 修改热键：设置新的快捷键组合（按下新组合后可能有 1-2 秒延迟才会显示设置成功提示）
- 🔔 显示提示：开启/关闭操作提示消息
- 📌 保持窗口置顶：窗口始终显示在最前
- ⚙️ 打开配置文件：自定义比例和尺寸
- ❌ 退出：关闭程序

### 自定义设置

1. 右键托盘图标，选择"打开配置文件"
2. 在配置文件中添加：
   - 自定义比例：[CustomRatio] 节的 RatioList 后添加 "16:10,17:10"
     - 用冒号(:)连接宽高比，逗号(,)分隔多个比例
   - 自定义尺寸：[CustomSize] 节的 SizeList 后添加 "1920x1080,2560x1440" 
     - 用字母x连接宽高像素，逗号(,)分隔多个尺寸
3. 保存并重启软件后生效

### 预设选项

- **比例**：32:9, 21:9, 16:9, 3:2, 1:1, 2:3, 9:16
- **尺寸**：
  - 7680×4320 (8K 16:9)
  - 4320×7680 (8K 9:16)
  - 8192×5464 (8K+ 3:2)
  - 5464×8192 (8K+ 2:3)

### 注意事项

- [Visual C++ Redistributable 2015-2022](https://aka.ms/vs/17/release/vc_redist.x64.exe)
  - 如果运行时提示缺少 DLL，请安装此运行时库

## 🛠️ 构建指南

### 环境要求

- Visual Studio 2019+
- Windows SDK 10.0.17763.0+
- CMake 3.15+

### 构建步骤

1. 克隆仓库到本地
2. 使用 Visual Studio 打开 CMakeLists.txt
3. 选择 Release/x64 配置
4. 生成解决方案

## ❤️ 致谢

- 99.9% 的代码由 Cursor AI 编写（不愧是你）
- 剩下的 0.1% 代码也是在 AI 的指导下完成的
- 项目所有者只是个调试工程师和项目经理 😂
- 感谢 Cursor AI 的强大能力，让这个项目能够不断进化和改进
  - 上面这句话是 AI 写的，我可没说过

## 📄 开源协议

本项目采用 [MIT 协议](LICENSE) 开源。

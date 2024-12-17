<div align="center">
  <h1>
    <img src="./docs/logo.png" width="200" alt="SpinningMomo Logo">
    <br/>
    🎮 旋转吧大喵
    <br/>
    <sup>优雅的 《无限暖暖》 窗口比例调整工具</sup>
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
      <td align="center">📸 <b>原生输出</b><br/>不影响游戏照片输出质量</td>
    </tr>
    <tr>
      <td align="center">⚡ <b>双模式切换</b><br/>快捷选择/常规模式随心切换</td>
      <td align="center">📐 <b>灵活比例调整</b><br/>多种预设和自定义比例</td>
    </tr>
    <tr>
      <td align="center">🚀 <b>轻量运行</b><br/>占用极低，性能优先</td>
      <td align="center">⌨️ <b>快捷键支持</b><br/>可自定义热键(默认Ctrl+Alt+R)</td>
    </tr>
  </table>
</div>

### 操作模式

- **🔄 快捷选择模式** (默认)
  - 按下热键即可快速切换不同比例
  - 适合需要频繁切换构图的场景

- **📌 常规模式**
  - 一键切换固定比例
  - 适合固定使用某个比例的场景

## 📖 使用指南

### 下载安装

- **GitHub Release**：[点击下载最新版本](https://github.com/ChanIok/SpinningMomo/releases/latest)
- **蓝奏云**：[点击下载](https://wwf.lanzoul.com/b0sxagp0d)（密码：momo）


### 快速开始

1. 以**管理员身份**运行程序，它会以托盘图标的形式在后台运行
2. 游戏窗口模式选择：
   - **窗口模式**（推荐）
     - 优点：支持所有比例的照片输出
     - 缺点：调整尺寸后可能需要在游戏内重新应用分辨率以修正边框
   - **全屏窗口模式**
     - 优点：游戏体验更佳，支持垂直构图
     - 缺点：照片输出仅限显示器原生比例？，更适合搭配常规模式
     
### 托盘功能

右键点击托盘图标可以：

- 🎯 选择目标窗口：从子菜单中选择要旋转的窗口
- 📐 设置窗口比例：选择预设比例或添加自定义比例
- ⚡ 快捷选择模式：开启后热键直接弹出比例选择菜单
- ⌨️ 修改热键：设置新的快捷键组合（按下新组合后可能有 1-2 秒延迟才会显示设置成功提示）
- 🔔 显示提示：开启/关闭旋转成功的提示消息
- 📌 保持窗口置顶：窗口始终显示在最前
- 🔄 自动隐藏任务栏：开启/关闭竖屏时自动隐藏任务栏
- ❌ 退出：关闭程序

### 自定义比例

1. 右键托盘图标，选择"添加自定义比例..."
2. 在配置文件的 [CustomRatio] 节添加比例
   - 格式：用英文逗号分隔，用英文冒号连接宽高
   - 示例：16:10,17:10,18:10
3. 保存并重启软件即可使用新比例

### 预设比例

- **横构图**：32:9, 21:9, 16:9, 16:10, 5:4, 4:3, 3:2, 1:1
- **竖构图**：2:3, 3:4, 4:5, 9:16(默认)

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

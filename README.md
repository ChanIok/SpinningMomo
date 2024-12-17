<div align="center">
  <h1 align="center">
   <img src="./docs/logo.png" width="200">
    <br/>
    🎮 旋转吧大喵
    <br/>
    <sup>优雅的 Windows 窗口比例调整工具</sup>
  </h1>

  <p>
    <img alt="Platform" src="https://img.shields.io/badge/platform-Windows-blue?style=flat-square" />
    <img alt="Release" src="https://img.shields.io/github/v/release/ChanIok/SpinningMomo?style=flat-square&color=brightgreen" />
    <img alt="License" src="https://img.shields.io/badge/license-MIT-orange?style=flat-square" />
  </p>

  <p>
    <a href="#项目说明">项目说明</a> •
    <a href="#功能特点">功能特点</a> •
    <a href="#使用方法">使用方法</a>
  </p>
</div>

## 项目说明

旋转吧大喵（SpinningMomo）最初是为了解决《无限暖暖》游戏拍摄竖构图的问题而开发的工具。通过实时调整游戏窗口大小来适配不同的拍摄比例，确保您能获得完美的构图效果。

  <h1 align="center">
   <img src="./docs/README.jpg"  >
  </h1>

## 功能特点

<div align="center">
  <table>
    <tr>
      <td>✨ 多种预设和自定义比例</td>
      <td>⌨️ 支持自定义热键（默认 Ctrl+Alt+R）</td>
    </tr>
    <tr>
      <td>🎯 支持选择任意窗口调整</td>
      <td>💾 自动保存窗口和设置</td>
    </tr>
    <tr>
      <td>🔄 支持自动隐藏任务栏（可选）</td>
      <td>📝 支持成功提示开关（可选）</td>
    </tr>
  </table>
</div>

## 使用方法

### 基本设置

1. 以**管理员身份**运行程序，它会以托盘图标的形式在后台运行
2. 游戏设置建议：
   - 使用"窗口模式"运行游戏
   - 将游戏分辨率设置为显示器的原生分辨率

### 托盘菜单功能

右键点击托盘图标，可以：

- 选择窗口：从子菜单中选择要旋转的窗口
- 窗口比例：选择预设比例或添加自定义比例
- 快捷选择模式：开启后热键直接弹出比例选择菜单
- 修改热键：设置新的快捷键组合（按下新组合后可能有 1-2 秒延迟才会显示设置成功提示）
- 显示提示：开启/关闭旋转成功的提示消息
- 自动隐藏任务栏：开启/关闭竖屏时自动隐藏任务栏
- 退出：关闭程序

### 自定义比例

1. 在托盘图标右键菜单中选择"添加自定义比例..."
2. 在打开的配置文件中找到 [CustomRatio] 节
3. 在 RatioList 后添加您需要的比例，格式如：16:10,17:10,18:10
4. 保存文件并重启软件即可使用自定义比例

### 预设比例

工具内置了多种常用比例：

- 横构图：32:9, 21:9, 16:9, 16:10, 5:4, 4:3, 3:2, 1:1
- 竖构图：2:3, 3:4, 4:5, 9:16（默认）

### 注意事项

- "全屏窗口模式"下的截图只能输出显示器的原生的比例，但支持垂直构图
- "窗口模式"下，重置回原来的尺寸可能会出现边框等误差，必要时请在游戏设置内重新应用分辨率
- 如果您在游戏设置中重新应用了分辨率，需要在托盘右键菜单中重新选择"无限暖暖"
- 程序会记录首次启动时窗口的原始尺寸，重置和修改是基于原始尺寸的

## 系统要求

- Windows 10 或更高版本
- [Visual C++ Redistributable 2015-2022](https://aka.ms/vs/17/release/vc_redist.x64.exe)
  - 如果运行时提示缺少 DLL，请安装此运行时库

## 构建指南

### 环境要求

- Visual Studio 2019 或更高版本
- Windows SDK 10.0.17763.0 或更高版本
- CMake 3.15 或更高版本

### 构建步骤

1. 克隆仓库到本地
2. 使用 Visual Studio 打开 CMakeLists.txt
3. 选择 Release/x64 配置
4. 生成解决方案

## 开源协议

本项目采用 MIT 协议开源，详见 [LICENSE](LICENSE) 文件。

## 致谢

- 99.9% 的代码由 Cursor AI 编写（不愧是你）
- 剩下的 0.1% 代码也是在 AI 的指导下完成的
- 项目所有者只是个调试工程师和项目经理 😂
- 感谢 Cursor AI 的强大能力，让这个项目能够不断进化和改进
  - 上面这句话是 AI 写的，我可没说过 😆

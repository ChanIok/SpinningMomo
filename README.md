<div align="center">
  <h1 align="center">
   <img src="./docs/logo.jpg" width="200">
    <br/>
    🎮 旋转吧大喵
    <br/>
    <sup>优雅的 Windows 窗口旋转工具</sup>
  </h1>

  <p>
    <img alt="Platform" src="https://img.shields.io/badge/platform-Windows-blue?style=flat-square" />
    <img alt="Release" src="https://img.shields.io/badge/release-v0.3.0-brightgreen?style=flat-square" />
    <img alt="License" src="https://img.shields.io/badge/license-MIT-orange?style=flat-square" />
  </p>

  <p>
    <a href="#项目说明">项目说明</a> •
    <a href="#功能特点">功能特点</a> •
    <a href="#使用方法">使用方法</a> •
    <a href="#更新历史">更新历史</a>
  </p>
</div>

## 项目说明

旋转吧大喵（SpinningMomo）最初是为了解决《无限暖暖》游戏拍摄竖构图的问题而开发的工具。

这个项目的进化史：

- v0.1.0：我用了 Windows Graphics Capture API 实现了一个高性能实时画面捕获和旋转方案。很酷，但实际操作体验没想象中那么好。
- v0.2.0：转念一想，为什么不直接旋转屏幕呢？简单粗暴，一点都不好，因为还是要歪着头看。
- v0.3.0：最终顿悟 —— 旋转窗口才是正道！这才是真正的优雅方案！✨

## 功能特点

<div align="center">
  <table>
    <tr>
      <td>✨ 保持窗口宽高比进行旋转</td>
      <td>⌨️ 支持自定义热键（默认 Ctrl+Alt+R）</td>
    </tr>
    <tr>
      <td>🎯 支持选择任意窗口进行旋转</td>
      <td>💾 支持保存上次选择的窗口</td>
    </tr>
    <tr>
      <td>🔄 支持自动隐藏任务栏（可选）</td>
      <td>📝 支持成功提示开关（可选）</td>
    </tr>
  </table>
</div>

## 使用方法

1. **以管理员身份**运行程序，它会以托盘图标的形式在后台运行
2. 右键点击托盘图标，可以：
   - 选择窗口：从子菜单中选择要旋转的窗口
   - 修改热键：设置新的快捷键组合（按下新组合后可能有 1-2 秒延迟才会显示设置成功提示）
   - 显示提示：开启/关闭旋转成功的提示消息
   - 自动隐藏任务栏：开启/关闭竖屏时自动隐藏任务栏
   - 退出：关闭程序
3. 按下热键（默认 Ctrl+Alt+R）即可旋转当前选择的窗口

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

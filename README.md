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
      <a href="#-特色功能">特色功能</a> •
      <a href="#-使用指南">使用指南</a> •
      <a href="#️-构建指南">构建指南</a> 
    </b>
  </p>

  <img src="./docs/README.jpg" alt="Screenshot" >
</div>

## 🎯 项目简介

旋转吧大喵（SpinningMomo）最初是为了解决《无限暖暖》游戏拍摄竖构图的问题而开发的工具。

通过快捷键让您能够轻松切换游戏窗口的比例和尺寸，完美满足各种构图需求。无论是竖构图拍摄、相册浏览还是留影沙漏，都能获得最佳的显示效果。

甚至能突破游戏和设备原有的限制，输出 8K 甚至 12K 以上超高分辨率照片！

## ✨ 特色功能

<div align="center">
  <table>
    <tr>
      <td align="center">🎮 <b>竖拍支持</b><br/>完美支持游戏竖拍UI，留影沙漏和大喵相册</td>
      <td align="center">📸 <b>超高分辨率</b><br/>支持突破游戏和设备分辨率的照片输出</td>
    </tr>
    <tr>
      <td align="center">📐 <b>灵活调整</b><br/>多种预设，自定义比例和尺寸</td>
      <td align="center">⌨️ <b>快捷键支持</b><br/>可自定义热键(默认Ctrl+Alt+R)</td>
    </tr>
    <tr>
      <td align="center">⚙️ <b>多种拍摄模式</b><br/>支持窗口和全屏窗口模式</td>
      <td align="center">🚀 <b>轻量运行</b><br/>占用极低，性能优先</td>
    </tr>
  </table>
</div>

## 📖 使用指南

### 下载安装

- **GitHub Release**：[点击下载最新版本](https://github.com/ChanIok/SpinningMomo/releases/latest)
- **蓝奏云**：[点击下载](https://wwf.lanzoul.com/b0sxagp0d)（密码：momo）

### 快速开始

1. 以**管理员身份**运行程序
2. 按下热键（默认 Ctrl+Alt+R）打开调整菜单
3. 选择需要的比例或固定尺寸
4. 拍摄完成后可使用重置选项恢复窗口

### 拍照模式选择

1. **窗口模式（推荐日常拍摄）**

   - 游戏设置：
     - 显示模式：窗口模式
     - 拍照画质：4K
   - 使用方法：
     - 使用程序的比例选项调整构图
     - 可输出任意比例的 4K 质量照片
     - 适合日常拍摄和构图调整

2. **全屏窗口模式（适合超高分辨率输出）**
   - 游戏设置：
     - 显示模式：全屏窗口模式
     - 拍照画质：窗口分辨率
   - 使用方法：
     - 先在游戏中调整好构图和参数
     - 使用程序的固定尺寸选项
     - 窗口会溢出屏幕，此时按空格拍照
     - 拍摄完成后点击重置窗口
     - 可获得 8K 甚至 12K 以上超高分辨率照片

### 托盘功能

右键或左键点击托盘图标可以：

- 🎯 选择目标窗口：从子菜单中选择要调整的窗口
- 📐 窗口比例：选择预设比例或自定义比例
- 📏 固定尺寸：选择预设的高分辨率尺寸或自定义尺寸
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
     - 用字母 x 连接宽高像素，逗号(,)分隔多个尺寸
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

## 📅 项目进化史 
<details>
<summary><h3>折腾的经历</h3></summary>

> _以下是 Cursor AI 生成的历史_

### v0.1.0 - 投屏大法好？

尝试用 Windows Graphics Capture API 实现实时画面捕获和旋转。  
效果很酷！但实际用起来... 嗯... UI 还是横着的啊喂！  
[查看代码](https://github.com/ChanIok/SpinningMomo/tree/v0.1.0)

### v0.2.0 - 灵光一闪

"既然 UI 不转，那我转屏幕总行了吧！"  
结果：确实可以，但是... 你愿意继续歪着脖子玩游戏吗？  
[查看代码](https://github.com/ChanIok/SpinningMomo/tree/v0.2.0)

### v0.3.0 - 顿悟时刻

终于开窍了 —— 旋转窗口才是正道！  
完美解决 UI 翻转问题，画质也不受影响，这才是真正的优雅方案！  
[查看代码](https://github.com/ChanIok/SpinningMomo/tree/v0.3.0)

### v0.4.0 - 意外之喜

某天摸鱼时的意外发现：  
全屏窗口模式 + 拍照画质选择窗口分辨率 = 8K 照片？！  
这个 bug 我喜欢，这个 bug 我留着！  
[查看代码](https://github.com/ChanIok/SpinningMomo/tree/v0.4.0)

</details>

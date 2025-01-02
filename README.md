<div align="center">
  <h1>
    <img src="./docs/logo.png" width="200" alt="SpinningMomo Logo">
    <br/>
    🎮 旋转吧大喵
    <br/><br/>
    <sup>一个为 《无限暖暖》 提升摄影体验的窗口调整工具</sup>
    <br/>

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
      <a href="#️-构建指南">构建指南</a> •
      <a href="./docs/README_EN.md">English</a>
    </b>
  </p>

  <img src="./docs/README.jpg" alt="Screenshot" >
</div>

## 🎯 项目简介

旋转吧大喵（SpinningMomo）最初是为了解决《无限暖暖》游戏拍摄竖构图的问题而开发的工具。

通过快捷键让您能够轻松切换游戏窗口的比例和尺寸，完美满足各种构图需求。无论是竖构图拍摄、相册浏览还是留影沙漏，都能获得最佳的显示效果。

甚至能突破游戏和设备原有的限制，输出 8K 甚至 12K 以上超高分辨率照片！

> 如果觉得这个工具对你有帮助，欢迎点个 Star ⭐ 支持一下~

## ✨ 特色功能

<div align="center">
  <table>
    <tr>
      <td align="center">🎮 <b>竖拍支持</b><br/>完美支持游戏竖拍UI，留影沙漏和大喵相册</td>
      <td align="center">📸 <b>超高分辨率</b><br/>支持突破游戏和设备分辨率的照片输出</td>
    </tr>
    <tr>
      <td align="center">📐 <b>灵活调整</b><br/>多种预设，自定义比例和分辨率</td>
      <td align="center">⌨️ <b>快捷键支持</b><br/>可自定义热键(默认Ctrl+Alt+R)</td>
    </tr>
    <tr>
      <td align="center">⚙️ <b>浮动窗口</b><br/>可选的浮动菜单，便捷调整窗口</td>
      <td align="center">🚀 <b>轻量运行</b><br/>占用极低，性能优先</td>
    </tr>
  </table>
</div>

## 📖 使用指南

### 下载安装

- **GitHub Release**：[点击下载最新版本](https://github.com/ChanIok/SpinningMomo/releases/latest)
- **蓝奏云**：[点击下载](https://wwf.lanzoul.com/b0sxagp0d)（密码：momo）
- **百度网盘**：[点击下载](https://pan.baidu.com/s/1UL9EJa2ogSZ4DcnGa2XcRQ?pwd=momo)（提取码：momo）

### 快速开始

1. 以**管理员身份**运行程序
2. 程序启动后会显示浮动窗口，在窗口中可以直接调整比例和分辨率
3. 使用热键（默认 Ctrl+Alt+R）可以显示/隐藏浮动窗口
4. 拍摄完成后可使用重置选项恢复窗口

### 拍照模式选择

<div align="center">
  <table>
    <tr>
      <th align="center">📸 窗口分辨率模式（推荐）</th>
      <th align="center">🎯 标准模式</th>
    </tr>
    <tr>
      <td>
        <b>游戏设置</b><br/>
        ▫️ 显示模式：<b>全屏窗口模式（推荐）</b> / 窗口模式<br/>
        ▫️ 拍照画质：<b>窗口分辨率</b>
      </td>
      <td>
        <b>游戏设置</b><br/>
        ▫️ 显示模式：<b>窗口模式</b> / 全屏窗口模式（比例受限）<br/>
        ▫️ 拍照画质：<b>4K</b>
      </td>
    </tr>
    <tr>
      <td>
        <b>使用步骤</b><br/>
        1️⃣ 使用程序的比例选项调整构图<br/>
        2️⃣ 选择需要的分辨率预设（4K~12K）<br/>
        3️⃣ 画面会溢出屏幕，此时按空格拍照<br/>
        4️⃣ 拍摄完成后点击重置窗口
      </td>
      <td>
        <b>使用说明</b><br/>
        ✅ 操作便捷，适合日常拍摄和预览<br/>
        ❗ 只能调整比例，无法调整分辨率<br/>
        ❗ 输出为伪4K，画质不如窗口分辨率<br/>
        ❗ 全屏窗口模式下输出受限于显示器原始比例
      </td>
    </tr>
    <tr>
      <td>
        <b>优势特点</b><br/>
        ⭐ 支持超高分辨率（最高12K+）<br/>
        ⭐ 可自由调整比例和分辨率
      </td>
      <td>
        <b>适用场景</b><br/>
        ⭐ 需要快速取景时<br/>
        ⭐ 不追求极致画质时<br/>
      </td>
    </tr>
  </table>
</div>

### 分辨率说明
- 输出分辨率计算过程：
  1. 根据选择的分辨率预设（如 4K、8K）确定总像素数
  2. 根据选择的比例计算最终的宽高
     - 例如：选择 8K（约 3320 万像素）和 9:16 比例
     - 计算得到 4320x7680 的输出分辨率（4320x7680=3320万像素）
     - 保证总像素数与预设值相近

### 托盘功能

右键或左键点击托盘图标可以：

- 🎯 选择目标窗口：从子菜单中选择要调整的窗口
- 📐 窗口比例：选择预设比例或自定义比例
- 📏 分辨率：选择预设分辨率或自定义分辨率
- 📸 截图：保存无损截图到程序目录下的ScreenShot文件夹中，主要用于调试和不支持截图的游戏（正常不需要使用）
- 📂 打开相册：打开游戏相册目录，用于查看和打开游戏截图
- 🔽 隐藏任务栏：隐藏任务栏，用于防止任务栏遮挡
- ⬇️ 调整时置底任务栏：调整窗口时将任务栏置底，用于防止任务栏遮挡
- ⌨️ 修改热键：设置新的快捷键组合（按下新组合后可能有 5 秒延迟才会显示设置成功提示）
- 🔍 预览窗口：类似 Photoshop 的导航器，在窗口溢出屏幕时提供实时预览和导航功能（可选功能）
  - 支持拖拽窗口顶部区域移动位置，滚轮缩放窗口大小
- 📱 浮窗模式：默认开启，可以在此开启/关闭浮动菜单。关闭后可使用热键（默认 Ctrl+Alt+R）打开调整菜单
- 🌐 语言：切换语言
- ⚙️ 打开配置文件：自定义比例和分辨率
- ❌ 退出：关闭程序

### 自定义设置

1. 右键托盘图标，选择"打开配置文件"
2. 在配置文件中添加：
   - 自定义比例：[CustomRatio] 节的 RatioList 后添加 "16:10,17:10"
     - 用冒号(:)连接宽高比，逗号(,)分隔多个比例
   - 自定义分辨率：[CustomResolution] 节的 ResolutionList 后添加 "6000x4000,7680x4320"
     - 用字母x连接宽高，逗号(,)分隔多个分辨率
3. 保存并重启软件后生效

### 预设选项

- **比例**：32:9, 21:9, 16:9, 3:2, 1:1, 2:3, 9:16
- **分辨率**：
  - 4K (3840×2160, 约 830 万像素)
  - 6K (5760×3240, 约 1870 万像素)
  - 8K (7680×4320, 约 3320 万像素)
  - 12K (11520×6480, 约 7460 万像素)

### 注意事项

- 系统要求：Windows 10 及以上版本
- [Visual C++ Redistributable 2015-2022](https://aka.ms/vs/17/release/vc_redist.x64.exe)
  - 如果运行时提示缺少 DLL，请安装此运行时库
- 由于 Windows 限制，当使用"窗口模式"且需要超出屏幕范围时，程序会自动切换为无边框样式，可通过游戏设置来恢复
- 较高的分辨率可能会影响游戏性能，请根据设备性能适当调整
- 建议在拍摄前先进行画质对比测试，选择最适合的设置

### 安全性说明

本程序仅通过 Windows 系统标准的窗口管理 API 发送请求，所有调整都由 Windows 系统自行执行，原理等同于：
- 系统分辨率变更时的窗口自适应
- 屏幕旋转时的窗口重排
- 多显示器下的窗口移动

## 🛠️ 构建指南

### 环境要求

- Visual Studio 2022+
- Windows SDK 10.0.22621.0+（Windows 11 SDK）
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

本项目采用 [MIT 协议](LICENSE) 开源。项目图标来自游戏《无限暖暖》，版权归游戏开发商所有。

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

### v0.5.0 - 原生の浮窗

"用原生 Win32 API 写 UI？疯了吧！"  
是的，我们就是用最朴素的 Windows API 手写了每一个像素...  
没有现代框架，没有拖拽设计器，纯手工打造的浮窗界面！  
[查看代码](https://github.com/ChanIok/SpinningMomo/tree/v0.5.0)

### v0.6.0 - 窗外有喵

"8K 截图是很爽，但是... 窗口跑到屏幕外面去了诶！"  
于是我们又双叒叕用 DirectX 11 手搓了一个预览窗口...  
拖拽、缩放、实时预览，完美复刻了 PS 的导航器！  
[查看代码](https://github.com/ChanIok/SpinningMomo/tree/v0.6.0)

</details>

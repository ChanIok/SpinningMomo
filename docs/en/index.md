---
layout: doc
---

<div class="center">
  <div class="logo-container">
    <img src="../public/logo.png" width="200" alt="SpinningMomo Logo" />
  </div>

  <h1 class="title">🎮 SpinningMomo</h1>

  A window adjustment tool to enhance photography experience in Infinity Nikki

  <div class="badges">
    <img alt="Platform" src="https://img.shields.io/badge/platform-Windows-blue?style=flat-square" />
    <img alt="Release" src="https://img.shields.io/github/v/release/ChanIok/SpinningMomo?style=flat-square&color=brightgreen" />
    <img alt="License" src="https://img.shields.io/badge/license-MIT-orange?style=flat-square" />
  </div>

  <div class="nav-links">
    <a href="#features">✨ Features</a> •
    <a href="#user-guide">🚀 User Guide</a>
  </div>

  <div class="screenshot-container">
    <img src="../public/README.jpg" alt="Screenshot" />
  </div>
</div>

## 🎯 Introduction

SpinningMomo was initially developed to solve the vertical composition photography issue in Infinity Nikki.

With hotkeys, you can easily switch the game window's aspect ratio and size, perfectly meeting various composition needs. Whether it's vertical photography, album browsing, or snapshot hourglass, you can achieve the best display effect.

It can even break through the original resolution limitations of the game and device, outputting ultra-high resolution photos up to 8K or even 12K!

> If you find this tool helpful, please consider giving it a Star ⭐!

## Features

<div class="feature-grid">
  <div class="feature-item">
    <h3>🎮 Portrait Mode</h3>
    <p>Perfect support for vertical UI, snapshot hourglass, and album</p>
  </div>
  <div class="feature-item">
    <h3>📸 Ultra-High Resolution</h3>
    <p>Support photo output beyond game and device resolution limits</p>
  </div>
  <div class="feature-item">
    <h3>📐 Flexible Adjustment</h3>
    <p>Multiple presets, custom ratios and resolutions</p>
  </div>
  <div class="feature-item">
    <h3>⌨️ Hotkey Support</h3>
    <p>Customizable hotkey (default: Ctrl+Alt+R)</p>
  </div>
  <div class="feature-item">
    <h3>⚙️ Floating Window</h3>
    <p>Optional floating menu for convenient window adjustment</p>
  </div>
  <div class="feature-item">
    <h3>🚀 Lightweight</h3>
    <p>Minimal resource usage, performance priority</p>
  </div>
</div>

## User Guide

### 1️⃣ Getting Started

::: danger Important
The program must be run as **Administrator**!
:::

Two ways to grant administrator privileges:
- **Temporary**: Right-click the program → Select "Run as administrator"
- **Permanent**: Right-click the program → Properties → Compatibility → Check "Run this program as an administrator" → Apply

After startup:
- Program icon <img src="../public/logo.png" style="display: inline; height: 1em; vertical-align: text-bottom;" /> will appear in system tray
- Floating window is shown by default for direct window adjustment

### 2️⃣ Hotkeys

| Function | Hotkey | Description |
|:--|:--|:--|
| Show/Hide Floating Window | `Ctrl + Alt + R` | Default hotkey, can be modified in tray menu |

### 3️⃣ Photography Modes

#### 🌟 Window Resolution Mode (Recommended)

Game Settings:
- Display Mode: **Fullscreen Window** (Recommended) or Window Mode
- Photo Quality: **Window Resolution**

Steps:
1. Use ratio options to adjust composition
2. Select desired resolution preset (4K~12K)
3. Screen will exceed display bounds, press space to capture
4. Click reset window after shooting

Advantages:
- ✨ Support ultra-high resolution (up to 12K+)
- ✨ Freely adjustable ratio and resolution

#### 📷 Standard Mode

Game Settings:
- Display Mode: **Window Mode** or Fullscreen Window (ratio limited)
- Photo Quality: **4K**

Notes:
- ✅ Convenient operation, suitable for daily shooting and preview
- ❗ Can only adjust ratio, cannot adjust resolution
- ❗ Outputs pseudo-4K, quality lower than window resolution
- ❗ In fullscreen window mode, output limited by monitor's native ratio

### Resolution Explanation
- Resolution calculation process:
  1. First determine total pixel count based on selected resolution preset (e.g., 4K, 8K)
  2. Calculate final width and height based on selected ratio
     - Example: When selecting 8K (about 33.2M pixels) and 9:16 ratio
     - Results in 4320x7680 output resolution (4320x7680=33.2M pixels)
     - Ensures total pixel count matches preset value

### Tray Features

Right-click or left-click the tray icon to:

- 🎯 **Select Window**: Choose the target window from the submenu
- 📐 **Window Ratio**: Select from preset ratios or custom ratios 
- 📏 **Resolution**: Select from preset resolutions or custom resolutions
- 📍 **Screenshot**: Save lossless screenshots to the ScreenShot folder in program directory
- 📂 **Open Screenshot Folder**: Open the game screenshot directory
- 🔽 **Hide Taskbar**: Hide the taskbar to prevent overlap
- ⬇️ **Lower Taskbar When Resizing**: Lower taskbar when resizing window
- ⌨️ **Modify Hotkey**: Set a new shortcut combination
- 🔍 **Preview Window**: Similar to Photoshop's navigator
  - Support dragging window top area to move position
  - Mouse wheel to zoom window size
- 📱 **Floating Mode**: Toggle floating menu visibility
- 🌐 **Language**: Switch language
- ⚙️ **Open Config**: Customize ratios and resolutions
- ❌ **Exit**: Close the program

### Custom Settings

1. Right-click tray icon, select "Open Config File"
2. Add in config file:
   - Custom ratios: Add `16:10,17:10` after RatioList in [CustomRatio] section
     - Use colon (`:`) for width:height, comma (`,`) to separate multiple ratios
   - Custom resolutions: Add `6000x4000,7680x4320` after ResolutionList in [CustomResolution] section
     - Use letter `x` to connect width and height, comma (`,`) to separate multiple resolutions
3. Save and restart software to apply changes

### Preset Options

- **Ratios**: 32:9, 21:9, 16:9, 3:2, 1:1, 2:3, 9:16
- **Resolutions**:
  - 4K (3840×2160, about 8.3M pixels)
  - 6K (5760×3240, about 18.7M pixels)
  - 8K (7680×4320, about 33.2M pixels)
  - 12K (11520×6480, about 74.6M pixels)

### Notes

- System Requirements: Windows 10 or above
- [Visual C++ Redistributable 2015-2022](https://aka.ms/vs/17/release/vc_redist.x64.exe)
  - Install this runtime if you encounter missing DLL errors
- Due to Windows limitations, when using "Window Mode" and the window size needs to exceed the screen range, the program will automatically switch to borderless style, which can be restored through game settings
- Higher resolutions may affect game performance, please adjust according to your device capabilities
- It's recommended to test quality comparison before shooting to choose the most suitable settings

### Security Statement

This program only sends requests through Windows standard window management APIs, with all adjustments executed by the Windows system itself, working similarly to:
- Window auto-adjustment when changing system resolution
- Window rearrangement when rotating screen
- Window movement in multi-display setups

## License

This project is licensed under the [MIT License](https://github.com/ChanIok/SpinningMomo/blob/main/LICENSE). The project icon is from the game "Infinity Nikki" and copyrighted by the game developer.

<style>
.center {
  text-align: center;
  max-width: 100%;
  margin: 0 auto;
}
.logo-container {
  display: flex;
  justify-content: center;
  margin: 2rem auto;
}
.logo-container img {
  display: block;
  margin: 0 auto;
}
.title {
  font-size: 2.5rem;
  margin: 1rem 0;
}
.description {
  font-size: 1.2rem;
  color: var(--vp-c-text-2);
  margin: 1rem 0;
}
.badges {
  display: flex;
  justify-content: center;
  gap: 0.5rem;
  margin: 1rem 0;
}
.badges img {
  display: inline-block;
}
.nav-links {
  margin: 1.5rem 0;
}
.nav-links a {
  text-decoration: none;
  font-weight: 500;
}
.screenshot-container {
  max-width: 100%;
  margin: 2rem auto;
}
.screenshot-container img {
  max-width: 100%;
  height: auto;
  border-radius: 8px;
  box-shadow: 0 4px 12px rgba(0, 0, 0, 0.1);
}
.feature-grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(250px, 1fr));
  gap: 1rem;
  margin: 2rem 0;
}
.feature-item {
  padding: 1rem;
  border: 1px solid var(--vp-c-divider);
  border-radius: 8px;
}
.feature-item h3 {
  margin-top: 0;
}
</style> 
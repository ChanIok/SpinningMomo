> *English README translated by Claude AI*
<div align="center">
  <h1>
    <img src="../docs/logo.png" width="200" alt="SpinningMomo Logo">
    <br/>
    🎮 SpinningMomo
    <br/><br/>
    <sup>A window adjustment tool to enhance photography experience in Infinity Nikki</sup>
    <br/>
  </h1>

  <p>
    <img alt="Platform" src="https://img.shields.io/badge/platform-Windows-blue?style=flat-square" />
    <img alt="Release" src="https://img.shields.io/github/v/release/ChanIok/SpinningMomo?style=flat-square&color=brightgreen" />
    <img alt="License" src="https://img.shields.io/badge/license-MIT-orange?style=flat-square" />
  </p>

  <p>
    <b>
      <a href="#-features">Features</a> •
      <a href="#-user-guide">User Guide</a> •
      <a href="#️-build-guide">Build Guide</a> •
      <a href="../README.md">中文</a>
    </b>
  </p>

  <img src="./README.jpg" alt="Screenshot" >
</div>

## 🎯 Introduction

SpinningMomo was initially developed to solve the vertical composition photography issue in Infinity Nikki.

With hotkeys, you can easily switch the game window's aspect ratio and size, perfectly meeting various composition needs. Whether it's vertical photography, album browsing, or snapshot hourglass, you can achieve the best display effect.

It can even break through the original resolution limitations of the game and device, outputting ultra-high resolution photos up to 8K or even 12K!

> If you find this tool helpful, please consider giving it a Star ⭐!

## ✨ Features

<div align="center">
  <table>
    <tr>
      <td align="center">🎮 <b>Portrait Mode</b><br/>Perfect support for vertical UI, snapshot hourglass, and album</td>
      <td align="center">📸 <b>Ultra-High Resolution</b><br/>Support photo output beyond game and device resolution limits</td>
    </tr>
    <tr>
      <td align="center">📐 <b>Flexible Adjustment</b><br/>Multiple presets, custom ratios and resolutions</td>
      <td align="center">⌨️ <b>Hotkey Support</b><br/>Customizable hotkey (default: Ctrl+Alt+R)</td>
    </tr>
    <tr>
      <td align="center">⚙️ <b>Floating Window</b><br/>Optional floating menu for convenient window adjustment</td>
      <td align="center">🚀 <b>Lightweight</b><br/>Minimal resource usage, performance priority</td>
    </tr>
  </table>
</div>

## 📖 User Guide

### Quick Start

1. Run the program as **Administrator**
2. A floating window will appear after startup, where you can adjust aspect ratio and resolution directly
3. Use hotkey (default Ctrl+Alt+R) to show/hide the floating window
4. Use reset option to restore window after shooting

### Photography Mode Selection

1. High Quality Mode (Recommended)
   - Game Settings:
     * Display Mode: Fullscreen Window
     * Photo Quality: Window Resolution
   - Steps:
     1. Use ratio options to adjust composition (e.g., 16:9, 3:2, 9:16)
     2. Select desired resolution preset (4K, 6K, 8K, 12K)
     3. Press space to capture
     4. Click reset window after shooting
   - Advantages:
     * Support ultra-high resolution output (up to 12K and beyond)
     * Sharp image quality with rich details
     * Freely adjustable ratio and resolution

2. Quick Mode
   - Game Settings:
     * Display Mode: Window Mode
     * Photo Quality: 4K
   - Notes:
     * Advantages: Convenient operation, suitable for daily shooting and preview
     * Disadvantages:
       - Outputs pseudo-4K images, actual rendering quality is lower than window resolution option at the same size
       - **Can only adjust ratio, cannot adjust resolution**
     * Suggestion: Sufficient for daily use if image quality is not a priority

### Resolution Explanation
- Resolution calculation process:
  1. First determine total pixel count based on selected resolution preset (e.g., 4K, 8K)
  2. Calculate final width and height based on selected ratio
     - Example: When selecting 8K (about 33.2M pixels) and 9:16 ratio
     - Results in 4320x7680 output resolution (4320x7680=33.2M pixels)
     - Ensures total pixel count matches preset value

### Tray Features

Right-click or left-click the tray icon to:

- 🎯 Select Window: Choose the target window from the submenu
- 📐 Window Ratio: Select from preset ratios or custom ratios
- 📏 Resolution: Select from preset resolutions or custom resolutions
- ⌨️ Modify Hotkey: Set a new shortcut combination (there may be a 1-2 second delay before the success prompt appears)
- 🔔 Show Tips: Enable/disable operation prompts
- 📍 Keep Window Topmost: Keep the game always on top to prevent taskbar overlap
- 🔽 Auto-hide Taskbar: Automatically hide the taskbar to prevent overlap
- 📱 Floating Mode: Enabled by default, you can enable/disable the floating menu here. When disabled, use hotkey (default Ctrl+Alt+R) to open the adjustment menu
- ⚙️ Open Config: Customize ratios and resolutions
- ❌ Exit: Close the program

### Custom Settings

1. Right-click tray icon, select "Open Config File"
2. Add in config file:
   - Custom ratios: Add "16:10,17:10" after RatioList in [CustomRatio] section
     - Use colon(:) for width:height, comma(,) to separate multiple ratios
   - Custom resolutions: Add "6000x4000,7680x4320" after ResolutionList in [CustomResolution] section
     - Use letter x to connect width and height, comma(,) to separate multiple resolutions
3. Save and restart software to apply changes

### Preset Options

- **Ratios**: 32:9, 21:9, 16:9, 3:2, 1:1, 2:3, 9:16
- **Resolutions**:
  - 4K (3840×2160, about 8.3M pixels)
  - 6K (5760×3240, about 18.7M pixels)
  - 8K (7680×4320, about 33.2M pixels)
  - 12K (11520×6480, about 74.6M pixels)

### Requirements

- System: Windows 10 or above
- [Visual C++ Redistributable 2015-2022](https://aka.ms/vs/17/release/vc_redist.x64.exe)

### Notes
- About 4K Photo Quality:
  1. Although the output resolution reaches 4K, the actual rendering quality is lower than the window resolution option at the same size
  2. In fullscreen window mode, the output image ratio will be limited to the monitor's native ratio and its corresponding vertical ratio
  3. For best image quality, it's recommended to use fullscreen window mode + window resolution
- Higher resolutions may affect game performance, please adjust according to your device capabilities
- It's recommended to test quality comparison before shooting to choose the most suitable settings

## 📄 License

This project is licensed under the [MIT License](../LICENSE).
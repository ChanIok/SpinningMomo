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

<div align="center">
  <table>
    <tr>
      <th align="center">📸 Window Resolution Mode (Recommended)</th>
      <th align="center">🎯 Standard Mode</th>
    </tr>
    <tr>
      <td>
        <b>Game Settings</b><br/>
        ▫️ Display Mode: <b>Fullscreen Window (Recommended)</b> / Window Mode<br/>
        ▫️ Photo Quality: <b>Window Resolution</b>
      </td>
      <td>
        <b>Game Settings</b><br/>
        ▫️ Display Mode: <b>Window Mode</b> / Fullscreen Window (ratio limited)<br/>
        ▫️ Photo Quality: <b>4K</b>
      </td>
    </tr>
    <tr>
      <td>
        <b>Steps</b><br/>
        1️⃣ Use ratio options to adjust composition<br/>
        2️⃣ Select desired resolution preset (4K~12K)<br/>
        3️⃣ Screen will exceed display bounds, press space to capture<br/>
        4️⃣ Click reset window after shooting
      </td>
      <td>
        <b>Notes</b><br/>
        ✅ Convenient operation, suitable for daily shooting and preview<br/>
        ❗ Can only adjust ratio, cannot adjust resolution<br/>
        ❗ Outputs pseudo-4K, quality lower than window resolution<br/>
        ❗ In fullscreen window mode, output limited by monitor's native ratio
      </td>
    </tr>
    <tr>
      <td>
        <b>Advantages</b><br/>
        ⭐ Support ultra-high resolution (up to 12K+)<br/>
        ⭐ Freely adjustable ratio and resolution
      </td>
      <td>
        <b>Best For</b><br/>
        ⭐ Quick composition check<br/>
        ⭐ When image quality is not priority<br/>
      </td>
    </tr>
  </table>
</div>

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
- 📍 Screenshot: Save lossless screenshots to the ScreenShot folder in program directory, mainly for debugging and games that don't support screenshots
- 🔽 Hide Taskbar: Hide the taskbar to prevent overlap
- ⬇️ Lower Taskbar When Resizing: Lower taskbar when resizing window to prevent overlap
- 🔔 Show Tips: Enable/disable window operation prompts
- ⌨️ Modify Hotkey: Set a new shortcut combination (may have 5 second delay before success prompt)
- 🔍 Preview Window: Similar to Photoshop's navigator, provides real-time preview and navigation when window exceeds screen bounds (optional feature)
  - Support dragging window top area to move position, mouse wheel to zoom window size
- 📱 Floating Mode: Enabled by default, can enable/disable floating menu here. When disabled, use hotkey (default Ctrl+Alt+R) to open adjustment menu
- 🌐 Language: Switch language
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

## 📄 License

This project is licensed under the [MIT License](../LICENSE). The project icon is from the game "Infinity Nikki" and copyrighted by the game developer.
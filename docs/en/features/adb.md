# ADB Mode

ADB (Android Debug Bridge) is Android's official debugging tool. By connecting to emulators or physical Android devices over ADB, you can control device resolution from your PC and capture screenshots or video recordings.

## Supported Games

- *Shining Nikki*

## Supported Devices

- **Android Emulators**: Automatically discovers running MuMu, LDPlayer, and BlueStacks instances without manual setup
- **Physical Devices**: Requires initial setup, see below

## Quick Start (Emulators)

1. **Enable the menu button**: Settings → Floating Window → Features, toggle on **ADB Mode**
2. Launch your emulator and start the game
3. Click the **ADB Mode** button on the floating window to discover and connect

Once connected, taking screenshots and recording works just like standard window mode.

## Recording Notes

Records both screen video and in-game audio to MP4. Configure bitrate (default 40 Mbps), framerate, and video codec (H.264 / H.265) under **Settings → ADB Mode**.

## Aspect Ratio Safety Notice

::: warning Match Your Current Orientation
- **In Portrait Mode** (vertical shooting or phone-mode emulator): Only select vertical or square ratios such as **9:16, 3:4, 2:3, 1:1**. **Do not switch to horizontal ratios like 16:9 or 21:9.**
- Changing aspect ratios across orientations (forcing landscape while portrait) can trigger abnormal stretching, screen inversion, misaligned touch coordinates, or emulator display freezes.
- **In case of display glitches**: Select **Default** in the floating window's resolution menu or turn off ADB Mode to immediately restore the device's native display dimensions.
:::

## Limitations

Using MuMu Player as an example:

- **Screenshot Resolution**: Supported up to 7680px along the long edge
- **Recording Resolution**: Supported up to 2560px along the long edge (limited by Android system video encoders)

For ultra-high resolution recording, use the emulator's built-in recorder. Physical Android devices are typically not subject to this recording resolution ceiling.

## Connecting Physical Devices

**Prerequisites**

1. Download [Android Platform Tools](https://developer.android.com/tools/releases/platform-tools) and extract `adb.exe`
2. Enable **Developer Options** and **USB Debugging** on your phone

**Connection Steps**

1. Connect your phone to your PC via USB
2. Go to **Settings → ADB Mode**, enable **Custom ADB Path**, and select your `adb.exe`
3. Select your device from the list, or enter its IP and port for wireless debugging

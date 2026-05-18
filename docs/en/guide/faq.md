# FAQ

## How does it work?

The tool works with how game engines and Windows handle windows.

### 1. Render Resolution

Most modern game engines (UE4/5, Unity, etc.) tie render resolution to window size when in Windowed or Borderless mode.

```
// UE4/5 example
r.ScreenPercentage  // controls the ratio of actual render resolution to window size
```

When the window is resized, the engine re-renders at the new resolution — even if the window is larger than your monitor. So when *Infinity Nikki*'s **Photo Quality** is set to **Window Resolution**, the game outputs photos at whatever resolution the window is set to.

### 2. Window Management

The tool uses standard Windows APIs to resize and reposition the game window.

```
SetWindowPos()   // resize and reposition
WS_POPUP         // switch to borderless when needed
```

These are the same system-level window operations Windows performs when you change your display resolution, rotate the screen, or move a window across monitors.

### 3. Safety

::: danger The tool does NOT:
- Modify game memory
- Inject into the game process
- Alter game files
:::

## Installer fails

Click **View log file** in the error dialog, then paste the log into an AI tool for diagnosis. The cause varies by system, and AI analysis is usually the fastest way to resolve it.

## Main UI shows a white/blank screen

The web resources are probably missing. If you're using the portable version, make sure you extracted **all files** from the archive into a dedicated folder.

## Can't drag the main window

Update your [WebView2 Runtime](https://developer.microsoft.com/en-us/microsoft-edge/webview2/). An outdated version can break interaction.

## Photo resolution or aspect ratio is wrong

Check these two settings in *Infinity Nikki*:
- **Display mode**: **Window**
- **Photo Quality**: **Window Resolution** (not a fixed preset like 4K)

## How to uninstall

Make sure the program is fully closed first.
- **Installer version**: Go to **Settings > Apps > Installed apps**, find SpinningMomo, and click **Uninstall**
- **Portable version**: Just delete the program folder

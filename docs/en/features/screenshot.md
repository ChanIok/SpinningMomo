# High-Res Screenshots

Captures the current frame of the target window; output size follows the window's current aspect ratio and resolution.

## In-Game Camera vs. App Screenshot

The two come from different sources and produce different results.

### In-Game Camera (Recommended)

Using *Infinity Nikki*'s **Momo's Camera** as an example, the image is rendered by the game itself. The game pauses rendering when the camera is open, avoiding motion blur and temporal artifacts, and keeps the in-game photo metadata.

Click **Game Album** on the floating window to open the game's album folder directly.

### App Screenshot

Captures the target window directly, at the same resolution as the current window size. Default hotkey: `F11`, or click **Capture** on the floating window.

Use it when the in-game camera can't save your current ultra-high resolution, or when you need to capture another window.

::: warning Capture issue when the window exceeds the screen
Some games (such as *Final Fantasy XIV* and *The Sims 4*) only render and refresh the portion of the frame that fits on screen once the window grows past the physical display, so screenshots and previews only contain the visible region.

Enable **Over-Screen Window Capture Workaround** under **Settings > Window Control**, or simply turn on the [Overlay](./window.md).
:::

## Screenshot Settings

- **Format**: PNG (default, lossless) or JPEG (always saved at maximum quality). JPEG is recommended for everyday use — it saves faster and takes less space; switch to PNG when you need lossless output for editing.
- **Borderless Capture**: Captures only the window client area, excluding the title bar and window borders.

## Output Location

Screenshots and recordings share the same output directory, saved to `SpinningMomo` under your system **Videos** folder by default. Change it under **Settings > Output Directory**, or have subfolders created automatically from the target window title.

A notification pops up when saving finishes — click **View** to open the file.

## HDR

With HDR enabled in Windows, screenshots can be saved as Ultra HDR JPEG, optionally alongside a lossless JXR master. See [HDR Support](./hdr.md).

## Experimental

Long Exposure (multi-frame stacking) under **Advanced Photography** on the floating window is experimental, has notable limitations, and may be changed or removed later.

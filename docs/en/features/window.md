# Window Adjustment

Freely adjust target window aspect ratios and ultra-high resolutions beyond physical screen limits for photography and composition.

## Picking a Target Window

::: info Supported games
The tool defaults to *Infinity Nikki* but works with most modern games running in windowed mode — for example:
- *Final Fantasy XIV*
- *The Sims 4*
- *Naraka: Bladepoint*
- *Wuthering Waves*
- *Arknights: Endfield*
- *Neverness to Everness*

To switch to a different window, **right-click the floating window or tray icon** and pick a window from the menu. **The game must be set to windowed mode.**
:::

## Aspect Ratio Presets

| Ratio | Use Case |
|-------|----------|
| 21:9 | Ultrawide, cinematic framing |
| 16:9 | Standard widescreen |
| 3:2 | Classic camera proportion |
| 1:1 | Square composition |
| 3:4 | Social media (Xiaohongshu-friendly) |
| 2:3 | Portrait orientation |
| 9:16 | Short video (TikTok-style) |

## Resolution Presets

| Preset | Equivalent Base | Total Pixels |
|--------|----------------|--------------|
| Default | Follows screen | — |
| 1080P  | 1920×1080      | ~2.1 MP |
| 2K     | 2560×1440      | ~3.7 MP |
| 4K     | 3840×2160      | ~8.3 MP |
| 6K     | 5760×3240      | ~18.7 MP |
| 8K     | 7680×4320      | ~33.2 MP |
| 12K    | 11520×6480     | ~74.6 MP |

::: tip How resolution works
The tool first calculates total pixels from the selected preset, then distributes them according to your chosen aspect ratio.
For example: **8K + 9:16** > **4320×7680** (roughly the same pixel count as 8K).

**Default** sets no pixel count; it takes the largest size your screen can fit at the current aspect ratio.
:::

::: warning Performance
Higher resolutions consume more VRAM, RAM, and virtual memory. On an RTX 3060 12G + 32 GB RAM system, 12K causes noticeable lag. If the game crashes, check your resource usage in Task Manager or increase your virtual memory.
:::

## Custom Presets

Both ratio and resolution presets can be added or removed under **Settings > Floating Window > Menu Settings**, and reordered by dragging.

- **Aspect Ratio**: Enter as `width:height`, e.g. `16:9` or `4:5`.
- **Resolution**: Enter as `width x height`, e.g. `1920x1080`; shorthands such as `4K` or `1080P` also work.

Newly added presets go to the end of the list — drag them to wherever you use them most.

## Assistive Features

When the resized window extends beyond your physical display, turn on any of the following to help inspect and interact with the frame:

### Preview

Similar to Photoshop's Navigator. Shows a real-time preview of the full off-screen frame in a separate floating window, with scroll-to-zoom and drag-to-pan.

### Overlay

Like a reverse, downscaled Magpie. Captures the oversized window and renders it scaled down to fullscreen, synchronizing cursor position dynamically so clicking and interacting still work at ultra-high resolutions.

### Letterbox

Adds a fullscreen black background behind the game window to hide the desktop for an immersive experience with non-native aspect ratios.

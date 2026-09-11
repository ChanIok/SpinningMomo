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
| 1080P  | 1920×1080      | ~2.1 MP |
| 2K     | 2560×1440      | ~3.7 MP |
| 4K     | 3840×2160      | ~8.3 MP |
| 6K     | 5760×3240      | ~18.7 MP |
| 8K     | 7680×4320      | ~33.2 MP |
| 12K    | 11520×6480     | ~74.6 MP |

::: tip How resolution works
The tool first calculates total pixels from the selected preset, then distributes them according to your chosen aspect ratio.
For example: **8K + 9:16** → **4320×7680** (roughly the same pixel count as 8K).
:::

::: warning Performance
Higher resolutions consume more VRAM, RAM, and virtual memory. On an RTX 3060 12G + 32 GB RAM system, 12K causes noticeable lag. If the game crashes, check your resource usage in Task Manager or increase your virtual memory.
:::

## Assistive Features

When the window extends beyond your physical display, use the **Preview** window or **Overlay** to help inspect and interact:

- **Preview**: Floating window similar to Photoshop's Navigator. Provides real-time preview of off-screen content with scroll-to-zoom and drag-to-pan.
- **Overlay**: Acts like an inverted downscaling mode (similar to a reverse Magpie). Captures and scales the oversized window to fit fullscreen while synchronizing mouse positions for normal clicking and interaction.
- **Letterbox**: Adds a fullscreen black background behind the game window to hide the desktop for an immersive experience with non-native aspect ratios.

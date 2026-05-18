# Window & Resolution

## Picking a Target Window

::: info Supported games
The tool defaults to *Infinity Nikki* but works with most modern games running in windowed mode — for example:
- *Final Fantasy XIV*
- *Naraka: Bladepoint*
- *Wuthering Waves*
- *Where Winds Meet*
- *Arknights: Endfield*
- *Honor of Kings: World*
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

**Preview Window**: When the game window extends beyond your screen, this floating preview (like Photoshop's Navigator) gives you a real-time view with scroll-to-zoom and drag-to-pan.

**Overlay**: Renders the oversized window scaled down into a fullscreen overlay, keeping mouse interactions normal at ultra-high resolutions. Adds CPU overhead — turn it off if you're already seeing lag.

**Letterbox**: Adds a full-screen black background behind the window for a more immersive look when using non-native aspect ratios.

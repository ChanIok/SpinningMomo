# Video Recording

Lightweight video and audio recording that automatically tracks custom window aspect ratios and resolutions.

## How to Use

1. Adjust the game window to your desired ratio and resolution
2. Click **Record** on the floating window to start recording (click again to stop)

Default hotkey: `F8`

Videos are saved as MP4 files under `SpinningMomo` in your system **Videos** folder (configurable in Settings), at the same resolution as the current window size.

## Compared to OBS & External Tools

Built-in recording uses native Windows capture pipelines (WGC + Media Foundation hardware acceleration) for minimal VRAM and system overhead, automatically matching window dimensions and aspect ratios.

For live streaming, multi-track audio routing, or complex scene compositing, consider pairing with OBS or other dedicated software.

::: warning Performance & Format Requirements
- **Hardware Requirements**: Recording at ultra-high resolutions (such as 6K / 8K) puts heavy demand on GPU hardware encoders and disk write speeds.
- **Encoding Limits**: Resolutions above 4K require **H.265 (HEVC)** encoding in settings (H.264 does not support resolutions exceeding 4K).
:::

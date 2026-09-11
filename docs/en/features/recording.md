# Video Recording

Records video and audio from the target window at the same resolution as the current window size.

## Starting & Stopping

Click **Record** on the floating window to start, then click again to stop. Default hotkey: `F8`.

## Recording Settings

- **Frame Rate**: 60 FPS by default.
- **Rate Control**: Quality-based (default, quality 80), constant bitrate, or manual QP.
- **Codec**: H.264 or H.265.
- **Encoder Mode**: Auto (default, prefers GPU hardware encoding), forced hardware encoding, or CPU software encoding.
- **Audio Source**: System Audio (default), Game Audio Only, or No Audio.
- **Show Cursor**: Whether the mouse cursor is recorded into the video.
- **Borderless Capture**: Captures only the window client area, excluding the title bar and window borders.

## Auto Split On Resize

If the window ratio or resolution changes during recording, the current segment is ended automatically and recording continues at the new size. This prevents a mid-recording size change from stretching or cropping the whole clip. Can be turned off in settings.

## Output Location

Output is an MP4 file, saved to the same output directory as screenshots.

## HDR

When enabled, records as HEVC HDR10 video, which requires GPU hardware encoding and the H.265 format. See [HDR Support](./hdr.md).

## High-Resolution Recording

::: warning Hardware & Encoding Limits
- **Hardware Requirements**: 6K / 8K recording puts heavy demand on GPU encoders and disk write speeds.
- **Codec Limits**: With GPU hardware encoding, H.264 tops out at roughly 4096 (a limit shared by NVIDIA, AMD, and Intel hardware encoders). For anything above 4K, switch to **H.265** (upper limit around 8192).
:::

## Compared to Other Recorders

**Lower overhead**: Capture and encoding run on native Windows pipelines and prefer GPU hardware encoding, with no scene compositing or streaming, so VRAM and system usage stay below general-purpose screen recorders.

For live streaming, multi-track audio, or complex scene compositing, use dedicated tools such as OBS.

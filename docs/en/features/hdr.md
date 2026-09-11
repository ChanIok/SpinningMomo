# HDR Support

Capture high dynamic range (HDR) screenshots and videos when HDR is enabled on Windows.

## Enabling It

1. Make sure **Windows HDR** is turned on in your system display settings
2. Go to **Settings > Capture & Output > HDR** and enable the features you want

## HDR Screenshots

### Ultra HDR JPEG

A standard image format based on gain map technology, offering both high dynamic range and broad compatibility:

- **On HDR-capable devices**: Renders bright, punchy highlights directly in major mobile photo apps, modern browsers, and platforms such as Xiaohongshu.
- **On regular SDR devices**: Falls back to a normal photo on displays or apps without HDR support — the image will not look washed out.

### Lossless JXR (Optional)

Enabling **Save JXR too** writes an additional lossless file alongside the screenshot. It stores the raw dynamic range and color output by the game as 16-bit floating point (FP16), making it a high-spec master copy.

Designed for deep retouching and color grading in Photoshop (opening it there requires the [official Microsoft JPEG XR plugin](https://www.microsoft.com/en-us/download/details.aspx?id=52369)).

## HDR Recording

Records as **HEVC HDR10 (10-bit)** video.

::: warning Recording Requirements
You must select both a **GPU encoder** and the **H.265 (HEVC)** format in settings (CPU encoding and H.264 are not supported).
:::

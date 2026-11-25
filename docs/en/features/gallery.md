# Gallery

A local tool for browsing, filtering, and organizing your photos and videos.

## Browsing & Filtering

- **Views**: Grid, Masonry, List, and Adaptive (equal-height rows that fill the width while keeping each photo's original aspect ratio).
- **Date Grouping**: Group by month or day. Grid and Adaptive views only.
- **Filters**: Folder, tag, source, file type, shape (landscape / portrait / square), rating, reject flag, dominant color, date range, and file name — all combinable.
- **Sorting**: Created date, name, resolution, size.

## Culling & Organizing

- **Ratings & Rejects**: 1–5 star ratings plus a "Reject" flag. Filter out rejects in one click and clean them up in bulk.
- **Drag-and-Drop**: Drag a selection onto a folder in the left panel to move files, or onto the tag bar to assign tags.
- **Clipboard**: Copy files to the system clipboard, or paste a screenshot from the clipboard straight into the gallery.
- **Selection**: Select all, invert selection, and batch operations.

## Details Panel & Photo Viewer

Selecting a photo or video shows its dominant colors, resolution, file size, creation date, custom description, and histogram in the right panel.

Double-click to open the photo viewer (single tap on touch input), with zoom, video playback, and continuous browsing.

## Network Storage

Full support for network shared folders (UNC paths, such as NAS or LAN network drives).

When a directory is temporarily unreachable, watching and scanning are skipped, so its photos are never wrongly marked as missing. The Windows Recycle Bin does not support UNC paths — deleting files on a network location deletes them permanently.

## Phones & Tablets

Open the gallery in a phone or tablet browser on the same local network and it automatically switches to a compact, touch-friendly layout with swipe navigation and pinch-to-zoom. See [LAN Access](./lan.md).

## Infinity Nikki Extension

- **Camera Parameters**: Automatically parses photo shooting parameters, allowing you to copy camera parameter codes anytime for in-game import.
- **Photo Map**: Pins photos onto the in-game map based on shot locations. Hover over pins for photo previews, or click pins to jump to the photo viewer.
- **Media Hardlinks**: The game saves duplicate copies of photos and videos by default. Enabling hardlinks keeps a single physical copy to save disk space without affecting viewing or sharing.

::: tip Missing parameters on some photos?
Right-click a folder in the sidebar > Extract Infinity Nikki Metadata, then enter your **UID** to re-run extraction.
:::

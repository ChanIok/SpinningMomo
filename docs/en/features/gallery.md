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

## Browsing Performance

Measured on directories holding around 100,000 photos, scrolling, load speed, and memory usage all stay within normal ranges.

- Only cards near the current viewport are rendered, so per-frame render cost is independent of the total photo count.
- Photos are loaded page by page on demand; switching filters or sorting also fetches only the pages near the current viewport.
- Only thumbnails are decoded while scrolling. Full-size images are swapped in one by one once scrolling settles, so decoding never blocks scrolling.

## File Changes & Annotation Retention

- **Directory Watching**: Additions, removals, and edits inside a directory are reflected in the gallery in real time. On startup, the NTFS change journal is used to catch up on changes made while the app was closed. Network directories do not support this and are re-scanned instead.
- **Move & Rename**: Moving or renaming a file in File Explorer does not immediately discard its annotations — they are retained for 30 days. Put the file back within that window and its tags, rating, and description reattach automatically.
- **Copy & Save As**: The new path is indexed as a new record. If its content is exactly identical to an existing photo, tags, rating, and description are inherited once; after that the two copies stay independent.

## Network Storage

Full support for network shared folders (UNC paths, such as NAS or LAN network drives).

When a directory is temporarily unreachable, watching and scanning are skipped, so its photos are never wrongly marked as missing. The Windows Recycle Bin does not support UNC paths — deleting files on a network location deletes them permanently.

## Phones & Tablets

Open the gallery in a phone or tablet browser on the same local network and it automatically switches to a compact, touch-friendly layout with swipe navigation and pinch-to-zoom. See [LAN Access](./lan.md).

## Infinity Nikki Extension

- **Camera Parameters**: Automatically parses photo shooting parameters, allowing you to copy camera parameter codes anytime for in-game import.
- **Dye Codes**: Record and copy outfit dye codes for easy outfit management and sharing.
- **Photo Map**: Pins photos onto the in-game map based on shot locations. Hover over pins for photo previews, or click pins to jump to the photo viewer.

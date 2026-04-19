/**
 * 注入 runtime 内联片段：照片缩略图 cell 与单点悬停卡片 HTML（依赖同作用域内 popup.js 已定义的 escapeHtml）。
 */
export function buildPhotoCardSnippet() {
  return `
  const buildPhotoThumbCellHtml = (item) => {
    if (!item || typeof item !== 'object') {
      return (
        '<div style="width:100%;height:100%;aspect-ratio:1/1;border-radius:6px;background:#1f2937;display:flex;align-items:center;justify-content:center;color:#9ca3af;font-size:11px;">无图</div>'
      );
    }

    const hasAssetId = Number.isFinite(Number(item.assetId));
    const assetIdAttr = hasAssetId ? ' data-sm-open-asset-id="' + String(item.assetId) + '"' : '';
    const hasAssetIndex = Number.isFinite(Number(item.assetIndex));
    const assetIndexAttr = hasAssetIndex
      ? ' data-sm-open-asset-index="' + String(item.assetIndex) + '"'
      : '';
    const cellStyle = hasAssetId ? 'width:100%;height:100%;cursor:pointer;' : 'width:100%;height:100%;';

    if (item.thumbnailUrl) {
      const innerHtml =
        '<img src="' +
        escapeHtml(String(item.thumbnailUrl)) +
        '" loading="lazy" style="width:100%;height:100%;aspect-ratio:1/1;object-fit:cover;border-radius:6px;background:#1f2937;display:block;" />';
      return '<div' + assetIdAttr + assetIndexAttr + ' style="' + cellStyle + '">' + innerHtml + '</div>';
    }

    const fallback =
      '<div style="width:100%;height:100%;aspect-ratio:1/1;border-radius:6px;background:#1f2937;display:flex;align-items:center;justify-content:center;color:#9ca3af;font-size:11px;">无图</div>';
    return '<div' + assetIdAttr + assetIndexAttr + ' style="' + cellStyle + '">' + fallback + '</div>';
  };

  const buildSinglePhotoHoverHtml = (marker) => {
    const title = escapeHtml(String(marker && marker.cardTitle != null ? marker.cardTitle : ''));
    const thumbnailUrl = marker && marker.thumbnailUrl ? String(marker.thumbnailUrl) : '';
    const hasAssetId = marker && Number.isFinite(Number(marker.assetId));
    const hasAssetIndex = marker && Number.isFinite(Number(marker.assetIndex));
    const assetIdAttr = hasAssetId ? ' data-sm-open-asset-id="' + String(Number(marker.assetId)) + '"' : '';
    const assetIndexAttr = hasAssetIndex
      ? ' data-sm-open-asset-index="' + String(Number(marker.assetIndex)) + '"'
      : '';
    const cursorStyle = hasAssetId ? 'cursor:pointer;' : '';

    let thumbBlock = '';
    if (thumbnailUrl) {
      thumbBlock =
        '<div class="spinning-momo-popup-thumbnail-block">' +
        '<div class="spinning-momo-popup-thumbnail-link" style="' +
        cursorStyle +
        '"' +
        assetIdAttr +
        assetIndexAttr +
        '>' +
        '<img class="spinning-momo-popup-thumbnail-image" src="' +
        escapeHtml(thumbnailUrl) +
        '" alt="' +
        title +
        '" loading="eager" decoding="async" />' +
        '</div>' +
        '</div>';
    } else {
      thumbBlock =
        '<div class="spinning-momo-popup-thumbnail-block">' +
        '<div class="spinning-momo-popup-thumbnail-link" style="' +
        cursorStyle +
        '"' +
        assetIdAttr +
        assetIndexAttr +
        '>' +
        '<div class="spinning-momo-popup-thumbnail-fallback">无图</div>' +
        '</div>' +
        '</div>';
    }

    return (
      '<div style="line-height: 1.5;">' +
      '<div class="spinning-momo-popup-body">' +
      '<div class="spinning-momo-popup-title">' +
      title +
      '</div>' +
      thumbBlock +
      '</div>' +
      '</div>'
    );
  };
`
}

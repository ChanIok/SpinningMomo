export function buildDevPolygonCollectorSnippet() {
  return `
  const mountOrUpdateDevPolygonCollector = (payload) => {
    const L = payload && payload.L;
    const map = payload && payload.map;
    if (!L || !map) return;

    if (!window.__SPINNING_MOMO_RUNTIME__) {
      window.__SPINNING_MOMO_RUNTIME__ = {};
    }
    const runtime = window.__SPINNING_MOMO_RUNTIME__;

    const cardId = 'spinning-momo-polygon-collector-card';
    const bodyId = 'spinning-momo-polygon-collector-body';
    const collapseBtnId = 'spinning-momo-polygon-collapse-btn';
    const pointListId = 'spinning-momo-polygon-point-list';
    const regionInputId = 'spinning-momo-polygon-region-name-input';
    const xInputId = 'spinning-momo-polygon-x-input';
    const yInputId = 'spinning-momo-polygon-y-input';
    const closedCheckboxId = 'spinning-momo-polygon-closed-checkbox';
    const zMinInputId = 'spinning-momo-polygon-z-min-input';
    const zMaxInputId = 'spinning-momo-polygon-z-max-input';
    const addBtnId = 'spinning-momo-polygon-add-btn';
    const exportBtnId = 'spinning-momo-polygon-export-btn';
    const clearBtnId = 'spinning-momo-polygon-clear-btn';

    const ensureStyles = () => {
      const styleId = 'spinning-momo-polygon-collector-style';
      const styleText = [
        '.spinning-momo-polygon-card {',
        '  position: fixed;',
        '  top: 56px;',
        '  right: 16px;',
        '  z-index: 9999;',
        '  width: 320px;',
        '  max-height: min(68vh, 640px);',
        '  box-sizing: border-box;',
        '  padding: 10px;',
        '  border-radius: 10px;',
        '  background: rgba(36, 31, 22, 0.94);',
        '  color: #f6efdf;',
        '  box-shadow: 0 8px 24px rgba(0, 0, 0, 0.3);',
        '  font-family: "Segoe UI", "Microsoft YaHei", sans-serif;',
        '  font-size: 12px;',
        '}',
        '.spinning-momo-polygon-card-header {',
        '  display: flex;',
        '  align-items: center;',
        '  justify-content: space-between;',
        '  gap: 8px;',
        '  font-weight: 600;',
        '  margin-bottom: 8px;',
        '}',
        '.spinning-momo-polygon-card.is-collapsed .spinning-momo-polygon-card-header {',
        '  margin-bottom: 0;',
        '}',
        '.spinning-momo-polygon-row {',
        '  display: flex;',
        '  align-items: center;',
        '  gap: 6px;',
        '  margin-bottom: 6px;',
        '}',
        '.spinning-momo-polygon-row label {',
        '  color: #dcc9ab;',
        '  min-width: 28px;',
        '}',
        '.spinning-momo-polygon-row input[type="text"],',
        '.spinning-momo-polygon-row input[type="number"] {',
        '  flex: 1;',
        '  min-width: 0;',
        '  box-sizing: border-box;',
        '  border: 1px solid rgba(255, 255, 255, 0.18);',
        '  border-radius: 6px;',
        '  background: rgba(14, 12, 8, 0.6);',
        '  color: #fff5e3;',
        '  padding: 4px 6px;',
        '  outline: none;',
        '}',
        '.spinning-momo-polygon-row input[type="text"]:focus,',
        '.spinning-momo-polygon-row input[type="number"]:focus {',
        '  border-color: rgba(245, 220, 177, 0.72);',
        '}',
        '.spinning-momo-polygon-btn {',
        '  border: none;',
        '  border-radius: 6px;',
        '  background: #7b5d4a;',
        '  color: #fff6ea;',
        '  padding: 5px 8px;',
        '  cursor: pointer;',
        '}',
        '.spinning-momo-polygon-btn:disabled {',
        '  cursor: not-allowed;',
        '  opacity: 0.45;',
        '}',
        '.spinning-momo-polygon-btn-row {',
        '  display: flex;',
        '  align-items: center;',
        '  gap: 6px;',
        '  margin: 8px 0;',
        '}',
        '.spinning-momo-polygon-list {',
        '  max-height: min(40vh, 300px);',
        '  overflow-y: auto;',
        '  border: 1px solid rgba(255, 255, 255, 0.14);',
        '  border-radius: 8px;',
        '  padding: 6px;',
        '  background: rgba(0, 0, 0, 0.16);',
        '}',
        '.spinning-momo-polygon-item {',
        '  display: flex;',
        '  align-items: center;',
        '  gap: 6px;',
        '  margin-bottom: 6px;',
        '}',
        '.spinning-momo-polygon-item input[type="number"] {',
        '  width: 96px;',
        '  min-width: 96px;',
        '  flex: none;',
        '}',
        '.spinning-momo-polygon-item:last-child {',
        '  margin-bottom: 0;',
        '}',
        '.spinning-momo-polygon-index {',
        '  width: 18px;',
        '  color: #d9be94;',
        '  text-align: right;',
        '  flex: none;',
        '}',
        '.spinning-momo-polygon-mini-btn {',
        '  border: none;',
        '  border-radius: 4px;',
        '  background: rgba(255, 255, 255, 0.16);',
        '  color: #ffe8c4;',
        '  padding: 2px 6px;',
        '  cursor: pointer;',
        '}',
        '.spinning-momo-polygon-footnote {',
        '  margin-top: 6px;',
        '  color: #cfb998;',
        '}',
      ].join('\\n');

      let style = document.getElementById(styleId);
      if (!style) {
        style = document.createElement('style');
        style.id = styleId;
        document.head.appendChild(style);
      }
      if (style.textContent !== styleText) {
        style.textContent = styleText;
      }
    };

    const ensureDraftState = () => {
      if (!runtime.devPolygonDraft || typeof runtime.devPolygonDraft !== 'object') {
        runtime.devPolygonDraft = {
          regionName: 'A区域',
          points: [],
          closed: true,
          collapsed: false,
          zRange: {
            min: -100000,
            max: 100000,
          },
        };
        return;
      }
      const draft = runtime.devPolygonDraft;
      if (typeof draft.regionName !== 'string') draft.regionName = 'A区域';
      if (!Array.isArray(draft.points)) draft.points = [];
      if (typeof draft.closed !== 'boolean') draft.closed = true;
      if (typeof draft.collapsed !== 'boolean') draft.collapsed = false;
      if (!draft.zRange || typeof draft.zRange !== 'object') {
        draft.zRange = { min: -100000, max: 100000 };
      }
      if (!Number.isFinite(draft.zRange.min)) draft.zRange.min = -100000;
      if (!Number.isFinite(draft.zRange.max)) draft.zRange.max = 100000;
    };

    const ensureLayer = () => {
      if (!runtime.devPolygonLayer) {
        runtime.devPolygonLayer = L.layerGroup().addTo(map);
      } else if (runtime.devPolygonLayer.addTo) {
        runtime.devPolygonLayer.addTo(map);
      }
      return runtime.devPolygonLayer;
    };

    const redrawDraft = () => {
      ensureDraftState();
      const layer = ensureLayer();
      if (!layer || !layer.clearLayers) return;
      layer.clearLayers();

      const draft = runtime.devPolygonDraft;
      const points = draft.points.filter(
        (point) => point && Number.isFinite(point.lat) && Number.isFinite(point.lng)
      );
      if (points.length === 0) return;

      const latLngs = points.map((point) => [point.lat, point.lng]);
      if (draft.closed === true && points.length >= 3) {
        latLngs.push([points[0].lat, points[0].lng]);
      }
      const line = L.polyline(latLngs, {
        color: '#ff4d4f',
        weight: 2,
        opacity: 0.95,
        dashArray: draft.closed ? '' : '4,4',
      });
      line.addTo(layer);

      points.forEach((point, index) => {
        const vertex = L.circleMarker([point.lat, point.lng], {
          radius: 5,
          color: '#ff4d4f',
          weight: 2,
          fillColor: '#ff4d4f',
          fillOpacity: 0.32,
        });
        vertex.bindTooltip(String(index + 1), { permanent: true, direction: 'top', opacity: 0.75 });
        vertex.addTo(layer);
      });
    };

    const buildPointListHtml = () => {
      const draft = runtime.devPolygonDraft;
      const points = draft.points || [];
      if (points.length === 0) {
        return '<div class="spinning-momo-polygon-footnote">尚未添加点，输入 x/y 后点击“添加点”。</div>';
      }
      return points
        .map((point, index) => {
          return (
            '<div class="spinning-momo-polygon-item">' +
            '<span class="spinning-momo-polygon-index">' +
            (index + 1) +
            '.</span>' +
            '<input type="number" step="any" value="' +
            String(point.lng) +
            '" data-sm-point-field="lng" data-sm-point-index="' +
            index +
            '" />' +
            '<input type="number" step="any" value="' +
            String(point.lat) +
            '" data-sm-point-field="lat" data-sm-point-index="' +
            index +
            '" />' +
            '<button class="spinning-momo-polygon-mini-btn" type="button" data-sm-point-delete="' +
            index +
            '">删</button>' +
            '</div>'
          );
        })
        .join('');
    };

    const syncCard = () => {
      ensureDraftState();
      const draft = runtime.devPolygonDraft;
      const card = document.getElementById(cardId);
      if (!card) return;

      const regionInput = card.querySelector('#' + regionInputId);
      if (regionInput && regionInput.value !== draft.regionName) regionInput.value = draft.regionName;
      const closedCheckbox = card.querySelector('#' + closedCheckboxId);
      if (closedCheckbox) closedCheckbox.checked = draft.closed === true;
      const zMinInput = card.querySelector('#' + zMinInputId);
      if (zMinInput) zMinInput.value = String(draft.zRange.min);
      const zMaxInput = card.querySelector('#' + zMaxInputId);
      if (zMaxInput) zMaxInput.value = String(draft.zRange.max);
      const list = card.querySelector('#' + pointListId);
      if (list) list.innerHTML = buildPointListHtml();

      const collapseBtn = card.querySelector('#' + collapseBtnId);
      if (collapseBtn) {
        collapseBtn.textContent = draft.collapsed ? '展开' : '收起';
        collapseBtn.setAttribute('aria-expanded', draft.collapsed ? 'false' : 'true');
      }
      const body = card.querySelector('#' + bodyId);
      if (body) body.style.display = draft.collapsed ? 'none' : 'block';
      if (draft.collapsed) card.classList.add('is-collapsed');
      else card.classList.remove('is-collapsed');

      const exportBtn = card.querySelector('#' + exportBtnId);
      if (exportBtn) exportBtn.disabled = draft.points.length < 3;
    };

    const exportDraft = () => {
      const draft = runtime.devPolygonDraft;
      if (!window.parent || window.parent === window) return;
      window.parent.postMessage(
        {
          action: 'SPINNING_MOMO_EXPORT_POLYGON',
          payload: {
            regionName: draft.regionName,
            coordinateSystem: 'map_latlng',
            points: draft.points.map((point) => ({
              lat: Number(point.lat),
              lng: Number(point.lng),
            })),
            closed: draft.closed,
            zRange: {
              min: Number(draft.zRange.min),
              max: Number(draft.zRange.max),
            },
            exportedAt: Date.now(),
          },
        },
        '*'
      );
    };

    const bindCardEvents = (card) => {
      if (!card || card.dataset.smPolygonCollectorBound === 'true') return;
      card.dataset.smPolygonCollectorBound = 'true';

      card.addEventListener('click', (event) => {
        const target = event.target;
        if (!target || !target.getAttribute) return;

        const deleteIndexRaw = target.getAttribute('data-sm-point-delete');
        if (deleteIndexRaw !== null) {
          const deleteIndex = Number(deleteIndexRaw);
          if (Number.isFinite(deleteIndex) && deleteIndex >= 0) {
            runtime.devPolygonDraft.points.splice(deleteIndex, 1);
            redrawDraft();
            syncCard();
          }
          return;
        }

        if (target.id === addBtnId) {
          const xInput = card.querySelector('#' + xInputId);
          const yInput = card.querySelector('#' + yInputId);
          const lng = Number(xInput ? xInput.value : NaN);
          const lat = Number(yInput ? yInput.value : NaN);
          if (!Number.isFinite(lng) || !Number.isFinite(lat)) return;
          runtime.devPolygonDraft.points.push({ lat, lng });
          if (xInput) xInput.value = '';
          if (yInput) yInput.value = '';
          redrawDraft();
          syncCard();
          return;
        }

        if (target.id === exportBtnId) {
          exportDraft();
          return;
        }

        if (target.id === collapseBtnId) {
          runtime.devPolygonDraft.collapsed = !runtime.devPolygonDraft.collapsed;
          syncCard();
          return;
        }

        if (target.id === clearBtnId) {
          runtime.devPolygonDraft.points = [];
          redrawDraft();
          syncCard();
        }
      });

      card.addEventListener('change', (event) => {
        const target = event.target;
        if (!target || !target.getAttribute) return;

        if (target.id === regionInputId) {
          runtime.devPolygonDraft.regionName = String(target.value || '').trim() || 'A区域';
          syncCard();
          return;
        }
        if (target.id === closedCheckboxId) {
          runtime.devPolygonDraft.closed = target.checked === true;
          redrawDraft();
          syncCard();
          return;
        }
        if (target.id === zMinInputId) {
          const value = Number(target.value);
          if (Number.isFinite(value)) runtime.devPolygonDraft.zRange.min = value;
          syncCard();
          return;
        }
        if (target.id === zMaxInputId) {
          const value = Number(target.value);
          if (Number.isFinite(value)) runtime.devPolygonDraft.zRange.max = value;
          syncCard();
          return;
        }

        const pointField = target.getAttribute('data-sm-point-field');
        const pointIndex = Number(target.getAttribute('data-sm-point-index'));
        if (!pointField || !Number.isFinite(pointIndex)) return;
        const point = runtime.devPolygonDraft.points[pointIndex];
        if (!point) return;
        const value = Number(target.value);
        if (!Number.isFinite(value)) return;
        if (pointField === 'lat' || pointField === 'lng') {
          point[pointField] = value;
          redrawDraft();
        }
      });
    };

    const mountCard = () => {
      let card = document.getElementById(cardId);
      if (!card) {
        card = document.createElement('section');
        card.id = cardId;
        card.className = 'spinning-momo-polygon-card';
        card.innerHTML =
          '<div class="spinning-momo-polygon-card-header">' +
          '<span>区域多边形采集（lat/lng）</span>' +
          '<button class="spinning-momo-polygon-mini-btn" id="' +
          collapseBtnId +
          '" type="button">收起</button>' +
          '</div>' +
          '<div id="' +
          bodyId +
          '">' +
          '<div class="spinning-momo-polygon-row">' +
          '<label>区域</label>' +
          '<input id="' +
          regionInputId +
          '" type="text" />' +
          '</div>' +
          '<div class="spinning-momo-polygon-row">' +
          '<label>x</label>' +
          '<input id="' +
          xInputId +
          '" type="number" step="any" placeholder="lng" />' +
          '<label>y</label>' +
          '<input id="' +
          yInputId +
          '" type="number" step="any" placeholder="lat" />' +
          '<button class="spinning-momo-polygon-btn" id="' +
          addBtnId +
          '" type="button">添加点</button>' +
          '</div>' +
          '<div class="spinning-momo-polygon-row">' +
          '<label>zMin</label>' +
          '<input id="' +
          zMinInputId +
          '" type="number" step="any" />' +
          '<label>zMax</label>' +
          '<input id="' +
          zMaxInputId +
          '" type="number" step="any" />' +
          '</div>' +
          '<div class="spinning-momo-polygon-row">' +
          '<label>闭合</label>' +
          '<input id="' +
          closedCheckboxId +
          '" type="checkbox" />' +
          '</div>' +
          '<div id="' +
          pointListId +
          '" class="spinning-momo-polygon-list"></div>' +
          '<div class="spinning-momo-polygon-btn-row">' +
          '<button class="spinning-momo-polygon-btn" id="' +
          exportBtnId +
          '" type="button">导出 JSON</button>' +
          '<button class="spinning-momo-polygon-btn" id="' +
          clearBtnId +
          '" type="button">清空点</button>' +
          '</div>' +
          '<div class="spinning-momo-polygon-footnote">输入 x=lng / y=lat；导出即地图坐标系。</div>' +
          '</div>';
      }
      if (card.parentElement !== document.body) {
        document.body.appendChild(card);
      }
      bindCardEvents(card);
      syncCard();
    };

    ensureStyles();
    ensureDraftState();
    redrawDraft();
    mountCard();
  };
`
}

import { computed } from 'vue'
import { useRoute, useRouter, type LocationQuery, type LocationQueryRaw } from 'vue-router'

export type GalleryOverlay = 'folder' | 'lightbox' | 'lightbox-details'

export interface GalleryOverlaySnapshot {
  overlay: GalleryOverlay | null
  assetId?: number
}

type GalleryHistoryParentOverlay = GalleryOverlay | null

const OVERLAY_QUERY_KEY = 'galleryOverlay'
const ASSET_QUERY_KEY = 'galleryAssetId'

function readQueryValue(query: LocationQuery, key: string): string | undefined {
  const value = query[key]
  if (Array.isArray(value)) {
    return value[0] ?? undefined
  }
  return value ?? undefined
}

function parseOverlay(query: LocationQuery): GalleryOverlay | null {
  const value = readQueryValue(query, OVERLAY_QUERY_KEY)
  if (value === 'folder' || value === 'lightbox' || value === 'lightbox-details') {
    return value
  }
  return null
}

function parseAssetId(query: LocationQuery): number | undefined {
  const value = readQueryValue(query, ASSET_QUERY_KEY)
  if (!value) {
    return undefined
  }

  const assetId = Number(value)
  return Number.isInteger(assetId) && assetId > 0 ? assetId : undefined
}

function snapshotFromQuery(query: LocationQuery): GalleryOverlaySnapshot {
  // 浏览器前进/后退只改变 URL；所有图库覆盖层都从这个快照恢复界面状态。
  const overlay = parseOverlay(query)
  return {
    overlay,
    ...(overlay === 'lightbox' || overlay === 'lightbox-details'
      ? { assetId: parseAssetId(query) }
      : {}),
  }
}

function isSameSnapshot(left: GalleryOverlaySnapshot, right: GalleryOverlaySnapshot): boolean {
  return left.overlay === right.overlay && left.assetId === right.assetId
}

export function isGalleryLightboxOverlay(overlay: GalleryOverlay | null | undefined): boolean {
  return overlay === 'lightbox' || overlay === 'lightbox-details'
}

// 统一管理图库抽屉和暗房的同页历史，保证手势返回、工具栏返回与页面按钮遵循同一层级。
export function useGalleryOverlayHistory() {
  const route = useRoute()
  const router = useRouter()
  const snapshot = computed(() => snapshotFromQuery(route.query))
  const hasOverlay = computed(() => snapshot.value.overlay !== null)

  function buildQuery(nextSnapshot: GalleryOverlaySnapshot): LocationQueryRaw {
    const query: LocationQueryRaw = { ...route.query }
    delete query[OVERLAY_QUERY_KEY]
    delete query[ASSET_QUERY_KEY]

    if (nextSnapshot.overlay !== null) {
      query[OVERLAY_QUERY_KEY] = nextSnapshot.overlay
    }
    if (nextSnapshot.assetId !== undefined) {
      query[ASSET_QUERY_KEY] = String(nextSnapshot.assetId)
    }

    return query
  }

  // 将覆盖层快照写回当前 gallery URL；打开建立历史层，修正状态时原地替换。
  function navigateToSnapshot(
    nextSnapshot: GalleryOverlaySnapshot,
    replace: boolean
  ): Promise<void> {
    if (isSameSnapshot(snapshot.value, nextSnapshot)) {
      return Promise.resolve()
    }

    const query = buildQuery(nextSnapshot)
    if (replace) {
      // replace 只修正当前状态，不新增可返回的层级；正常打开覆盖层必须走 push。
      return router.replace({ query }).then(() => undefined)
    }
    return router.push({ query }).then(() => undefined)
  }

  // 打开文件夹抽屉时新增一层历史，让系统返回手势可以只关闭抽屉。
  function openFolderDrawer() {
    return navigateToSnapshot({ overlay: 'folder' }, false)
  }

  // 从 Vue Router 当前条目读取 back 指向的父级，用来判断是否可以安全消费一层历史。
  function getHistoryParentSnapshot(): GalleryOverlaySnapshot | undefined {
    if (typeof window === 'undefined') {
      return undefined
    }

    const state = window.history.state as { back?: unknown } | null
    if (typeof state?.back !== 'string') {
      return undefined
    }

    try {
      const parent = router.resolve(state.back)
      return parent.name === 'gallery' ? snapshotFromQuery(parent.query) : undefined
    } catch {
      return undefined
    }
  }

  // 仅当父级确实是预期状态时回退，避免误退到图库之外的页面。
  function consumeHistoryEntry(expectedOverlay: GalleryHistoryParentOverlay): Promise<boolean> {
    if (getHistoryParentSnapshot()?.overlay !== expectedOverlay) {
      return Promise.resolve(false)
    }

    return new Promise((resolve) => {
      const removeAfterEach = router.afterEach(() => {
        removeAfterEach()
        resolve(true)
      })
      // 等待导航完成后再读取 snapshot，确保调用方看到的是已经消费后的界面状态。
      router.back()
    })
  }

  // 关闭文件夹抽屉时消费 folder -> gallery 的历史层，避免 replace 产生重复图库条目。
  async function closeFolderDrawer() {
    if (snapshot.value.overlay !== 'folder') {
      return
    }

    const consumed = await consumeHistoryEntry(null)
    if (!consumed || snapshot.value.overlay !== null) {
      // 直链打开或历史栈不匹配时没有可消费的父级，只能原地清除覆盖层参数。
      await navigateToSnapshot({ overlay: null }, true)
    }
  }

  // 打开暗房时记录当前资产，浏览器返回可以恢复到原图库位置。
  function openLightbox(assetId: number) {
    return navigateToSnapshot({ overlay: 'lightbox', assetId }, false)
  }

  // 暗房工具栏的返回语义是退出暗房；详情打开时先消费详情层，再消费暗房层。
  async function closeLightbox() {
    if (!isGalleryLightboxOverlay(snapshot.value.overlay)) {
      return
    }

    if (snapshot.value.overlay === 'lightbox-details') {
      // 先从详情退回暗房，保持 Escape/系统返回的层级语义一致。
      const detailsConsumed = await consumeHistoryEntry('lightbox')
      if (!detailsConsumed || snapshot.value.overlay !== 'lightbox') {
        // 详情是直链或父级异常时，直接回到图库，避免卡在无效覆盖层地址。
        await navigateToSnapshot({ overlay: null }, true)
        return
      }
    }

    // 消费 lightbox -> gallery 的历史层，交给界面 watcher 播放关闭动画。
    const lightboxConsumed = await consumeHistoryEntry(null)
    if (!lightboxConsumed || snapshot.value.overlay !== null) {
      // 没有可消费的父级时，用 replace 清除暗房地址作为安全兜底。
      await navigateToSnapshot({ overlay: null }, true)
    }
  }

  // 打开详情抽屉时新增一层历史，让返回手势只关闭详情而不退出暗房。
  function openLightboxDetails(assetId?: number) {
    if (!isGalleryLightboxOverlay(snapshot.value.overlay)) {
      return Promise.resolve()
    }

    return navigateToSnapshot(
      {
        overlay: 'lightbox-details',
        assetId: assetId ?? snapshot.value.assetId,
      },
      false
    )
  }

  // 关闭详情抽屉时消费 lightbox-details -> lightbox 的历史层。
  async function closeLightboxDetails() {
    if (snapshot.value.overlay !== 'lightbox-details') {
      return
    }

    const consumed = await consumeHistoryEntry('lightbox')
    if (!consumed || snapshot.value.overlay !== 'lightbox') {
      // 详情地址可能是用户直接打开的，此时原地恢复暗房状态即可。
      await navigateToSnapshot(
        {
          overlay: 'lightbox',
          assetId: snapshot.value.assetId,
        },
        true
      )
    }
  }

  // 切换暗房中的图片不新增历史，只更新当前暗房条目的资产身份。
  function replaceLightboxAsset(assetId: number) {
    if (!isGalleryLightboxOverlay(snapshot.value.overlay)) {
      return Promise.resolve()
    }

    return navigateToSnapshot(
      {
        overlay: snapshot.value.overlay,
        assetId,
      },
      true
    )
  }

  // 页面级返回只关闭当前最上层覆盖层，不越过仍然存在的父级状态。
  function closeTopOverlay() {
    switch (snapshot.value.overlay) {
      case 'folder':
        return closeFolderDrawer()
      case 'lightbox-details':
        return closeLightboxDetails()
      case 'lightbox':
        return closeLightbox()
      default:
        return Promise.resolve()
    }
  }

  return {
    snapshot,
    hasOverlay,
    openFolderDrawer,
    closeFolderDrawer,
    openLightbox,
    closeLightbox,
    openLightboxDetails,
    closeLightboxDetails,
    replaceLightboxAsset,
    closeTopOverlay,
  }
}

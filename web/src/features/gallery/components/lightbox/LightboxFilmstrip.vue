<script setup lang="ts">
import { ref, computed, watch, nextTick, onMounted } from 'vue'
import { useVirtualizer } from '@tanstack/vue-virtual'
import { Play } from 'lucide-vue-next'
import { galleryApi } from '../../api'
import { useGalleryData, useGallerySelection } from '../../composables'
import { useGalleryStore } from '../../store'
import type { ReviewFlag } from '../../types'
import ScrollArea from '@/components/ui/scroll-area/ScrollArea.vue'
import ScrollBar from '@/components/ui/scroll-area/ScrollBar.vue'
import GalleryAssetContextMenuContent from '../GalleryAssetContextMenuContent.vue'
import { ContextMenu, ContextMenuContent, ContextMenuTrigger } from '@/components/ui/context-menu'

const store = useGalleryStore()
const gallerySelection = useGallerySelection()
const galleryData = useGalleryData()

const scrollAreaRef = ref<InstanceType<typeof ScrollArea> | null>(null)
const filmstripRef = ref<HTMLElement | null>(null)
const loadingPages = ref<Set<number>>(new Set())

const THUMBNAIL_SIZE = 64
const THUMBNAIL_GAP = 12

const totalCount = computed(() => store.totalCount)
const currentIndex = computed(() => store.selection.activeIndex ?? 0)
const selectedIds = computed(() => store.selection.selectedIds)
const perPage = computed(() => store.perPage)

const virtualizer = useVirtualizer({
  get count() {
    return totalCount.value
  },
  getScrollElement: () => filmstripRef.value,
  estimateSize: () => THUMBNAIL_SIZE + THUMBNAIL_GAP,
  horizontal: true,
  scrollPaddingEnd: 24,
  scrollPaddingStart: 6,
  overscan: 5,
})

watch(scrollAreaRef, (newRef) => {
  if (newRef) {
    filmstripRef.value = newRef.viewportElement
  }
})

function getAssetAtIndex(index: number) {
  return store.getAssetsInRange(index, index)[0]
}

async function loadMissingData(
  items: ReturnType<typeof virtualizer.value.getVirtualItems>
): Promise<void> {
  if (items.length === 0) {
    return
  }

  const visibleIndexes = items.map((item) => item.index)
  const neededPages = new Set(visibleIndexes.map((idx) => Math.floor(idx / perPage.value) + 1))
  const loadPromises: Promise<void>[] = []

  neededPages.forEach((pageNum) => {
    if (!store.isPageLoaded(pageNum) && !loadingPages.value.has(pageNum)) {
      loadingPages.value.add(pageNum)
      loadPromises.push(
        galleryData.loadPage(pageNum).finally(() => {
          loadingPages.value.delete(pageNum)
        })
      )
    }
  })

  if (loadPromises.length > 0) {
    await Promise.all(loadPromises)
  }
}

watch(
  () => ({
    items: virtualizer.value.getVirtualItems(),
    totalCount: totalCount.value,
  }),
  async ({ items }) => {
    await loadMissingData(items)
  }
)

onMounted(() => {
  nextTick(() => {
    virtualizer.value.scrollToIndex(currentIndex.value, {
      align: 'center',
    })
  })
})

watch(currentIndex, (newIndex) => {
  nextTick(() => {
    virtualizer.value.scrollToIndex(newIndex, {
      align: 'auto',
    })
  })
})

function handleThumbnailClick(index: number, event: MouseEvent) {
  const asset = getAssetAtIndex(index)
  if (!asset) {
    return
  }

  void gallerySelection.handleAssetClick(asset, event, index)
}

function handleThumbnailContextMenu(index: number, event: MouseEvent) {
  const asset = getAssetAtIndex(index)
  if (!asset) {
    return
  }

  void gallerySelection.handleAssetContextMenu(asset, event, index)
}

function isSelected(index: number): boolean {
  const asset = getAssetAtIndex(index)
  return asset ? selectedIds.value.has(asset.id) : false
}

function getThumbnailUrl(index: number) {
  const asset = getAssetAtIndex(index)
  return asset ? galleryApi.getAssetThumbnailUrl(asset) : ''
}

function isVideoAsset(index: number): boolean {
  return getAssetAtIndex(index)?.type === 'video'
}

function getReviewFlagLabel(reviewFlag: ReviewFlag) {
  switch (reviewFlag) {
    case 'picked':
      return 'P'
    case 'rejected':
      return 'X'
    default:
      return ''
  }
}

function handleWheel(event: WheelEvent) {
  if (filmstripRef.value) {
    filmstripRef.value.scrollLeft += event.deltaY
  }
}
</script>

<template>
  <div class="surface-top flex flex-col border-t border-border">
    <ScrollArea ref="scrollAreaRef" class="filmstrip-scroller" @wheel.prevent="handleWheel">
      <div class="px-4 py-3">
        <div
          :style="{
            width: `${virtualizer.getTotalSize()}px`,
            position: 'relative',
          }"
        >
          <div
            v-for="item in virtualizer.getVirtualItems()"
            :key="item.index"
            :data-index="item.index"
            :style="{
              position: 'absolute',
              top: 0,
              left: 0,
              width: `${THUMBNAIL_SIZE}px`,
              height: `${THUMBNAIL_SIZE}px`,
              transform: `translateX(${item.start}px)`,
            }"
          >
            <ContextMenu v-if="getAssetAtIndex(item.index)">
              <ContextMenuTrigger as-child>
                <div
                  class="filmstrip-thumbnail group relative h-full w-full cursor-pointer overflow-hidden rounded transition-all duration-200 select-none"
                  :class="{
                    'scale-110 ring-2 ring-primary ring-offset-4': item.index === currentIndex,
                    'bg-foreground/20': isSelected(item.index),
                  }"
                  @click="handleThumbnailClick(item.index, $event)"
                  @contextmenu="handleThumbnailContextMenu(item.index, $event)"
                  @selectstart.prevent
                >
                  <img
                    :src="getThumbnailUrl(item.index)"
                    :alt="getAssetAtIndex(item.index)?.name"
                    class="h-full w-full object-contain object-center"
                    draggable="false"
                    @dragstart.prevent
                  />

                  <div
                    class="absolute inset-0 bg-transparent transition-all group-hover:bg-foreground/10"
                    :class="{
                      'bg-foreground/12': isSelected(item.index),
                      'bg-foreground/18': item.index === currentIndex,
                    }"
                  />

                  <div
                    v-if="isVideoAsset(item.index)"
                    class="absolute inset-x-0 bottom-0 flex items-end justify-start bg-gradient-to-t from-black/55 via-black/10 to-transparent p-1.5"
                  >
                    <div
                      class="flex h-5 w-5 items-center justify-center rounded-full border border-white/20 bg-black/60 text-white shadow-sm"
                    >
                      <Play class="ml-px h-2.5 w-2.5 fill-current" />
                    </div>
                  </div>

                  <div
                    v-if="(getAssetAtIndex(item.index)?.rating ?? 0) > 0"
                    class="absolute top-1 left-1 rounded bg-black/60 px-1.5 py-0.5 text-[10px] font-medium text-white"
                  >
                    {{ getAssetAtIndex(item.index)?.rating }}★
                  </div>

                  <div
                    v-if="getReviewFlagLabel(getAssetAtIndex(item.index)?.reviewFlag ?? 'none')"
                    class="absolute right-1 bottom-1 rounded px-1.5 py-0.5 text-[10px] font-semibold text-white"
                    :class="
                      getAssetAtIndex(item.index)?.reviewFlag === 'picked'
                        ? 'bg-emerald-600/85'
                        : 'bg-rose-600/85'
                    "
                  >
                    {{ getReviewFlagLabel(getAssetAtIndex(item.index)?.reviewFlag ?? 'none') }}
                  </div>

                  <div
                    v-if="isSelected(item.index)"
                    class="absolute top-1 right-1 rounded-full bg-primary p-0.5 text-primary-foreground"
                  >
                    <svg class="h-3 w-3" fill="currentColor" viewBox="0 0 20 20">
                      <path
                        fill-rule="evenodd"
                        d="M16.707 5.293a1 1 0 010 1.414l-8 8a1 1 0 01-1.414 0l-4-4a1 1 0 011.414-1.414L8 12.586l7.293-7.293a1 1 0 011.414 0z"
                        clip-rule="evenodd"
                      />
                    </svg>
                  </div>
                </div>
              </ContextMenuTrigger>
              <ContextMenuContent>
                <GalleryAssetContextMenuContent />
              </ContextMenuContent>
            </ContextMenu>

            <div v-else class="h-full w-full animate-pulse rounded bg-muted" />
          </div>
        </div>
      </div>
      <ScrollBar orientation="horizontal" />
    </ScrollArea>
  </div>
</template>

<style scoped>
.filmstrip-scroller {
  height: 100px;
}

.filmstrip-thumbnail {
  user-select: none;
  -webkit-user-select: none;
  transition:
    transform 0.2s ease,
    box-shadow 0.2s ease;
}
</style>

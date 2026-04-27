<script setup lang="ts">
import { computed } from 'vue'
import { useI18n } from '@/composables/useI18n'
import { useGalleryStore } from '../../store'
import { cn } from '@/lib/utils'
import {
  Flag,
  MoreHorizontal,
  Film,
  RotateCw,
  Minimize,
  Maximize,
  ZoomOut,
  ZoomIn,
} from 'lucide-vue-next'
import { Button } from '@/components/ui/button'
import { Popover, PopoverContent, PopoverTrigger } from '@/components/ui/popover'
import {
  DropdownMenu,
  DropdownMenuContent,
  DropdownMenuItem,
  DropdownMenuSeparator,
  DropdownMenuTrigger,
} from '@/components/ui/dropdown-menu'
import ReviewFilterPopover from '../tags/ReviewFilterPopover.vue'

const ACTUAL_SIZE_EPSILON = 0.001

const emit = defineEmits<{
  back: []
  fit: []
  actual: []
  zoomIn: []
  zoomOut: []
  rotate: [deltaDegrees: number]
  toggleFilmstrip: []
  toggleImmersive: []
}>()

const { t } = useI18n()
const store = useGalleryStore()

const currentIndex = computed(() => store.selection.activeIndex ?? 0)
const totalCount = computed(() => store.totalCount)
const selectedCount = computed(() => store.selection.selectedIds.size)
const showFilmstrip = computed(() => store.lightbox.showFilmstrip)
const isImmersive = computed(() => store.lightbox.isImmersive)
const currentAsset = computed(() => {
  const currentIndex = store.selection.activeIndex
  if (currentIndex === undefined) {
    return null
  }

  return store.getAssetsInRange(currentIndex, currentIndex)[0] ?? null
})
// 视频使用原生 controls，不适用灯箱图片的适屏/缩放语义。
const supportsZoom = computed(() => currentAsset.value?.type !== 'video')
const supportsRotate = computed(
  () => currentAsset.value !== null && currentAsset.value.type !== 'video'
)
const isFitMode = computed(() => store.lightbox.fitMode === 'contain')
const isActualSize = computed(
  () =>
    store.lightbox.fitMode === 'actual' && Math.abs(store.lightbox.zoom - 1) <= ACTUAL_SIZE_EPSILON
)
const lightboxMode = computed(() => {
  if (currentAsset.value?.type === 'video') {
    return t('gallery.toolbar.filter.type.video')
  }

  if (isFitMode.value) {
    return t('gallery.lightbox.toolbar.fit')
  }

  return `${Math.round(store.lightbox.zoom * 100)}%`
})

const hasReviewFilter = computed(
  () => store.filter.rating !== undefined || store.filter.reviewFlag !== undefined
)

const toggleActiveClass =
  'bg-sidebar-accent font-medium text-primary hover:text-primary [&_svg]:text-primary'

function handleRotateClick(event: MouseEvent) {
  emit('rotate', event.altKey ? -90 : 90)
}
</script>

<template>
  <div class="@container flex items-center justify-between px-2 py-2">
    <div class="flex min-w-0 items-center gap-3 text-foreground">
      <Button
        variant="sidebarGhost"
        size="icon-sm"
        class="shrink-0"
        @click="emit('back')"
        :title="t('gallery.lightbox.toolbar.backTitle')"
      >
        <svg class="size-4" fill="none" stroke="currentColor" viewBox="0 0 24 24">
          <path
            stroke-linecap="round"
            stroke-linejoin="round"
            stroke-width="2"
            d="M15 19l-7-7 7-7"
          />
        </svg>
      </Button>

      <div class="flex min-w-0 items-center gap-3">
        <span class="shrink-0 text-xs font-medium">{{ currentIndex + 1 }} / {{ totalCount }}</span>
        <span class="truncate text-xs text-muted-foreground">{{ lightboxMode }}</span>
        <span v-if="selectedCount > 0" class="shrink-0 text-xs text-primary">
          {{ t('gallery.lightbox.toolbar.selected') }} {{ selectedCount }}
        </span>
      </div>
    </div>

    <div class="flex items-center gap-2">
      <!-- 宽屏缩放控制 -->
      <div class="mr-2 hidden items-center gap-1 @[640px]:flex">
        <Button
          variant="sidebarGhost"
          class="h-8 px-2.5 text-xs"
          :disabled="!supportsZoom"
          :class="
            cn(
              !supportsZoom && 'cursor-not-allowed',
              supportsZoom && isFitMode && toggleActiveClass
            )
          "
          @click="emit('fit')"
          :title="t('gallery.lightbox.toolbar.fitTitle')"
        >
          {{ t('gallery.lightbox.toolbar.fit') }}
        </Button>

        <Button
          variant="sidebarGhost"
          class="h-8 px-2.5 text-xs"
          :disabled="!supportsZoom"
          :class="
            cn(
              !supportsZoom && 'cursor-not-allowed',
              supportsZoom && isActualSize && toggleActiveClass
            )
          "
          @click="emit('actual')"
          :title="t('gallery.lightbox.toolbar.actualTitle')"
        >
          100%
        </Button>

        <Button
          variant="sidebarGhost"
          size="icon-sm"
          :disabled="!supportsZoom"
          :class="!supportsZoom && 'cursor-not-allowed'"
          @click="emit('zoomOut')"
          :title="t('gallery.lightbox.toolbar.zoomOutTitle')"
        >
          <svg class="size-4" fill="none" stroke="currentColor" viewBox="0 0 24 24">
            <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M20 12H4" />
          </svg>
        </Button>

        <Button
          variant="sidebarGhost"
          size="icon-sm"
          :disabled="!supportsZoom"
          :class="!supportsZoom && 'cursor-not-allowed'"
          @click="emit('zoomIn')"
          :title="t('gallery.lightbox.toolbar.zoomInTitle')"
        >
          <svg class="size-4" fill="none" stroke="currentColor" viewBox="0 0 24 24">
            <path
              stroke-linecap="round"
              stroke-linejoin="round"
              stroke-width="2"
              d="M12 4v16m8-8H4"
            />
          </svg>
        </Button>
      </div>

      <Button
        v-if="supportsRotate"
        variant="sidebarGhost"
        size="icon-sm"
        class="mr-2 hidden @[640px]:inline-flex"
        @click="handleRotateClick"
        :title="t('gallery.lightbox.toolbar.rotateTitle')"
      >
        <RotateCw class="size-4" />
      </Button>

      <!-- 窄屏缩放控制 (折叠菜单) -->
      <DropdownMenu>
        <DropdownMenuTrigger as-child>
          <Button variant="sidebarGhost" size="icon-sm" class="mr-2 flex @[640px]:hidden">
            <MoreHorizontal class="size-4" />
          </Button>
        </DropdownMenuTrigger>
        <DropdownMenuContent align="end" class="w-40">
          <DropdownMenuItem
            :disabled="!supportsZoom"
            :class="supportsZoom && isFitMode ? 'font-medium text-primary focus:text-primary' : ''"
            @click="emit('fit')"
          >
            <Minimize class="mr-2 size-4" />
            {{ t('gallery.lightbox.toolbar.fit') }}
          </DropdownMenuItem>
          <DropdownMenuItem
            :disabled="!supportsZoom"
            :class="
              supportsZoom && isActualSize ? 'font-medium text-primary focus:text-primary' : ''
            "
            @click="emit('actual')"
          >
            <Maximize class="mr-2 size-4" />
            100%
          </DropdownMenuItem>
          <DropdownMenuSeparator />
          <DropdownMenuItem :disabled="!supportsZoom" @click="emit('zoomOut')">
            <ZoomOut class="mr-2 size-4" />
            {{ t('gallery.lightbox.toolbar.zoomOutTitle') }}
          </DropdownMenuItem>
          <DropdownMenuItem :disabled="!supportsZoom" @click="emit('zoomIn')">
            <ZoomIn class="mr-2 size-4" />
            {{ t('gallery.lightbox.toolbar.zoomInTitle') }}
          </DropdownMenuItem>
          <DropdownMenuSeparator v-if="supportsRotate" />
          <DropdownMenuItem v-if="supportsRotate" @click="emit('rotate', 90)">
            <RotateCw class="mr-2 size-4" />
            {{ t('gallery.lightbox.toolbar.rotateTitle') }}
          </DropdownMenuItem>
        </DropdownMenuContent>
      </DropdownMenu>

      <!-- 评分与标记筛选 -->
      <Popover>
        <PopoverTrigger as-child>
          <Button
            variant="sidebarGhost"
            size="icon-sm"
            :class="hasReviewFilter ? 'text-primary' : ''"
            :title="t('gallery.toolbar.filter.review.tooltip')"
          >
            <Flag class="size-4" />
          </Button>
        </PopoverTrigger>
        <PopoverContent align="end" class="w-56 p-3">
          <ReviewFilterPopover
            :rating="store.filter.rating"
            :review-flag="store.filter.reviewFlag"
            @update:rating="(v) => store.setFilter({ rating: v })"
            @update:review-flag="(v) => store.setFilter({ reviewFlag: v })"
          />
        </PopoverContent>
      </Popover>

      <Button
        variant="sidebarGhost"
        size="icon-sm"
        :class="showFilmstrip ? toggleActiveClass : ''"
        @click="emit('toggleFilmstrip')"
        :title="
          showFilmstrip
            ? t('gallery.lightbox.toolbar.filmstripHideTitle')
            : t('gallery.lightbox.toolbar.filmstripShowTitle')
        "
      >
        <Film class="size-4 rotate-90" />
      </Button>

      <Button
        variant="sidebarGhost"
        size="icon-sm"
        @click="emit('toggleImmersive')"
        :title="
          isImmersive
            ? t('gallery.lightbox.toolbar.exitImmersiveTitle')
            : t('gallery.lightbox.toolbar.immersiveTitle')
        "
      >
        <svg
          v-if="isImmersive"
          class="size-4"
          fill="none"
          stroke="currentColor"
          viewBox="0 0 24 24"
        >
          <path
            stroke-linecap="round"
            stroke-linejoin="round"
            stroke-width="2"
            d="M8 3H5a2 2 0 00-2 2v3m18 0V5a2 2 0 00-2-2h-3m0 18h3a2 2 0 002-2v-3M3 16v3a2 2 0 002 2h3m-1-7l4-4m0 0h-3m3 0v3m-8 1l4-4m0 0v3m0-3H8"
          />
        </svg>
        <svg v-else class="size-4" fill="none" stroke="currentColor" viewBox="0 0 24 24">
          <path
            stroke-linecap="round"
            stroke-linejoin="round"
            stroke-width="2"
            d="M4 8V4m0 0h4M4 4l5 5m11-1V4m0 0h-4m4 0l-5 5M4 16v4m0 0h4m-4 0l5-5m11 5l-5-5m5 5v-4m0 4h-4"
          />
        </svg>
      </Button>
    </div>
  </div>
</template>

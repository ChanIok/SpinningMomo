<script setup lang="ts">
import { computed } from 'vue'
import { useI18n } from '@/composables/useI18n'
import { useGalleryStore } from '../../store'
import { cn } from '@/lib/utils'
import {
  ChevronLeft,
  Flag,
  Film,
  RotateCw,
  Minimize,
  Maximize,
  ZoomOut,
  ZoomIn,
  Info,
} from '@lucide/vue'
import { Button } from '@/components/ui/button'
import { Popover, PopoverContent, PopoverTrigger } from '@/components/ui/popover'
import { Tooltip, TooltipContent, TooltipProvider, TooltipTrigger } from '@/components/ui/tooltip'
import { Kbd, KbdGroup } from '@/components/ui/kbd'
import ReviewFilterPopover from '../tags/ReviewFilterPopover.vue'

const ACTUAL_SIZE_EPSILON = 0.001

const props = defineProps<{
  compressed?: boolean
  detailsOpen: boolean
}>()

const emit = defineEmits<{
  back: []
  fit: []
  actual: []
  zoomIn: []
  zoomOut: []
  rotate: [deltaDegrees: number]
  toggleFilmstrip: []
  toggleImmersive: []
  toggleDetails: []
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
  () => (store.filter.ratings?.length ?? 0) > 0 || store.filter.reviewFlag !== undefined
)

const toggleActiveClass =
  'bg-sidebar-accent font-medium text-primary hover:text-primary [&_svg]:text-primary'

function handleRotateClick(event: MouseEvent) {
  emit('rotate', event.altKey ? -90 : 90)
}

function isEditableTarget(target: EventTarget | null): boolean {
  if (!(target instanceof HTMLElement)) {
    return false
  }

  return target.isContentEditable || ['INPUT', 'TEXTAREA', 'SELECT'].includes(target.tagName)
}

function handleToolbarContextMenu(event: MouseEvent) {
  if (isEditableTarget(event.target)) {
    return
  }

  event.preventDefault()
}
</script>

<template>
  <div
    class="@container flex w-full items-center justify-between px-2.5 text-foreground transition-colors"
    :class="
      props.compressed
        ? 'h-16 bg-gradient-to-b from-background/55 via-background/35 to-transparent pt-1 pb-4'
        : 'h-12 bg-transparent'
    "
    @contextmenu="handleToolbarContextMenu"
  >
    <TooltipProvider :delay-duration="300">
      <!-- 左侧区域 -->
      <div class="flex min-w-0 items-center gap-3">
        <Tooltip>
          <TooltipTrigger as-child>
            <Button
              variant="ghost"
              size="icon"
              class="h-10 w-10 shrink-0 text-foreground transition-colors hover:bg-black/10 active:bg-black/15 dark:hover:bg-white/10 dark:active:bg-white/15"
              @click="emit('back')"
            >
              <ChevronLeft class="size-5" :stroke-width="1.5" />
            </Button>
          </TooltipTrigger>
          <TooltipContent side="bottom" class="flex items-center gap-2">
            <span>{{ t('gallery.lightbox.toolbar.backTitle') }}</span>
            <Kbd>Esc</Kbd>
          </TooltipContent>
        </Tooltip>

        <!-- 宽屏模式下保留计数与模式信息 -->
        <div v-if="!props.compressed" class="flex min-w-0 items-center gap-3">
          <span class="shrink-0 text-xs font-medium"
            >{{ currentIndex + 1 }} / {{ totalCount }}</span
          >
          <span class="truncate text-xs text-muted-foreground">{{ lightboxMode }}</span>
          <span v-if="selectedCount > 0" class="shrink-0 text-xs text-primary">
            {{ t('gallery.lightbox.toolbar.selected') }} {{ selectedCount }}
          </span>
        </div>
      </div>

      <!-- 右侧控制区 -->
      <!-- 紧凑模式右侧：仅保留详情与沉浸/全屏按钮 -->
      <div v-if="props.compressed" class="flex items-center gap-1">
        <Tooltip>
          <TooltipTrigger as-child>
            <Button
              variant="ghost"
              size="icon"
              class="h-10 w-10 text-foreground transition-colors hover:bg-black/10 active:bg-black/15 dark:hover:bg-white/10 dark:active:bg-white/15"
              :class="props.detailsOpen ? 'text-primary hover:text-primary' : ''"
              @click="emit('toggleDetails')"
            >
              <Info class="size-5" :stroke-width="1.5" />
            </Button>
          </TooltipTrigger>
          <TooltipContent side="bottom">
            {{ t('gallery.lightbox.toolbar.detailsTitle') }}
          </TooltipContent>
        </Tooltip>

        <Tooltip>
          <TooltipTrigger as-child>
            <Button
              variant="ghost"
              size="icon"
              class="h-10 w-10 text-foreground transition-colors hover:bg-black/10 active:bg-black/15 dark:hover:bg-white/10 dark:active:bg-white/15"
              @click="emit('toggleImmersive')"
            >
              <Minimize v-if="isImmersive" class="size-5" :stroke-width="1.5" />
              <Maximize v-else class="size-5" :stroke-width="1.5" />
            </Button>
          </TooltipTrigger>
          <TooltipContent side="bottom" class="flex items-center gap-2">
            <span>
              {{
                isImmersive
                  ? t('gallery.lightbox.toolbar.exitImmersiveTitle')
                  : t('gallery.lightbox.toolbar.immersiveTitle')
              }}
            </span>
            <Kbd>F</Kbd>
          </TooltipContent>
        </Tooltip>
      </div>

      <!-- 宽屏模式右侧：保留缩放、旋转、筛选、胶片栏与全屏控件 -->
      <div v-else class="flex items-center gap-2">
        <div class="mr-2 flex items-center gap-1">
          <Tooltip>
            <TooltipTrigger as-child>
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
              >
                {{ t('gallery.lightbox.toolbar.fit') }}
              </Button>
            </TooltipTrigger>
            <TooltipContent side="bottom" class="flex items-center gap-2">
              <span>{{ t('gallery.lightbox.toolbar.fitTitle') }}</span>
              <Kbd>Z</Kbd>
            </TooltipContent>
          </Tooltip>

          <Tooltip>
            <TooltipTrigger as-child>
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
              >
                {{ t('gallery.lightbox.toolbar.actual') }}
              </Button>
            </TooltipTrigger>
            <TooltipContent side="bottom" class="flex items-center gap-2">
              <span>{{ t('gallery.lightbox.toolbar.actualTitle') }}</span>
              <Kbd>Z</Kbd>
            </TooltipContent>
          </Tooltip>

          <Tooltip>
            <TooltipTrigger as-child>
              <Button
                variant="sidebarGhost"
                size="icon-sm"
                :disabled="!supportsZoom"
                :class="!supportsZoom && 'cursor-not-allowed'"
                @click="emit('zoomOut')"
              >
                <ZoomOut class="size-4" />
              </Button>
            </TooltipTrigger>
            <TooltipContent side="bottom" class="flex items-center gap-2">
              <span>{{ t('gallery.lightbox.toolbar.zoomOutTitle') }}</span>
              <Kbd>-</Kbd>
            </TooltipContent>
          </Tooltip>

          <Tooltip>
            <TooltipTrigger as-child>
              <Button
                variant="sidebarGhost"
                size="icon-sm"
                :disabled="!supportsZoom"
                :class="!supportsZoom && 'cursor-not-allowed'"
                @click="emit('zoomIn')"
              >
                <ZoomIn class="size-4" />
              </Button>
            </TooltipTrigger>
            <TooltipContent side="bottom" class="flex items-center gap-2">
              <span>{{ t('gallery.lightbox.toolbar.zoomInTitle') }}</span>
              <Kbd>+</Kbd>
            </TooltipContent>
          </Tooltip>
        </div>

        <Tooltip v-if="supportsRotate">
          <TooltipTrigger as-child>
            <Button variant="sidebarGhost" size="icon-sm" class="mr-2" @click="handleRotateClick">
              <RotateCw class="size-4" />
            </Button>
          </TooltipTrigger>
          <TooltipContent side="bottom" class="flex items-center gap-2">
            <span>{{ t('gallery.lightbox.toolbar.rotateTitle') }}</span>
            <KbdGroup>
              <Kbd>Alt</Kbd>
              <span class="text-[10px] opacity-60">+</span>
              <Kbd>Click</Kbd>
            </KbdGroup>
          </TooltipContent>
        </Tooltip>

        <!-- 评分与标记筛选 -->
        <Tooltip>
          <TooltipTrigger as-child>
            <span class="inline-flex">
              <Popover>
                <PopoverTrigger as-child>
                  <Button
                    variant="sidebarGhost"
                    size="icon-sm"
                    :class="hasReviewFilter ? 'text-primary' : ''"
                  >
                    <Flag class="size-4" />
                  </Button>
                </PopoverTrigger>
                <PopoverContent align="end" class="w-56 p-3">
                  <ReviewFilterPopover
                    :ratings="store.filter.ratings"
                    :review-flag="store.filter.reviewFlag"
                    @update:ratings="(v) => store.setFilter({ ratings: v })"
                    @update:review-flag="(v) => store.setFilter({ reviewFlag: v })"
                  />
                </PopoverContent>
              </Popover>
            </span>
          </TooltipTrigger>
          <TooltipContent side="bottom">
            {{ t('gallery.toolbar.filter.review.tooltip') }}
          </TooltipContent>
        </Tooltip>

        <Tooltip>
          <TooltipTrigger as-child>
            <Button
              variant="sidebarGhost"
              size="icon-sm"
              :class="showFilmstrip ? toggleActiveClass : ''"
              @click="emit('toggleFilmstrip')"
            >
              <Film class="size-4 rotate-90" />
            </Button>
          </TooltipTrigger>
          <TooltipContent side="bottom" class="flex items-center gap-2">
            <span>
              {{
                showFilmstrip
                  ? t('gallery.lightbox.toolbar.filmstripHideTitle')
                  : t('gallery.lightbox.toolbar.filmstripShowTitle')
              }}
            </span>
            <Kbd>Tab</Kbd>
          </TooltipContent>
        </Tooltip>

        <Tooltip>
          <TooltipTrigger as-child>
            <Button
              variant="ghost"
              size="icon"
              class="h-10 w-10 text-foreground transition-colors hover:bg-black/10 active:bg-black/15 dark:hover:bg-white/10 dark:active:bg-white/15"
              @click="emit('toggleImmersive')"
            >
              <Minimize v-if="isImmersive" class="size-5" :stroke-width="1.5" />
              <Maximize v-else class="size-5" :stroke-width="1.5" />
            </Button>
          </TooltipTrigger>
          <TooltipContent side="bottom" class="flex items-center gap-2">
            <span>
              {{
                isImmersive
                  ? t('gallery.lightbox.toolbar.exitImmersiveTitle')
                  : t('gallery.lightbox.toolbar.immersiveTitle')
              }}
            </span>
            <Kbd>F</Kbd>
          </TooltipContent>
        </Tooltip>
      </div>
    </TooltipProvider>
  </div>
</template>

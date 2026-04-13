<script setup lang="ts">
import { computed, ref, watch } from 'vue'
import { Play } from 'lucide-vue-next'
import { formatFileSize } from '@/lib/utils'
import { useGalleryData } from '../../composables/useGalleryData'
import type { Asset } from '../../types'

interface AssetListRowProps {
  asset: Asset
  isSelected?: boolean
  rowHeight: number
  thumbnailSize: number
  columnsTemplate: string
}

const props = withDefaults(defineProps<AssetListRowProps>(), {
  isSelected: false,
})

const emit = defineEmits<{
  click: [asset: Asset, event: MouseEvent]
  'double-click': [asset: Asset, event: MouseEvent]
  'context-menu': [asset: Asset, event: MouseEvent]
  'drag-start': [asset: Asset, event: DragEvent]
}>()

const { getAssetThumbnailUrl } = useGalleryData()

const imageError = ref(false)

const thumbnailUrl = computed(() => getAssetThumbnailUrl(props.asset))
const hasThumbnail = computed(() => thumbnailUrl.value.length > 0)
const isVideoAsset = computed(() => props.asset.type === 'video')
// 文件类型标签：优先显示扩展名，其次 MIME 子类型，最后回退到 type 字段
const fileTypeLabel = computed(() => {
  const extension = props.asset.extension?.replace(/^\./, '').trim().toLowerCase()
  if (extension) {
    return extension
  }

  const mimeSubtype = props.asset.mimeType?.split('/')[1]?.trim().toLowerCase()
  if (mimeSubtype) {
    return mimeSubtype
  }

  return props.asset.type
})
const resolutionLabel = computed(() => {
  if (!props.asset.width || !props.asset.height) {
    return '-'
  }

  return `${props.asset.width}×${props.asset.height}`
})
const fileSizeLabel = computed(() => formatFileSize(props.asset.size))

// thumbnailUrl 变化时（如切换资产）重置图片加载错误状态
watch(
  thumbnailUrl,
  () => {
    imageError.value = false
  },
  { immediate: true }
)

function onImageError() {
  imageError.value = true
}

function handleClick(event: MouseEvent) {
  emit('click', props.asset, event)
}

function handleDoubleClick(event: MouseEvent) {
  emit('double-click', props.asset, event)
}

function handleContextMenu(event: MouseEvent) {
  emit('context-menu', props.asset, event)
}

function handleDragStart(event: DragEvent) {
  emit('drag-start', props.asset, event)
}
</script>

<template>
  <div
    data-asset-list-row
    draggable="true"
    class="group grid w-full items-center gap-3 rounded-sm px-3 transition-colors select-none"
    :class="
      props.isSelected ? 'bg-primary text-primary-foreground' : 'text-foreground hover:bg-muted/55'
    "
    :style="{
      gridTemplateColumns: props.columnsTemplate,
      height: `${props.rowHeight}px`,
    }"
    @click="handleClick"
    @dblclick="handleDoubleClick"
    @contextmenu="handleContextMenu"
    @dragstart="handleDragStart"
  >
    <div class="flex items-center justify-center">
      <div
        data-asset-thumbnail
        class="relative overflow-hidden rounded-sm border border-border/50 bg-muted"
        :style="{
          width: `${props.thumbnailSize}px`,
          height: `${props.thumbnailSize}px`,
          backgroundColor: props.asset.dominantColorHex || undefined,
        }"
      >
        <img
          v-if="hasThumbnail && !imageError"
          :src="thumbnailUrl"
          :alt="asset.name"
          class="h-full w-full object-cover"
          @error="onImageError"
        />
        <div v-else class="absolute inset-0 bg-white/20 dark:bg-black/20" />
        <div
          v-if="isVideoAsset"
          class="absolute right-1 bottom-1 flex h-4 w-4 items-center justify-center rounded-full bg-black/60 text-white"
        >
          <Play class="ml-[1px] h-2.5 w-2.5 fill-current" />
        </div>
      </div>
    </div>

    <div class="truncate text-sm" :title="asset.name">
      {{ asset.name }}
    </div>
    <div class="truncate text-sm opacity-85">
      {{ fileTypeLabel }}
    </div>
    <div class="truncate text-sm opacity-85">
      {{ resolutionLabel }}
    </div>
    <div class="truncate text-right text-sm opacity-85">
      {{ fileSizeLabel }}
    </div>
  </div>
</template>

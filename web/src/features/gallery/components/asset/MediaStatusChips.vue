<script setup lang="ts">
/**
 * 媒体资产状态角标组件 (MediaStatusChips)
 *
 * 作用：在图库列表卡片或底栏缩略图（Filmstrip）的顶部/底部浮层覆盖层中，
 * 展示星级评分、染色码标识、标签（Tag）以及弃用标记（Rejected Status）等状态角标。
 */
import { computed } from 'vue'
import { Paintbrush, Star, X } from 'lucide-vue-next'
import { useI18n } from '@/composables/useI18n'
import type { ReviewFlag, Tag } from '../../types'

/** 组件 Props 属性定义 */
interface MediaStatusChipsProps {
  /** 星级评分（0 为未评分） */
  rating?: number
  /** 审核/标记状态（如 rejected 等） */
  reviewFlag?: ReviewFlag
  /** 是否为紧凑模式（例如缩略图/底栏小卡片） */
  compact?: boolean
  /** 是否显示星级角标 */
  showRating?: boolean
  /** 是否包含染色码数据 */
  hasDyeCode?: boolean
  /** 是否在卡片上渲染标签列表 */
  showTags?: boolean
  /** 媒体关联的标签列表 */
  tags?: Tag[]
}

// 卡片模式下最多渲染的标签数量上限
const MAX_RENDERED_CARD_TAGS = 12

const props = withDefaults(defineProps<MediaStatusChipsProps>(), {
  rating: 0,
  reviewFlag: 'none',
  compact: false,
  showRating: true,
  hasDyeCode: false,
  showTags: false,
  tags: () => [],
})

const { t } = useI18n()

// 计算属性：判定各类角标与标签的展示条件
const hasRating = computed(() => props.showRating && (props.rating ?? 0) > 0)
const isRejected = computed(() => props.reviewFlag === 'rejected')

// 仅在非紧凑模式且开启 showTags 时，限制最多展示 MAX_RENDERED_CARD_TAGS 个标签
const renderedTags = computed(() =>
  props.showTags && !props.compact ? props.tags.slice(0, MAX_RENDERED_CARD_TAGS) : []
)
const hasMoreTags = computed(
  () => props.showTags && !props.compact && props.tags.length > MAX_RENDERED_CARD_TAGS
)

// 悬浮提示文本：拼合所有标签名称
const tagTooltip = computed(() => props.tags.map((tag) => tag.name).join(' · '))

// 状态标记（星级或染色码）是否存在
const hasStatusMarkers = computed(() => hasRating.value || props.hasDyeCode)

// 是否存在顶部展示内容（状态标记或标签）
const hasTopContent = computed(() => hasStatusMarkers.value || renderedTags.value.length > 0)
</script>

<template>
  <!-- 浮层容器：设置 pointer-events-none 避免阻挡下方卡片点击事件 -->
  <div class="pointer-events-none absolute inset-0">
    <!-- 顶部状态与标签区域 -->
    <div
      v-if="hasTopContent"
      class="absolute flex flex-col items-start gap-1 overflow-hidden"
      :class="compact ? 'top-1 left-1' : 'top-2 right-2 bottom-10 left-2'"
    >
      <!-- 星级与染色码角标行 -->
      <div v-if="hasStatusMarkers" class="flex shrink-0 items-start gap-1">
        <!-- 星级评分角标 -->
        <div
          v-if="hasRating"
          class="flex items-center gap-1 rounded-md border border-white/15 bg-black/45 text-white"
          :class="compact ? 'px-1.5 py-0.5 text-[10px]' : 'px-2 py-1 text-[11px]'"
        >
          <Star
            class="shrink-0 fill-current text-current"
            :class="compact ? 'h-2.5 w-2.5' : 'h-3 w-3'"
          />
          <span class="font-medium">{{ rating }}</span>
        </div>
        <!-- 染色码标识角标 -->
        <div
          v-if="hasDyeCode"
          :title="t('gallery.preferences.badges.dyeCodeTooltip')"
          class="flex items-center justify-center rounded-md border border-white/15 bg-black/45 text-white"
          :class="compact ? 'h-4 w-4' : 'h-6 w-6'"
        >
          <Paintbrush :class="compact ? 'h-2.5 w-2.5' : 'h-3.5 w-3.5'" />
        </div>
      </div>

      <!-- 标签展示行 -->
      <div
        v-if="renderedTags.length > 0"
        :title="tagTooltip"
        class="flex min-h-0 w-full flex-wrap content-start gap-1 overflow-hidden"
      >
        <div
          v-for="tag in renderedTags"
          :key="tag.id"
          class="max-w-full min-w-0 rounded border border-white/15 bg-black/45 px-1.5 py-0.5 text-[10px] leading-3 text-white shadow-sm backdrop-blur-[1px]"
        >
          <span class="block truncate font-medium">{{ tag.name }}</span>
        </div>
        <!-- 超出数量上限时的省略指示 -->
        <div
          v-if="hasMoreTags"
          class="rounded border border-white/15 bg-black/45 px-1.5 py-0.5 text-[10px] leading-3 text-white shadow-sm backdrop-blur-[1px]"
        >
          …
        </div>
      </div>
    </div>

    <!-- 紧凑模式（如底栏 Filmstrip）弃用标记：右下角小红 X 方框 -->
    <div
      v-if="isRejected && compact"
      :title="t('gallery.review.flag.rejected')"
      class="absolute right-1 bottom-1 flex h-4 w-4 items-center justify-center rounded-sm border border-white/20 bg-black/50 text-current shadow-sm backdrop-blur-sm"
    >
      <X class="h-3 w-3 stroke-[3] text-rose-400" />
    </div>

    <!-- 主图库卡片弃用标记：右下角胶囊角标（红 X + 文字） -->
    <div
      v-if="isRejected && !compact"
      :title="t('gallery.review.flag.rejected')"
      class="absolute right-2 bottom-2 flex items-center gap-1 rounded-md border border-white/15 bg-black/50 px-2 py-1 text-[11px] text-white transition-opacity duration-150"
    >
      <X class="h-3.5 w-3.5 shrink-0 text-rose-400" />
      <span class="font-medium">{{ t('gallery.review.flag.rejected') }}</span>
    </div>
  </div>
</template>

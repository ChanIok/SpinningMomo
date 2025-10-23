<script setup lang="ts">
import { ref, computed } from 'vue'
import { Button } from '@/components/ui/button'
import type { TagTreeNode } from '../types'

interface Props {
  tags: TagTreeNode[]
  selectedTagIds?: number[]
}

const props = withDefaults(defineProps<Props>(), {
  selectedTagIds: () => [],
})

const emit = defineEmits<{
  select: [tagId: number]
}>()

// 标签展开状态
const expandedTagIds = ref<Set<number>>(new Set())

// 当前选中的标签 ID 集合
const selectedIds = computed(() => new Set(props.selectedTagIds))

// 切换展开状态
function toggleExpand(tagId: number) {
  if (expandedTagIds.value.has(tagId)) {
    expandedTagIds.value.delete(tagId)
  } else {
    expandedTagIds.value.add(tagId)
  }
}

// 选择标签
function handleSelectTag(tagId: number) {
  emit('select', tagId)
}

// 递归渲染标签树项
function renderTagItem(tag: TagTreeNode, depth = 0) {
  return {
    tag,
    depth,
    hasChildren: tag.children && tag.children.length > 0,
    isExpanded: expandedTagIds.value.has(tag.id),
    isSelected: selectedIds.value.has(tag.id),
  }
}
</script>

<template>
  <div class="w-64 max-h-96 overflow-y-auto p-2">
    <!-- 标题 -->
    <div class="mb-2 px-2 text-xs font-medium text-muted-foreground">选择标签</div>

    <!-- 标签列表 -->
    <div v-if="tags.length > 0" class="space-y-0.5">
      <template v-for="tag in tags" :key="tag.id">
        <!-- 标签项 -->
        <div>
          <Button
            variant="ghost"
            size="sm"
            :class="[
              'h-8 w-full justify-start gap-2 px-2 text-sm',
              renderTagItem(tag).isSelected && 'bg-accent',
            ]"
            @click="handleSelectTag(tag.id)"
          >
            <!-- 展开箭头 -->
            <span
              v-if="renderTagItem(tag).hasChildren"
              class="flex h-4 w-4 flex-shrink-0 items-center justify-center"
              @click.stop="toggleExpand(tag.id)"
            >
              <svg
                xmlns="http://www.w3.org/2000/svg"
                width="12"
                height="12"
                viewBox="0 0 24 24"
                fill="none"
                stroke="currentColor"
                stroke-width="2"
                stroke-linecap="round"
                stroke-linejoin="round"
                class="transition-transform"
                :class="{ 'rotate-90': renderTagItem(tag).isExpanded }"
              >
                <path d="m9 18 6-6-6-6" />
              </svg>
            </span>
            <span v-else class="w-4 flex-shrink-0" />

            <!-- 标签图标 -->
            <svg
              xmlns="http://www.w3.org/2000/svg"
              width="14"
              height="14"
              viewBox="0 0 24 24"
              fill="none"
              stroke="currentColor"
              stroke-width="2"
              stroke-linecap="round"
              stroke-linejoin="round"
              class="flex-shrink-0"
            >
              <path
                d="M12 2H2v10l9.29 9.29c.94.94 2.48.94 3.42 0l6.58-6.58c.94-.94.94-2.48 0-3.42L12 2Z"
              />
              <path d="M7 7h.01" />
            </svg>

            <!-- 标签名称 -->
            <span class="flex-1 truncate text-left">{{ tag.name }}</span>

            <!-- 选中标记 -->
            <svg
              v-if="renderTagItem(tag).isSelected"
              xmlns="http://www.w3.org/2000/svg"
              width="14"
              height="14"
              viewBox="0 0 24 24"
              fill="none"
              stroke="currentColor"
              stroke-width="2"
              stroke-linecap="round"
              stroke-linejoin="round"
              class="flex-shrink-0 text-primary"
            >
              <path d="M20 6 9 17l-5-5" />
            </svg>
          </Button>

          <!-- 递归渲染子标签 -->
          <div
            v-if="renderTagItem(tag).hasChildren && renderTagItem(tag).isExpanded"
            class="ml-4 space-y-0.5"
          >
            <template v-for="child in tag.children" :key="child.id">
              <div>
                <Button
                  variant="ghost"
                  size="sm"
                  :class="[
                    'h-8 w-full justify-start gap-2 px-2 text-sm',
                    selectedIds.has(child.id) && 'bg-accent',
                  ]"
                  @click="handleSelectTag(child.id)"
                >
                  <!-- 展开箭头（子标签） -->
                  <span
                    v-if="child.children && child.children.length > 0"
                    class="flex h-4 w-4 flex-shrink-0 items-center justify-center"
                    @click.stop="toggleExpand(child.id)"
                  >
                    <svg
                      xmlns="http://www.w3.org/2000/svg"
                      width="12"
                      height="12"
                      viewBox="0 0 24 24"
                      fill="none"
                      stroke="currentColor"
                      stroke-width="2"
                      stroke-linecap="round"
                      stroke-linejoin="round"
                      class="transition-transform"
                      :class="{ 'rotate-90': expandedTagIds.has(child.id) }"
                    >
                      <path d="m9 18 6-6-6-6" />
                    </svg>
                  </span>
                  <span v-else class="w-4 flex-shrink-0" />

                  <!-- 标签图标 -->
                  <svg
                    xmlns="http://www.w3.org/2000/svg"
                    width="14"
                    height="14"
                    viewBox="0 0 24 24"
                    fill="none"
                    stroke="currentColor"
                    stroke-width="2"
                    stroke-linecap="round"
                    stroke-linejoin="round"
                    class="flex-shrink-0"
                  >
                    <path
                      d="M12 2H2v10l9.29 9.29c.94.94 2.48.94 3.42 0l6.58-6.58c.94-.94.94-2.48 0-3.42L12 2Z"
                    />
                    <path d="M7 7h.01" />
                  </svg>

                  <!-- 标签名称 -->
                  <span class="flex-1 truncate text-left">{{ child.name }}</span>

                  <!-- 选中标记 -->
                  <svg
                    v-if="selectedIds.has(child.id)"
                    xmlns="http://www.w3.org/2000/svg"
                    width="14"
                    height="14"
                    viewBox="0 0 24 24"
                    fill="none"
                    stroke="currentColor"
                    stroke-width="2"
                    stroke-linecap="round"
                    stroke-linejoin="round"
                    class="flex-shrink-0 text-primary"
                  >
                    <path d="M20 6 9 17l-5-5" />
                  </svg>
                </Button>

                <!-- 更深层级的递归（支持三级及以上） -->
                <div
                  v-if="
                    child.children &&
                    child.children.length > 0 &&
                    expandedTagIds.has(child.id)
                  "
                  class="ml-4 space-y-0.5"
                >
                  <Button
                    v-for="grandChild in child.children"
                    :key="grandChild.id"
                    variant="ghost"
                    size="sm"
                    :class="[
                      'h-8 w-full justify-start gap-2 px-2 text-sm',
                      selectedIds.has(grandChild.id) && 'bg-accent',
                    ]"
                    @click="handleSelectTag(grandChild.id)"
                  >
                    <span class="w-4 flex-shrink-0" />
                    <svg
                      xmlns="http://www.w3.org/2000/svg"
                      width="14"
                      height="14"
                      viewBox="0 0 24 24"
                      fill="none"
                      stroke="currentColor"
                      stroke-width="2"
                      stroke-linecap="round"
                      stroke-linejoin="round"
                      class="flex-shrink-0"
                    >
                      <path
                        d="M12 2H2v10l9.29 9.29c.94.94 2.48.94 3.42 0l6.58-6.58c.94-.94.94-2.48 0-3.42L12 2Z"
                      />
                      <path d="M7 7h.01" />
                    </svg>
                    <span class="flex-1 truncate text-left">{{ grandChild.name }}</span>
                    <svg
                      v-if="selectedIds.has(grandChild.id)"
                      xmlns="http://www.w3.org/2000/svg"
                      width="14"
                      height="14"
                      viewBox="0 0 24 24"
                      fill="none"
                      stroke="currentColor"
                      stroke-width="2"
                      stroke-linecap="round"
                      stroke-linejoin="round"
                      class="flex-shrink-0 text-primary"
                    >
                      <path d="M20 6 9 17l-5-5" />
                    </svg>
                  </Button>
                </div>
              </div>
            </template>
          </div>
        </div>
      </template>
    </div>

    <!-- 空状态 -->
    <div v-else class="py-8 text-center text-sm text-muted-foreground">暂无标签</div>
  </div>
</template>

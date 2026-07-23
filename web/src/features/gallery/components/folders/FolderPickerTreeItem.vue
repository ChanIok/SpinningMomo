<script setup lang="ts">
import { computed } from 'vue'
import { ChevronRight, Cloud, Folder } from 'lucide-vue-next'
import { cn } from '@/lib/utils'
import type { FolderTreeNode } from '../../types'

const props = withDefaults(
  defineProps<{
    folder: FolderTreeNode
    depth?: number
    selectedFolderId: number | null
    expandedIds: Set<number>
  }>(),
  {
    depth: 0,
  }
)

const emit = defineEmits<{
  select: [folderId: number]
  toggleExpand: [folderId: number]
}>()

const hasChildren = computed(() => (props.folder.children?.length ?? 0) > 0)
const isExpanded = computed(() => props.expandedIds.has(props.folder.id))
const isSelected = computed(() => props.selectedFolderId === props.folder.id)
const label = computed(() => props.folder.displayName || props.folder.name)

function handleSelect() {
  emit('select', props.folder.id)
}

function handleToggleExpand(event: MouseEvent) {
  event.stopPropagation()
  emit('toggleExpand', props.folder.id)
}
</script>

<template>
  <li>
    <button
      type="button"
      :class="
        cn(
          'flex h-8 w-full items-center gap-1 rounded-md px-1 text-left text-sm transition-colors outline-none',
          'hover:bg-accent hover:text-accent-foreground',
          'focus-visible:ring-2 focus-visible:ring-ring',
          isSelected && 'bg-accent text-accent-foreground'
        )
      "
      :style="{ paddingLeft: `${depth * 12 + 4}px` }"
      @click="handleSelect"
    >
      <span
        class="flex h-6 w-6 shrink-0 items-center justify-center rounded-sm"
        :class="hasChildren ? 'hover:bg-muted' : 'opacity-0'"
        @click="hasChildren ? handleToggleExpand($event) : undefined"
      >
        <ChevronRight
          class="h-3.5 w-3.5 transition-transform"
          :class="isExpanded ? 'rotate-90' : ''"
        />
      </span>
      <Cloud v-if="folder.isNetwork" class="h-3.5 w-3.5 shrink-0 text-muted-foreground" />
      <Folder v-else class="h-3.5 w-3.5 shrink-0 text-muted-foreground" />
      <span class="truncate pl-1">{{ label }}</span>
    </button>

    <ul v-if="hasChildren && isExpanded" class="space-y-0.5">
      <FolderPickerTreeItem
        v-for="child in folder.children"
        :key="child.id"
        :folder="child"
        :depth="depth + 1"
        :selected-folder-id="selectedFolderId"
        :expanded-ids="expandedIds"
        @select="emit('select', $event)"
        @toggle-expand="emit('toggleExpand', $event)"
      />
    </ul>
  </li>
</template>

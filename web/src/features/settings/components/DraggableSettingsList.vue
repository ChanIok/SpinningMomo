<script setup lang="ts">
import { ref, onUnmounted, nextTick } from 'vue'
import { Switch } from '@/components/ui/switch'
import { Label } from '@/components/ui/label'
import { Button } from '@/components/ui/button'
import { Input } from '@/components/ui/input'
import { GripVertical, Plus, Trash2 } from 'lucide-vue-next'
import { useI18n } from '@/composables/useI18n'
import type { MenuItem } from '../types'

const props = defineProps<{
  items: MenuItem[]
  title: string
  description: string

  // Optional features
  allowAdd?: boolean
  allowRemove?: boolean
  showToggle?: boolean // 是否显示启用/禁用切换开关

  // Customization
  addPlaceholder?: string
  validateInput?: (value: string) => boolean
  getLabel?: (id: string) => string
}>()

const emit = defineEmits<{
  (e: 'reorder', items: MenuItem[]): void
  (e: 'toggle', id: string, enabled: boolean): void
  (e: 'add', item: { id: string; enabled: boolean }): void
  (e: 'remove', id: string): void
}>()

const { t } = useI18n()
const isAdding = ref(false)
const newItemValue = ref('')

// Resolve label
const getItemLabel = (id: string) => {
  return props.getLabel ? props.getLabel(id) : id
}

// --- Drag & Drop State ---
const draggingId = ref<string | null>(null)
const dragStartY = ref(0)
const mouseOffsetInItem = ref(0) // Mouse position relative to item top
const itemsContainer = ref<HTMLElement | null>(null)
const sourceEl = ref<HTMLElement | null>(null)
const ghostEl = ref<HTMLElement | null>(null)
const cachedRects = ref<Map<string, DOMRect>>(new Map())
const rafId = ref<number | null>(null)

// --- Helper Actions ---

const handleAddItem = () => {
  if (!props.allowAdd) return
  const value = newItemValue.value.trim()
  if (!value) return
  if (props.items.some((item) => item.id === value)) return // Exists
  if (props.validateInput && !props.validateInput(value)) return // Invalid

  emit('add', { id: value, enabled: true })
  newItemValue.value = ''
  isAdding.value = false
}

const handleKeyDown = (e: KeyboardEvent) => {
  if (e.key === 'Enter') handleAddItem()
  if (e.key === 'Escape') {
    isAdding.value = false
    newItemValue.value = ''
  }
}

const handleToggle = (id: string, enabled: boolean) => {
  if (draggingId.value) return // Prevent toggle while dragging
  emit('toggle', id, enabled)
}

// --- Ghost Element Functions ---

const createGhost = (source: HTMLElement, rect: DOMRect, initialTop: number): HTMLElement => {
  const ghost = source.cloneNode(true) as HTMLElement
  ghost.classList.add('draggable-ghost')

  // Remove Tailwind classes that would override our styles
  ghost.classList.remove('surface-top', 'hover:bg-accent/50')

  ghost.style.cssText = `
        position: fixed;
        left: ${rect.left}px;
        top: ${initialTop}px;
        width: ${rect.width}px;
        height: ${rect.height}px;
        margin: 0;
        pointer-events: none;
        z-index: 9999;
        will-change: top;
    `
  document.body.appendChild(ghost)
  return ghost
}

const removeGhost = () => {
  if (ghostEl.value) {
    ghostEl.value.remove()
    ghostEl.value = null
  }
}

const cacheItemRects = () => {
  if (!itemsContainer.value) return
  cachedRects.value.clear()
  const itemElements = Array.from(
    itemsContainer.value.querySelectorAll('.draggable-item')
  ) as HTMLElement[]
  itemElements.forEach((el, index) => {
    const itemId = props.items[index]?.id
    if (itemId) {
      cachedRects.value.set(itemId, el.getBoundingClientRect())
    }
  })
}

// --- Drag & Drop Logic ---

const handlePointerDown = (_id: string, event: PointerEvent) => {
  if (event.button !== 0) return
  const target = event.currentTarget as HTMLElement
  if (!target) return

  const itemEl = target.closest('.draggable-item') as HTMLElement
  if (!itemEl) return

  // Immediately prevent text selection
  document.body.style.userSelect = 'none'
  document.body.style.cursor = 'grabbing'

  sourceEl.value = itemEl
  dragStartY.value = event.clientY

  // Record mouse offset within the item (for ghost positioning)
  const rect = itemEl.getBoundingClientRect()
  mouseOffsetInItem.value = event.clientY - rect.top

  // Cache all item rects for efficient swap detection
  cacheItemRects()

  window.addEventListener('pointermove', handlePointerMove)
  window.addEventListener('pointerup', handlePointerUp)
  window.addEventListener('pointercancel', handlePointerUp)
}

const handlePointerMove = (event: PointerEvent) => {
  const currentY = event.clientY
  const deltaY = currentY - dragStartY.value

  // Start dragging after 3px threshold
  if (!draggingId.value && sourceEl.value && Math.abs(deltaY) > 3) {
    // Find the item id from the source element
    const itemIndex = Array.from(
      itemsContainer.value?.querySelectorAll('.draggable-item') || []
    ).indexOf(sourceEl.value)
    if (itemIndex >= 0 && props.items[itemIndex]) {
      draggingId.value = props.items[itemIndex].id

      // Create ghost element at current mouse position
      const rect = sourceEl.value.getBoundingClientRect()
      const ghostTop = currentY - mouseOffsetInItem.value
      ghostEl.value = createGhost(sourceEl.value, rect, ghostTop)
    }
  }

  // Update ghost position directly based on mouse Y (no transform offset needed)
  if (draggingId.value && ghostEl.value) {
    if (rafId.value) cancelAnimationFrame(rafId.value)
    rafId.value = requestAnimationFrame(() => {
      if (ghostEl.value) {
        // Ghost follows mouse directly
        ghostEl.value.style.top = `${currentY - mouseOffsetInItem.value}px`
      }
    })

    // Check for swap
    checkSwap(currentY)
  }
}

const checkSwap = (cursorY: number) => {
  if (!draggingId.value || !itemsContainer.value) return

  const currentIndex = props.items.findIndex((item) => item.id === draggingId.value)
  if (currentIndex === -1) return

  // Check swap with previous item
  if (currentIndex > 0) {
    const prevItem = props.items[currentIndex - 1]
    if (!prevItem) return
    const prevRect = cachedRects.value.get(prevItem.id)
    if (prevRect) {
      const prevCenter = prevRect.top + prevRect.height / 2
      if (cursorY < prevCenter) {
        performSwap(currentIndex, currentIndex - 1, -prevRect.height)
        return
      }
    }
  }

  // Check swap with next item
  if (currentIndex < props.items.length - 1) {
    const nextItem = props.items[currentIndex + 1]
    if (!nextItem) return
    const nextRect = cachedRects.value.get(nextItem.id)
    if (nextRect) {
      const nextCenter = nextRect.top + nextRect.height / 2
      if (cursorY > nextCenter) {
        performSwap(currentIndex, currentIndex + 1, nextRect.height)
        return
      }
    }
  }
}

const performSwap = (fromIndex: number, toIndex: number, _offsetAdjust: number) => {
  const newItems = [...props.items]
  const moved = newItems.splice(fromIndex, 1)[0]
  if (moved) {
    newItems.splice(toIndex, 0, moved)
    // 不再需要设置 order，数组顺序即为显示顺序
    emit('reorder', newItems)

    // Re-cache rects after DOM update
    nextTick(() => cacheItemRects())
  }
}

const handlePointerUp = () => {
  // Cancel any pending animation frame
  if (rafId.value) {
    cancelAnimationFrame(rafId.value)
    rafId.value = null
  }

  // Clean up ghost
  removeGhost()

  // Reset state
  draggingId.value = null
  sourceEl.value = null
  mouseOffsetInItem.value = 0
  cachedRects.value.clear()

  // Restore body styles
  document.body.style.userSelect = ''
  document.body.style.cursor = ''

  window.removeEventListener('pointermove', handlePointerMove)
  window.removeEventListener('pointerup', handlePointerUp)
  window.removeEventListener('pointercancel', handlePointerUp)
}

onUnmounted(() => {
  if (rafId.value) cancelAnimationFrame(rafId.value)
  removeGhost()
  document.body.style.userSelect = ''
  document.body.style.cursor = ''
  window.removeEventListener('pointermove', handlePointerMove)
  window.removeEventListener('pointerup', handlePointerUp)
  window.removeEventListener('pointercancel', handlePointerUp)
})
</script>

<template>
  <div class="space-y-3">
    <div>
      <h3 class="text-base font-semibold text-foreground">{{ title }}</h3>
      <p class="mt-1 text-sm text-muted-foreground">{{ description }}</p>
    </div>

    <div class="surface-top rounded-md p-3">
      <div ref="itemsContainer" class="relative flex flex-col gap-1">
        <TransitionGroup name="list">
          <div
            v-for="item in items"
            :key="item.id"
            class="draggable-item surface-top group flex cursor-grab items-center justify-between rounded-md p-2.5 transition-colors hover:bg-accent/50 active:cursor-grabbing"
            :class="{
              'is-dragging-source': draggingId === item.id,
            }"
            @pointerdown="handlePointerDown(item.id, $event)"
          >
            <div class="flex flex-1 items-center gap-3 pr-4">
              <div
                class="text-muted-foreground/60 transition-colors group-hover:text-muted-foreground"
              >
                <GripVertical class="h-4 w-4" />
              </div>
              <Label class="pointer-events-none text-sm font-medium text-foreground select-none">
                {{ getItemLabel(item.id) }}
              </Label>
            </div>
            <div class="flex flex-shrink-0 items-center gap-2">
              <Switch
                v-if="showToggle"
                :model-value="item.enabled"
                @update:model-value="(v: boolean) => handleToggle(item.id, v)"
                @click.stop
                @pointerdown.stop
              />
              <Button
                v-if="allowRemove"
                variant="ghost"
                size="icon"
                class="h-6 w-6 text-destructive hover:bg-destructive/10 hover:text-destructive"
                @click="emit('remove', item.id)"
                @pointerdown.stop
              >
                <Trash2 class="size-4" />
              </Button>
            </div>
          </div>
        </TransitionGroup>
      </div>

      <!-- Add Item Section (only if allowAdd is true) -->
      <div v-if="allowAdd" class="mt-3 pt-3">
        <div
          v-if="isAdding"
          class="flex items-center gap-2 rounded-md border border-primary bg-primary/5 p-3"
        >
          <Input
            v-model="newItemValue"
            @keydown="handleKeyDown"
            :placeholder="addPlaceholder"
            class="flex-1"
            autofocus
          />
          <Button size="sm" @click="handleAddItem">
            {{ t('settings.menu.actions.add') }}
          </Button>
          <Button
            variant="ghost"
            size="sm"
            @click="
              () => {
                isAdding = false
                newItemValue = ''
              }
            "
          >
            {{ t('settings.menu.actions.cancel') }}
          </Button>
        </div>
        <Button
          v-else
          variant="outline"
          size="sm"
          class="w-full border-dashed hover:border-primary hover:text-primary"
          @click="isAdding = true"
        >
          <Plus class="mr-2 h-4 w-4" />
          {{ t('settings.menu.actions.addCustomItem') }}
        </Button>
      </div>

      <div
        v-if="items.length === 0 && !isAdding"
        class="flex items-center justify-center py-8 text-center text-sm text-muted-foreground"
      >
        {{ t('settings.menu.status.noPresetItems') }}
      </div>
    </div>
  </div>
</template>

<style scoped>
/* Smooth transition for items when reordering */
.list-move {
  transition: transform 0.2s ease;
}

/* Hide the source element while dragging (ghost shows instead) */
.draggable-item.is-dragging-source {
  opacity: 0 !important;
  pointer-events: none;
}

/* Prevent text selection on all draggable items */
.draggable-item {
  user-select: none;
}

/* Ghost element styles (applied via JS, but kept here for reference) */
:global(.draggable-ghost) {
  /* Use color-mix to handle transparency for oklch variables */
  background: color-mix(in srgb, var(--accent), transparent 50%);
  border: 1px solid color-mix(in srgb, var(--primary), transparent 80%);
  border-radius: 0.375rem;
  opacity: 0.9;
}
</style>

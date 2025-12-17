
<script setup lang="ts">
import { ref, onUnmounted } from 'vue'
import { Switch } from '@/components/ui/switch'
import { Label } from '@/components/ui/label'
import { GripVertical } from 'lucide-vue-next'
import { useI18n } from '@/composables/useI18n'
import type { FeatureItem } from '../types'

const props = defineProps<{
  items: FeatureItem[]
  title: string
  description: string
}>()

const emit = defineEmits<{
  (e: 'reorder', items: FeatureItem[]): void
  (e: 'toggle', id: string, enabled: boolean): void
}>()

const { t } = useI18n()

// I18n helper
const getFeatureItemLabel = (id: string): string => {
  const labelMap: Record<string, string> = {
    'screenshot.capture': t('settings.menu.items.screenshotCapture'),
    'screenshot.open_folder': t('settings.menu.items.screenshotOpenFolder'),
    'feature.toggle_preview': t('settings.menu.items.featureTogglePreview'),
    'feature.toggle_overlay': t('settings.menu.items.featureToggleOverlay'),
    'feature.toggle_letterbox': t('settings.menu.items.featureToggleLetterbox'),
    'window.reset_transform': t('settings.menu.items.windowResetTransform'),
    'panel.hide': t('settings.menu.items.panelHide'),
    'app.exit': t('settings.menu.items.appExit'),
  }
  return labelMap[id] || id
}

// --- Drag & Drop State ---
const draggingId = ref<string | null>(null)
const pressedId = ref<string | null>(null)
const pressedEl = ref<HTMLElement | null>(null)
const dragOffset = ref(0)
const dragStartY = ref(0)
const itemsContainer = ref<HTMLElement | null>(null)

// --- Drag & Drop Logic ---
const handlePointerDown = (id: string, event: PointerEvent) => {
    if (event.button !== 0) return
    const target = event.currentTarget as HTMLElement
    // Only allow drag from handle
    if (!target) return

    pressedId.value = id
    pressedEl.value = target.closest('.draggable-item') as HTMLElement // Find the row element
    dragStartY.value = event.clientY
    dragOffset.value = 0

    window.addEventListener('pointermove', handlePointerMove)
    window.addEventListener('pointerup', handlePointerUp)
    window.addEventListener('pointercancel', handlePointerUp)
}

const handlePointerMove = (event: PointerEvent) => {
    if (draggingId.value) {
        // Dragging logic
        const currentY = event.clientY
        const deltaY = currentY - dragStartY.value
        dragOffset.value = deltaY

        // Threshold verify
        if (Math.abs(deltaY) > 5) {
             checkSwap(currentY)
        }
        return
    }

    // Pending drag logic
    if (pressedId.value) {
        const currentY = event.clientY
        const moveDist = Math.abs(currentY - dragStartY.value)
        if (moveDist > 5) {
            draggingId.value = pressedId.value
            if (pressedEl.value) {
                try {
                    pressedEl.value.setPointerCapture(event.pointerId)
                } catch (e) { /* ignore */ }
            }
            dragOffset.value = currentY - dragStartY.value
        }
    }
}

const checkSwap = (cursorY: number) => {
    if (!draggingId.value || !itemsContainer.value) return
    
    // Find current index based on ID from props.items
    const currentIndex = props.items.findIndex(item => item.id === draggingId.value)
    if (currentIndex === -1) return

    const itemElements = Array.from(itemsContainer.value.children) as HTMLElement[]
    
    // Check Previous
    if (currentIndex > 0) {
        const prevEl = itemElements[currentIndex - 1]
        if (prevEl) {
             const prevRect = prevEl.getBoundingClientRect()
             const prevCenter = prevRect.y + prevRect.height / 2
             
             if (cursorY < prevCenter) {
                 // Swap Data
                 const newItems = [...props.items]
                 // Reorder: remove current, insert at index-1
                 const moved = newItems.splice(currentIndex, 1)[0]
                 if (moved) {
                    newItems.splice(currentIndex - 1, 0, moved)
                 
                    // Update Order props to keep consistent (optional if backend ignores order field but frontend uses array index)
                    newItems.forEach((item, index) => item.order = index + 1)
                 
                    emit('reorder', newItems)
                 }
                 
                 // Compensate view
                 dragStartY.value -= prevRect.height
                 dragOffset.value = cursorY - dragStartY.value
                 return
             }
        }
    }

    // Check Next
    if (currentIndex < props.items.length - 1) {
        const nextEl = itemElements[currentIndex + 1]
        if (nextEl) {
            const nextRect = nextEl.getBoundingClientRect()
            const nextCenter = nextRect.y + nextRect.height / 2
            
            if (cursorY > nextCenter) {
                const newItems = [...props.items]
                const moved = newItems.splice(currentIndex, 1)[0]
                if (moved) {
                    newItems.splice(currentIndex + 1, 0, moved)
                
                    newItems.forEach((item, index) => item.order = index + 1)
                
                    emit('reorder', newItems)
                }
                
                dragStartY.value += nextRect.height
                dragOffset.value = cursorY - dragStartY.value
            }
        }
    }
}

const handlePointerUp = () => {
    draggingId.value = null
    pressedId.value = null
    pressedEl.value = null
    dragOffset.value = 0
    
    window.removeEventListener('pointermove', handlePointerMove)
    window.removeEventListener('pointerup', handlePointerUp)
    window.removeEventListener('pointercancel', handlePointerUp)
}

onUnmounted(() => {
    window.removeEventListener('pointermove', handlePointerMove)
    window.removeEventListener('pointerup', handlePointerUp)
    window.removeEventListener('pointercancel', handlePointerUp)
})

const handleToggle = (id: string, enabled: boolean) => {
    if (draggingId.value) return // Prevent toggle while dragging
    emit('toggle', id, enabled)
}
</script>

<template>
  <div class="space-y-4">
    <div>
      <h3 class="text-lg font-semibold text-foreground">{{ title }}</h3>
      <p class="mt-1 text-sm text-muted-foreground">{{ description }}</p>
    </div>

    <div class="rounded-lg border bg-card p-4 text-card-foreground shadow-sm">
        <div ref="itemsContainer" class="flex flex-col gap-1 relative"> 
            <TransitionGroup name="list">
                <div
                    v-for="item in items"
                    :key="item.id"
                    class="draggable-item flex items-center justify-between rounded-md p-3 transition-colors hover:bg-accent/50 group border border-transparent hover:border-primary/20 bg-card"
                    :style="{
                        transform: draggingId === item.id ? `translateY(${dragOffset}px)` : '',
                        zIndex: draggingId === item.id ? 50 : 'auto',
                        position: 'relative',
                    }"
                    :class="{
                        'cursor-grabbing': draggingId === item.id,
                        'select-none': draggingId === item.id
                    }"
                >
                    <div class="flex-1 pr-4 flex items-center gap-3">
                         <div
                            class="cursor-grab hover:text-foreground text-muted-foreground active:cursor-grabbing p-1 -ml-1" 
                            @pointerdown="handlePointerDown(item.id, $event)"
                         >
                            <GripVertical class="h-4 w-4" />
                         </div>
                         <Label class="text-sm font-medium text-foreground cursor-default pointer-events-none">
                            {{ getFeatureItemLabel(item.id) }}
                         </Label>
                    </div>
                    <div class="flex flex-shrink-0 items-center gap-2">
                        <Switch
                            :checked="item.enabled"
                            @update:checked="(v: boolean) => handleToggle(item.id, v)"
                            @click.stop
                        />
                         <span class="min-w-[3rem] text-xs text-muted-foreground text-right">
                            {{ item.enabled ? t('settings.menu.status.visible') : t('settings.menu.status.hidden') }}
                         </span>
                    </div>
                </div>
            </TransitionGroup>
         </div>
         <div v-if="items.length === 0" class="flex items-center justify-center py-8 text-center text-muted-foreground text-sm">
             {{ t('settings.menu.status.noFeatureItems') }}
         </div>
    </div>
  </div>
</template>

<style scoped>
.list-move {
  transition: transform 0.2s ease;
}

/* Ensure dragging item stays on top and has no transition delay for instant follow */
.draggable-item.cursor-grabbing {
    transition: none;
    box-shadow: 0 4px 12px rgba(0,0,0,0.1); /* Optional: add shadow while dragging */
}
</style>

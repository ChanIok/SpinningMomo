
<script setup lang="ts">
import { ref, onUnmounted } from 'vue'
import { Switch } from '@/components/ui/switch'
import { Label } from '@/components/ui/label'
import { Button } from '@/components/ui/button'
import { Input } from '@/components/ui/input'
import { GripVertical, Plus, Trash2 } from 'lucide-vue-next'
import { useI18n } from '@/composables/useI18n'
import type { PresetItem } from '../types'

const props = defineProps<{
  items: PresetItem[]
  title: string
  description: string
  addPlaceholder: string
  validateCustom?: (value: string) => boolean
}>()

const emit = defineEmits<{
  (e: 'reorder', items: PresetItem[]): void
  (e: 'toggle', id: string, enabled: boolean): void
  (e: 'add', item: Omit<PresetItem, 'order'>): void
  (e: 'remove', id: string): void
}>()

const { t } = useI18n()
const isAdding = ref(false)
const newItemValue = ref('')

// --- Drag & Drop State ---
const draggingId = ref<string | null>(null)
const pressedId = ref<string | null>(null)
const pressedEl = ref<HTMLElement | null>(null)
const dragOffset = ref(0)
const dragStartY = ref(0)
const itemsContainer = ref<HTMLElement | null>(null)

// --- Helper Actions ---

const handleAddItem = () => {
    const value = newItemValue.value.trim()
    if (!value) return
    if (props.items.some(item => item.id === value)) return // Exists
    if (props.validateCustom && !props.validateCustom(value)) return // Invalid

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

// --- Drag & Drop Logic ---

const handlePointerDown = (id: string, event: PointerEvent) => {
    if (event.button !== 0) return
    const target = event.currentTarget as HTMLElement
    if (!target) return

    pressedId.value = id
    pressedEl.value = target.closest('.draggable-item') as HTMLElement
    dragStartY.value = event.clientY
    dragOffset.value = 0

    window.addEventListener('pointermove', handlePointerMove)
    window.addEventListener('pointerup', handlePointerUp)
    window.addEventListener('pointercancel', handlePointerUp)
}

const handlePointerMove = (event: PointerEvent) => {
     if (draggingId.value) {
        const currentY = event.clientY
        const deltaY = currentY - dragStartY.value
        dragOffset.value = deltaY

        if (Math.abs(deltaY) > 5) {
             checkSwap(currentY)
        }
        return
     }

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
    
    const currentIndex = props.items.findIndex(item => item.id === draggingId.value)
    if (currentIndex === -1) return

    const itemElements = Array.from(itemsContainer.value.children) as HTMLElement[]

    if (currentIndex > 0) {
        const prevEl = itemElements[currentIndex - 1]
        if (prevEl) {
             const prevRect = prevEl.getBoundingClientRect()
             const prevCenter = prevRect.y + prevRect.height / 2
             if (cursorY < prevCenter) {
                 const newItems = [...props.items]
                 const moved = newItems.splice(currentIndex, 1)[0]
                 if (moved) {
                    newItems.splice(currentIndex - 1, 0, moved)
                    newItems.forEach((item, index) => item.order = index + 1)
                    emit('reorder', newItems)
                 }
                 dragStartY.value -= prevRect.height
                 dragOffset.value = cursorY - dragStartY.value
                 return
             }
        }
    }

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
                            {{ item.id }}
                         </Label>
                    </div>
                    <div class="flex flex-shrink-0 items-center gap-2">
                        <Switch
                            :checked="item.enabled"
                            @update:checked="(v: boolean) => emit('toggle', item.id, v)"
                            @click.stop
                        />
                         <span class="min-w-[3rem] text-xs text-muted-foreground text-right">
                            {{ item.enabled ? t('settings.menu.status.visible') : t('settings.menu.status.hidden') }}
                         </span>
                         <Button 
                            variant="ghost" 
                            size="icon" 
                            class="h-8 w-8 text-destructive hover:text-destructive hover:bg-destructive/10" 
                            @click="emit('remove', item.id)"
                         >
                            <Trash2 class="h-4 w-4" />
                         </Button>
                    </div>
                </div>
            </TransitionGroup>
         </div>

         <!-- Add Item Section -->
        <div class="mt-4 pt-4 border-t">
           <div v-if="isAdding" class="flex items-center gap-2 rounded-md border border-primary bg-primary/5 p-3">
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
               <Button variant="ghost" size="sm" @click="() => { isAdding = false; newItemValue = '' }">
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

         <div v-if="items.length === 0 && !isAdding" class="flex items-center justify-center py-8 text-center text-muted-foreground text-sm">
            {{ t('settings.menu.status.noPresetItems') }}
        </div>
    </div>
  </div>
</template>

<style scoped>
.list-move {
  transition: transform 0.2s ease;
}

.draggable-item.cursor-grabbing {
    transition: none;
    box-shadow: 0 4px 12px rgba(0,0,0,0.1);
}
</style>

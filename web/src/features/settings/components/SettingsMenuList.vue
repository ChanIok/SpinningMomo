<script setup lang="ts">
import { useRouter } from 'vue-router'
import { ChevronRight } from '@lucide/vue'
import { ScrollArea } from '@/components/ui/scroll-area'
import { useI18n } from '@/composables/useI18n'
import { pushWithViewTransition } from '@/router/viewTransition'
import { SETTINGS_MENUS, type SettingsMenuItem } from '../menu'

const router = useRouter()
const { t } = useI18n()

const handleItemClick = (item: SettingsMenuItem) => {
  void pushWithViewTransition(router, {
    name: 'settings',
    params: { section: item.key },
  })
}
</script>

<template>
  <div class="h-full w-full overflow-hidden text-foreground">
    <ScrollArea class="h-full w-full">
      <div class="mx-auto w-full max-w-lg px-4 py-3">
        <div class="surface-top overflow-hidden rounded-md">
          <button
            v-for="item in SETTINGS_MENUS"
            :key="item.key"
            type="button"
            class="flex w-full items-center justify-between gap-3 px-4 py-3.5 text-left transition-colors duration-150 hover:bg-accent/40 focus-visible:ring-2 focus-visible:ring-ring focus-visible:outline-none active:bg-accent/70"
            @click="handleItemClick(item)"
          >
            <div class="flex min-w-0 items-center gap-3.5">
              <div
                class="flex h-8 w-8 shrink-0 items-center justify-center rounded-lg bg-primary/10 text-primary"
              >
                <component :is="item.icon" class="h-4.5 w-4.5" stroke-width="2" />
              </div>
              <span class="truncate text-sm font-medium text-foreground">
                {{ t(item.label) }}
              </span>
            </div>
            <ChevronRight class="h-4 w-4 shrink-0 text-muted-foreground/60" stroke-width="1.8" />
          </button>
        </div>
      </div>
    </ScrollArea>
  </div>
</template>

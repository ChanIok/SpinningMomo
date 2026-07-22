<script setup lang="ts">
import { cn } from '@/lib/utils'
import {
  Settings,
  Keyboard,
  Camera,
  Blocks,
  Monitor,
  Menu,
  Palette,
  DatabaseBackup,
} from 'lucide-vue-next'
import { ScrollArea } from '@/components/ui/scroll-area'
import { useI18n } from '@/composables/useI18n'

export type SettingsPageKey =
  | 'general'
  | 'hotkeys'
  | 'capture'
  | 'extensions'
  | 'windowScene'
  | 'floatingWindow'
  | 'webAppearance'
  | 'backup'

interface SettingsMenuItem {
  key: SettingsPageKey
  label: string
  icon: any
}

defineProps<{
  activePage: SettingsPageKey
}>()

const emit = defineEmits<{
  (e: 'update:activePage', page: SettingsPageKey): void
}>()

const { t } = useI18n()

const settingsMenus: SettingsMenuItem[] = [
  {
    key: 'general',
    label: 'settings.layout.general.title',
    icon: Settings,
  },
  {
    key: 'hotkeys',
    label: 'settings.layout.hotkeys.title',
    icon: Keyboard,
  },
  {
    key: 'capture',
    label: 'settings.layout.capture.title',
    icon: Camera,
  },
  {
    key: 'windowScene',
    label: 'settings.layout.windowScene.title',
    icon: Monitor,
  },
  {
    key: 'floatingWindow',
    label: 'settings.layout.floatingWindow.title',
    icon: Menu,
  },
  {
    key: 'webAppearance',
    label: 'settings.layout.webAppearance.title',
    icon: Palette,
  },
  {
    key: 'extensions',
    label: 'settings.layout.extensions.title',
    icon: Blocks,
  },
  {
    key: 'backup',
    label: 'settings.layout.backup.title',
    icon: DatabaseBackup,
  },
]

const handleMenuClick = (key: SettingsPageKey) => {
  emit('update:activePage', key)
}
</script>

<template>
  <div class="flex h-full w-48 flex-col border-r border-border/40 lg:w-56 2xl:w-64">
    <ScrollArea class="h-full w-full flex-1">
      <div class="p-4 pr-3">
        <nav class="flex-1">
          <div class="space-y-1">
            <div v-for="item in settingsMenus" :key="item.key" class="group">
              <button
                @click="handleMenuClick(item.key)"
                :class="
                  cn(
                    'flex w-full items-center space-x-3 rounded-md px-4 py-2.5 transition-colors duration-200 ease-out',
                    'text-left focus-visible:ring-2 focus-visible:ring-sidebar-ring focus-visible:ring-offset-2 focus-visible:outline-none',
                    activePage === item.key
                      ? 'bg-sidebar-accent font-medium text-primary hover:text-primary [&>svg]:text-primary'
                      : 'text-sidebar-foreground hover:bg-sidebar-hover hover:text-sidebar-accent-foreground'
                  )
                "
              >
                <component
                  :is="item.icon"
                  class="h-5 w-5 flex-shrink-0 transition-colors duration-200 ease-out"
                  stroke-width="1.8"
                />
                <div class="min-w-0 flex-1">
                  <div class="text-sm font-medium">{{ t(item.label) }}</div>
                </div>
              </button>
            </div>
          </div>
        </nav>
      </div>
    </ScrollArea>
  </div>
</template>

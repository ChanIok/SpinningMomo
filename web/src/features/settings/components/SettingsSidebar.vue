<script setup lang="ts">
import { cn } from '@/lib/utils'
import { Settings, Keyboard, Camera, Monitor, Menu, Palette } from 'lucide-vue-next'
import { useI18n } from '@/composables/useI18n'

export type SettingsPageKey =
  | 'general'
  | 'hotkeys'
  | 'capture'
  | 'windowScene'
  | 'floatingWindow'
  | 'webAppearance'

interface SettingsMenuItem {
  key: SettingsPageKey
  label: string
  icon: any
  description: string
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
    description: 'settings.layout.general.description',
  },
  {
    key: 'hotkeys',
    label: 'settings.layout.hotkeys.title',
    icon: Keyboard,
    description: 'settings.layout.hotkeys.description',
  },
  {
    key: 'capture',
    label: 'settings.layout.capture.title',
    icon: Camera,
    description: 'settings.layout.capture.description',
  },
  {
    key: 'windowScene',
    label: 'settings.layout.windowScene.title',
    icon: Monitor,
    description: 'settings.layout.windowScene.description',
  },
  {
    key: 'floatingWindow',
    label: 'settings.layout.floatingWindow.title',
    icon: Menu,
    description: 'settings.layout.floatingWindow.description',
  },
  {
    key: 'webAppearance',
    label: 'settings.layout.webAppearance.title',
    icon: Palette,
    description: 'settings.layout.webAppearance.description',
  },
]

const handleMenuClick = (key: SettingsPageKey) => {
  emit('update:activePage', key)
}
</script>

<template>
  <div class="flex h-full w-48 flex-col lg:w-56 2xl:w-64">
    <div class="h-full p-4">
      <nav class="flex-1">
        <div class="space-y-1">
          <div v-for="item in settingsMenus" :key="item.key" class="group">
            <button
              @click="handleMenuClick(item.key)"
              :class="
                cn(
                  'flex w-full items-center space-x-3 rounded-md px-4 py-3 transition-all duration-200',
                  'text-left focus-visible:ring-2 focus-visible:ring-sidebar-ring focus-visible:ring-offset-2 focus-visible:outline-none',
                  'hover:bg-accent hover:text-accent-foreground',
                  activePage === item.key
                    ? 'bg-accent font-medium text-accent-foreground'
                    : 'text-muted-foreground'
                )
              "
              :title="t(item.description)"
            >
              <component :is="item.icon" class="h-5 w-5 flex-shrink-0" stroke-width="1.8" />
              <div class="min-w-0 flex-1">
                <div class="text-sm font-medium">{{ t(item.label) }}</div>
              </div>
            </button>
          </div>
        </div>
      </nav>
    </div>
  </div>
</template>

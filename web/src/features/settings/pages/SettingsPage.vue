<script setup lang="ts">
import { ref, watch, onMounted } from 'vue'
import { ScrollArea } from '@/components/ui/scroll-area'
import SettingsSidebar, { type SettingsPageKey } from '../components/SettingsSidebar.vue'
import FunctionContent from '../components/FunctionContent.vue'
import AppearanceContent from '../components/AppearanceContent.vue'
import GeneralSettingsContent from '../components/GeneralSettingsContent.vue'
import HotkeySettingsContent from '../components/HotkeySettingsContent.vue'
import WindowSceneContent from '../components/WindowSceneContent.vue'
import FloatingWindowContent from '../components/FloatingWindowContent.vue'
import { useSettingsStore } from '../store'

const activePage = ref<SettingsPageKey>('general')
const scrollAreaRef = ref<InstanceType<typeof ScrollArea> | null>(null)
const store = useSettingsStore()

// 初始化时加载设置
onMounted(() => {
  store.init()
})

const scrollToTop = () => {
  scrollAreaRef.value?.viewportElement?.scrollTo({ top: 0, behavior: 'smooth' })
}

watch(activePage, () => {
  scrollToTop()
})
</script>

<template>
  <div class="flex h-full bg-background text-foreground">
    <SettingsSidebar v-model:activePage="activePage" />
    <div class="flex h-full flex-1 flex-col overflow-hidden">
      <ScrollArea ref="scrollAreaRef" class="h-full w-full flex-1">
        <div class="mx-auto max-w-4xl p-8">
          <GeneralSettingsContent v-if="activePage === 'general'" />
          <HotkeySettingsContent v-if="activePage === 'hotkeys'" />
          <FunctionContent v-if="activePage === 'capture'" />
          <WindowSceneContent v-if="activePage === 'windowScene'" />
          <FloatingWindowContent v-if="activePage === 'floatingWindow'" />
          <AppearanceContent v-if="activePage === 'webAppearance'" />
        </div>
      </ScrollArea>
    </div>
  </div>
</template>

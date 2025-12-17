
<script setup lang="ts">
import { ref, watch, onMounted } from 'vue'
import { ScrollArea } from '@/components/ui/scroll-area'
import SettingsSidebar, { type SettingsPageKey } from './SettingsSidebar.vue'
import FunctionContent from './FunctionContent.vue'
import MenuContent from './MenuContent.vue'
import AppearanceContent from './AppearanceContent.vue'
import GeneralSettingsContent from './GeneralSettingsContent.vue'
import { useSettingsStore } from '../store'

const activePage = ref<SettingsPageKey>('function')
const store = useSettingsStore()

// 初始化时加载设置
onMounted(() => {
  store.init()
})

const scrollToTop = () => {
    // Scroll area reset logic if needed, usually managed natively or via ref access
    // Shadcn vue ScrollArea might not expose direct method easily without accessing underlying viewportElement
    // For now we rely on content replacement
}

watch(activePage, () => {
    scrollToTop()
})
</script>

<template>
  <div class="flex h-full bg-background text-foreground">
    <SettingsSidebar v-model:activePage="activePage" />
    <div class="flex h-full flex-1 flex-col overflow-hidden">
      <ScrollArea class="flex-1 w-full">
        <div class="mx-auto max-w-4xl p-8">
           <FunctionContent v-if="activePage === 'function'" />
           <MenuContent v-if="activePage === 'menu'" />
           <AppearanceContent v-if="activePage === 'appearance'" />
           <GeneralSettingsContent v-if="activePage === 'general'" />
        </div>
      </ScrollArea>
    </div>
  </div>
</template>

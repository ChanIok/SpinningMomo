<script setup lang="ts">
import { computed, onMounted, ref, watch } from 'vue'
import { useRoute, useRouter } from 'vue-router'
import { useWindowSize } from '@vueuse/core'
import { ScrollArea } from '@/components/ui/scroll-area'
import SettingsSidebar from '../components/SettingsSidebar.vue'
import SettingsMenuList from '../components/SettingsMenuList.vue'
import CaptureSettingsContent from '../components/CaptureSettingsContent.vue'
import AppearanceContent from '../components/AppearanceContent.vue'
import ExtensionsContent from '../components/ExtensionsContent.vue'
import GeneralSettingsContent from '../components/GeneralSettingsContent.vue'
import HotkeySettingsContent from '../components/HotkeySettingsContent.vue'
import WindowSceneContent from '../components/WindowSceneContent.vue'
import FloatingWindowContent from '../components/FloatingWindowContent.vue'
import BackupSettingsContent from '../components/BackupSettingsContent.vue'
import NetworkAccessContent from '../components/NetworkAccessContent.vue'
import AdbModeContent from '../components/AdbModeContent.vue'
import { useSettingsStore } from '../store'
import { SETTINGS_COMPACT_BREAKPOINT, isValidSettingsPageKey, type SettingsPageKey } from '../menu'

const route = useRoute()
const router = useRouter()
const store = useSettingsStore()
const scrollAreaRef = ref<InstanceType<typeof ScrollArea> | null>(null)

const { width: windowWidth } = useWindowSize()
const isCompact = computed(
  () => windowWidth.value > 0 && windowWidth.value < SETTINGS_COMPACT_BREAKPOINT
)

// 当前路由参数中的有效 section
const routeSection = computed<SettingsPageKey | undefined>(() => {
  const section = route.params.section
  return typeof section === 'string' && isValidSettingsPageKey(section) ? section : undefined
})

// PC 桌面端激活的分类（默认为 general）
const pcActivePage = computed<SettingsPageKey>(() => routeSection.value ?? 'general')

// 在 PC 模式下，若进入设置且没有 section 参数，静默同步路由为 /settings/general
watch(
  [isCompact, routeSection],
  ([compact, section]) => {
    if (!compact && !section && route.name === 'settings') {
      void router.replace({
        name: 'settings',
        params: { section: 'general' },
      })
    }
  },
  { immediate: true }
)

// PC 端侧边栏点击切换时，使用 replace 保持历史栈不被膨胀
const handlePcPageChange = (key: SettingsPageKey) => {
  if (key === routeSection.value) {
    return
  }
  void router.replace({
    name: 'settings',
    params: { section: key },
  })
}

// 初始化时加载设置
onMounted(() => {
  store.init()
})

const scrollToTop = () => {
  scrollAreaRef.value?.viewportElement?.scrollTo({ top: 0, behavior: 'smooth' })
}

watch(
  () => routeSection.value,
  () => {
    scrollToTop()
  }
)
</script>

<template>
  <div class="h-full w-full text-foreground">
    <!-- 紧凑模式：一级菜单列表视图 -->
    <SettingsMenuList v-if="isCompact && !routeSection" />

    <!-- 紧凑模式：二级具体设置项全宽视图 -->
    <div v-else-if="isCompact && routeSection" class="h-full w-full overflow-hidden">
      <ScrollArea ref="scrollAreaRef" class="h-full w-full">
        <div class="compact-settings-content px-4 py-4">
          <GeneralSettingsContent v-if="routeSection === 'general'" />
          <NetworkAccessContent v-if="routeSection === 'networkAccess'" />
          <HotkeySettingsContent v-if="routeSection === 'hotkeys'" />
          <CaptureSettingsContent v-if="routeSection === 'capture'" />
          <ExtensionsContent v-if="routeSection === 'extensions'" />
          <AdbModeContent v-if="routeSection === 'adbMode'" />
          <WindowSceneContent v-if="routeSection === 'windowScene'" />
          <FloatingWindowContent v-if="routeSection === 'floatingWindow'" />
          <AppearanceContent v-if="routeSection === 'webAppearance'" />
          <BackupSettingsContent v-if="routeSection === 'backup'" />
        </div>
      </ScrollArea>
    </div>

    <!-- PC 桌面模式：保持原汁原味的左右双栏布局 -->
    <div v-else class="flex h-full w-full justify-center">
      <div class="flex h-full w-full max-w-6xl">
        <SettingsSidebar :active-page="pcActivePage" @update:active-page="handlePcPageChange" />
        <div class="flex h-full flex-1 flex-col overflow-hidden">
          <ScrollArea ref="scrollAreaRef" class="h-full w-full flex-1">
            <div class="px-8 py-4">
              <GeneralSettingsContent v-if="pcActivePage === 'general'" />
              <NetworkAccessContent v-if="pcActivePage === 'networkAccess'" />
              <HotkeySettingsContent v-if="pcActivePage === 'hotkeys'" />
              <CaptureSettingsContent v-if="pcActivePage === 'capture'" />
              <ExtensionsContent v-if="pcActivePage === 'extensions'" />
              <AdbModeContent v-if="pcActivePage === 'adbMode'" />
              <WindowSceneContent v-if="pcActivePage === 'windowScene'" />
              <FloatingWindowContent v-if="pcActivePage === 'floatingWindow'" />
              <AppearanceContent v-if="pcActivePage === 'webAppearance'" />
              <BackupSettingsContent v-if="pcActivePage === 'backup'" />
            </div>
          </ScrollArea>
        </div>
      </div>
    </div>
  </div>
</template>

<style scoped>
/* 仅在紧凑模式详情页生效：使分组大标题及描述与下方卡片内文字垂直对齐 */
.compact-settings-content :deep(div:has(> h3)) {
  padding-left: 1rem;
  padding-right: 1rem;
}
</style>

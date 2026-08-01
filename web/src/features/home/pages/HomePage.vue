<script setup lang="ts">
import type { Component } from 'vue'
import { ref, computed, onMounted, onUnmounted } from 'vue'
import { useRouter } from 'vue-router'
import { Images, Info, Settings } from '@lucide/vue'
import { Tooltip, TooltipContent, TooltipProvider, TooltipTrigger } from '@/components/ui/tooltip'
import { on as onRpc, off as offRpc } from '@/core/rpc'
import { useI18n } from '@/composables/useI18n'
import { galleryApi } from '@/features/gallery/api'
import type { HomeStats } from '@/features/gallery/types'
import { formatFileSize } from '@/lib/utils'
import { useSettingsStore } from '@/features/settings/store'
import { resolveBackgroundImageUrl } from '@/features/settings/backgroundPath'
import { pushWithViewTransition } from '@/router/viewTransition'
import momoOutlineSvg from '@/assets/momo-outline.svg?raw'

interface NavAction {
  key: string
  label: string
  icon: Component
  action: () => void
  disabled?: boolean
  animate?: boolean
}

const { t, locale } = useI18n()
const router = useRouter()
const settingsStore = useSettingsStore()

const showMomoOutline = computed(
  () => !resolveBackgroundImageUrl(settingsStore.appSettings.ui.background)
)

const HOME_STATS_REFRESH_DEBOUNCE_MS = 400

const hasLoadedHomeStats = ref(false)
const homeStats = ref<HomeStats>({
  totalCount: 0,
  photoCount: 0,
  videoCount: 0,
  livePhotoCount: 0,
  totalSize: 0,
  todayAddedCount: 0,
})

const photoCount = computed(() => homeStats.value.photoCount + homeStats.value.livePhotoCount)
const videoCount = computed(() => homeStats.value.videoCount)

const numberFormatter = computed(() => new Intl.NumberFormat(locale.value))

const formatCount = (value: number): string => {
  return numberFormatter.value.format(Math.max(0, value))
}

const formattedPhotoCount = computed(() => formatCount(photoCount.value))
const formattedVideoCount = computed(() => formatCount(videoCount.value))
const formattedTotalSize = computed(() => formatFileSize(homeStats.value.totalSize))
const formattedTodayAdded = computed(() => {
  const value = Math.max(0, homeStats.value.todayAddedCount)
  const formatted = formatCount(value)
  return value > 0 ? `+${formatted}` : formatted
})

let isUnmounted = false
let refreshInFlight = false
let refreshQueued = false
let refreshTimer: ReturnType<typeof setTimeout> | null = null

const clearRefreshTimer = () => {
  if (refreshTimer !== null) {
    clearTimeout(refreshTimer)
    refreshTimer = null
  }
}

const refreshHomeStats = async () => {
  if (refreshInFlight) {
    refreshQueued = true
    return
  }

  refreshInFlight = true
  do {
    refreshQueued = false
    try {
      const stats = await galleryApi.getHomeStats()
      if (isUnmounted) break
      homeStats.value = stats
      hasLoadedHomeStats.value = true
    } catch (error) {
      console.error('Failed to refresh home stats:', error)
    }
  } while (refreshQueued)

  refreshInFlight = false
}

const scheduleHomeStatsRefresh = () => {
  clearRefreshTimer()
  refreshTimer = setTimeout(() => {
    refreshTimer = null
    if (isUnmounted) return
    void refreshHomeStats()
  }, HOME_STATS_REFRESH_DEBOUNCE_MS)
}

const galleryChangedHandler = () => {
  scheduleHomeStatsRefresh()
}

const handleOpenPage = (name: 'gallery' | 'settings' | 'about') => {
  void pushWithViewTransition(router, { name })
}

const navActions = computed<NavAction[]>(() => [
  {
    key: 'gallery',
    label: t('app.navigation.gallery'),
    icon: Images,
    action: () => handleOpenPage('gallery'),
  },
  {
    key: 'settings',
    label: t('app.navigation.settings'),
    icon: Settings,
    action: () => handleOpenPage('settings'),
  },
  {
    key: 'about',
    label: t('app.navigation.about'),
    icon: Info,
    action: () => handleOpenPage('about'),
  },
])

onMounted(() => {
  void refreshHomeStats()
  onRpc('gallery.changed', galleryChangedHandler)
})

onUnmounted(() => {
  isUnmounted = true
  clearRefreshTimer()
  offRpc('gallery.changed', galleryChangedHandler)
})
</script>

<template>
  <div class="relative h-full w-full overflow-hidden select-none">
    <!-- 大喵插画背景 -->
    <div
      v-if="showMomoOutline"
      class="pointer-events-none absolute top-10 right-10 bottom-6 z-0 w-auto max-w-full text-white select-none dark:text-white/50"
      aria-hidden="true"
    >
      <div
        class="flex h-full w-full items-center justify-end [&_svg]:h-full [&_svg]:w-auto [&_svg]:max-w-none [&_svg]:shrink-0"
        v-html="momoOutlineSvg"
      ></div>
    </div>

    <!-- 1. 左上角：品牌 Header 水印 + 垂直导航工具栏 -->
    <div class="absolute top-8 left-8 z-20 flex flex-col items-start gap-14">
      <div class="pointer-events-none flex flex-col">
        <h1 class="text-sm font-medium tracking-[0.3em] text-foreground/90 uppercase">
          Spinning Momo
        </h1>
        <p class="mt-1 text-[0.7rem] font-light tracking-[0.22em] text-foreground/50 uppercase">
          Infinity Record
        </p>
      </div>

      <TooltipProvider>
        <div class="flex flex-col items-start gap-3">
          <Tooltip v-for="item in navActions" :key="item.key">
            <TooltipTrigger as-child>
              <div class="relative shrink-0 overflow-hidden rounded-md backdrop-blur-md">
                <div class="app-background-overlay pointer-events-none absolute inset-0 z-0"></div>
                <div
                  class="surface-middle pointer-events-none absolute inset-0 z-0 opacity-90"
                ></div>

                <button
                  type="button"
                  class="relative z-10 flex h-12 w-12 items-center justify-center text-foreground/75 transition-colors hover:bg-black/10 hover:text-foreground focus-visible:ring-2 focus-visible:ring-ring focus-visible:outline-none disabled:pointer-events-none disabled:opacity-40 dark:hover:bg-white/10"
                  :disabled="item.disabled"
                  @click="item.action"
                >
                  <component
                    :is="item.icon"
                    class="h-5 w-5"
                    :class="item.animate ? 'animate-pulse' : ''"
                    :stroke-width="1.7"
                  />
                </button>
              </div>
            </TooltipTrigger>
            <TooltipContent side="right" variant="sidebar">
              {{ item.label }}
            </TooltipContent>
          </Tooltip>
        </div>
      </TooltipProvider>
    </div>

    <!-- 2. 左下角：数据概览卡片 (HUD Corner Widget) -->
    <div class="absolute bottom-8 left-8 z-20 animate-in duration-300 fade-in-0">
      <div class="relative overflow-hidden rounded-md backdrop-blur-md">
        <div class="app-background-overlay pointer-events-none absolute inset-0 z-0"></div>
        <div class="surface-middle pointer-events-none absolute inset-0 z-0 opacity-90"></div>

        <div class="relative z-10 p-5">
          <div class="flex items-center gap-6">
            <div class="flex flex-col gap-0.5">
              <span class="text-[0.65rem] font-light tracking-widest text-foreground/40 uppercase">
                Photos
              </span>
              <span class="text-sm font-medium tracking-wider text-foreground/90">
                {{ hasLoadedHomeStats ? formattedPhotoCount : '—' }}
              </span>
            </div>

            <template v-if="videoCount > 0">
              <div class="h-6 w-px bg-foreground/10"></div>
              <div class="flex flex-col gap-0.5">
                <span
                  class="text-[0.65rem] font-light tracking-widest text-foreground/40 uppercase"
                >
                  Videos
                </span>
                <span class="text-sm font-medium tracking-wider text-foreground/90">
                  {{ hasLoadedHomeStats ? formattedVideoCount : '—' }}
                </span>
              </div>
            </template>

            <div class="h-6 w-px bg-foreground/10"></div>

            <div class="flex flex-col gap-0.5">
              <span class="text-[0.65rem] font-light tracking-widest text-foreground/40 uppercase">
                Storage
              </span>
              <span class="text-sm font-medium tracking-wider text-foreground/90">
                {{ hasLoadedHomeStats ? formattedTotalSize : '—' }}
              </span>
            </div>

            <div class="h-6 w-px bg-foreground/10"></div>

            <div class="flex flex-col gap-0.5">
              <span class="text-[0.65rem] font-light tracking-widest text-foreground/40 uppercase">
                Today
              </span>
              <span
                class="text-sm font-medium tracking-wider"
                :class="homeStats.todayAddedCount > 0 ? 'text-primary/90' : 'text-foreground/90'"
              >
                {{ hasLoadedHomeStats ? formattedTodayAdded : '—' }}
              </span>
            </div>
          </div>
        </div>
      </div>
    </div>
  </div>
</template>

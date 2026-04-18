<script setup lang="ts">
import { ref, computed, onMounted, onUnmounted } from 'vue'
import { FolderOpen } from 'lucide-vue-next'
import { on as onRpc, off as offRpc } from '@/core/rpc'
import { useI18n } from '@/composables/useI18n'
import { useToast } from '@/composables/useToast'
import { galleryApi } from '@/features/gallery/api'
import type { HomeStats } from '@/features/gallery/types'
import { formatFileSize } from '@/lib/utils'
import { featuresApi } from '@/features/settings/featuresApi'
import { useSettingsStore } from '@/features/settings/store'
import { resolveBackgroundImageUrl } from '@/features/settings/backgroundPath'
import momoOutlineSvg from '@/assets/momo-outline.svg?raw'

const { t, locale } = useI18n()
const { toast } = useToast()
const settingsStore = useSettingsStore()

const showMomoOutline = computed(
  () => !resolveBackgroundImageUrl(settingsStore.appSettings.ui.background)
)

const HOME_STATS_REFRESH_DEBOUNCE_MS = 400

const isOpening = ref(false)
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

const handleOpenOutputDirectory = async () => {
  if (isOpening.value) return

  isOpening.value = true
  try {
    await featuresApi.invoke('output.open_folder')
  } catch (error) {
    console.error('Failed to open output directory:', error)
    toast.error(t('home.outputDir.openFailed'))
  } finally {
    isOpening.value = false
  }
}

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
  <div class="relative h-full w-full overflow-x-hidden">
    <div
      v-if="showMomoOutline"
      class="pointer-events-none absolute top-10 right-10 bottom-6 z-10 w-[min(46vw,580px)] max-w-full text-white select-none dark:text-white/50"
      aria-hidden="true"
    >
      <div
        class="flex h-full w-full items-center justify-end [&_svg]:h-full [&_svg]:w-auto [&_svg]:max-w-none [&_svg]:shrink-0"
        v-html="momoOutlineSvg"
      ></div>
    </div>

    <div
      v-if="hasLoadedHomeStats"
      class="pointer-events-none absolute bottom-8 left-8 z-20 animate-in duration-600 fade-in-0"
    >
      <div
        class="relative overflow-hidden rounded-sm border border-border/30 shadow-sm backdrop-blur-md"
      >
        <!-- Base Backgrounds -->
        <div class="app-background-overlay pointer-events-none absolute inset-0 z-0"></div>
        <div class="surface-middle pointer-events-none absolute inset-0 z-0 opacity-90"></div>

        <!-- Subtle Inner Border for Premium Feel -->
        <div
          class="pointer-events-none absolute inset-[1px] z-10 rounded-sm border border-foreground/5"
        ></div>

        <div class="relative z-20 flex min-w-[240px] flex-col p-6">
          <!-- Brand Header -->
          <div class="mb-4 flex flex-col">
            <h2 class="text-xs font-medium tracking-[0.3em] text-foreground/90 uppercase">
              Spinning Momo
            </h2>
            <p class="mt-1 text-[0.65rem] font-light tracking-[0.2em] text-foreground/50 uppercase">
              Infinity Record
            </p>
          </div>

          <!-- Divider -->
          <div class="mb-5 h-[1px] w-full bg-foreground/10"></div>

          <!-- Stats Grid -->
          <div class="grid grid-cols-2 gap-x-6 gap-y-4">
            <div class="flex flex-col gap-0.5">
              <span class="text-[0.65rem] font-light tracking-widest text-foreground/40 uppercase"
                >Photos</span
              >
              <span class="text-sm font-medium tracking-wider text-foreground/90">{{
                formattedPhotoCount
              }}</span>
            </div>

            <div class="flex flex-col gap-0.5">
              <span class="text-[0.65rem] font-light tracking-widest text-foreground/40 uppercase"
                >Storage</span
              >
              <span class="text-sm font-medium tracking-wider text-foreground/90">{{
                formattedTotalSize
              }}</span>
            </div>

            <div v-if="videoCount > 0" class="flex flex-col gap-0.5">
              <span class="text-[0.65rem] font-light tracking-widest text-foreground/40 uppercase"
                >Videos</span
              >
              <span class="text-sm font-medium tracking-wider text-foreground/90">{{
                formattedVideoCount
              }}</span>
            </div>

            <div class="flex flex-col gap-0.5">
              <span class="text-[0.65rem] font-light tracking-widest text-foreground/40 uppercase"
                >Today</span
              >
              <span
                class="text-sm font-medium tracking-wider"
                :class="homeStats.todayAddedCount > 0 ? 'text-primary/90' : 'text-foreground/90'"
              >
                {{ formattedTodayAdded }}
              </span>
            </div>
          </div>
        </div>
      </div>
    </div>

    <div class="group absolute right-8 bottom-8 z-20 flex flex-col items-end gap-3">
      <!-- Tooltip Label -->
      <div
        class="pointer-events-none rounded-sm border border-border/30 px-3 py-1.5 opacity-0 backdrop-blur-md transition-all duration-300 group-hover:-translate-y-1 group-hover:opacity-100"
      >
        <div
          class="app-background-overlay pointer-events-none absolute inset-0 z-0 rounded-sm"
        ></div>
        <div
          class="surface-middle pointer-events-none absolute inset-0 z-0 rounded-sm opacity-90"
        ></div>
        <span
          class="relative z-10 text-[0.65rem] font-medium tracking-widest text-foreground/80 uppercase"
        >
          Open Folder
        </span>
      </div>

      <!-- Square Shutter Button -->
      <button
        class="relative flex h-[52px] w-[52px] cursor-pointer items-center justify-center overflow-hidden rounded-sm border border-border/30 shadow-sm backdrop-blur-md transition-all duration-300 hover:shadow-md focus:outline-none active:scale-[0.97]"
        :disabled="isOpening"
        @click="handleOpenOutputDirectory"
      >
        <!-- Base Backgrounds -->
        <div class="app-background-overlay pointer-events-none absolute inset-0 z-0"></div>
        <div class="surface-middle pointer-events-none absolute inset-0 z-0 opacity-90"></div>

        <!-- Subtle Inner Border -->
        <div
          class="pointer-events-none absolute inset-[1px] z-10 rounded-sm border border-foreground/5"
        ></div>

        <!-- Hover Overlay -->
        <div
          class="pointer-events-none absolute inset-0 z-10 bg-foreground/5 opacity-0 transition-opacity duration-300 group-hover:opacity-100"
        ></div>

        <!-- Icon -->
        <FolderOpen
          class="relative z-20 h-5 w-5 text-foreground/70 transition-all duration-300 group-hover:text-foreground"
          :class="isOpening ? 'animate-pulse' : ''"
          stroke-width="1.5"
        />
      </button>
    </div>
  </div>
</template>

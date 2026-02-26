<script setup lang="ts">
import { ref, computed } from 'vue'
import { FolderOpen, Aperture } from 'lucide-vue-next'
import { useI18n } from '@/composables/useI18n'
import { useToast } from '@/composables/useToast'
import { featuresApi } from '@/features/settings/featuresApi'
import { useSettingsStore } from '@/features/settings/store'
import { resolveBackgroundImageUrl } from '@/features/settings/backgroundPath'

const { t } = useI18n()
const { toast } = useToast()
const settingsStore = useSettingsStore()

const isOpening = ref(false)

const hasBackgroundImage = computed(() =>
  Boolean(resolveBackgroundImageUrl(settingsStore.appSettings.ui.background))
)

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
</script>

<template>
  <div class="relative h-full w-full">
    <!-- Empty State -->
    <div
      v-if="!hasBackgroundImage"
      class="pointer-events-none absolute inset-0 z-0 flex flex-col items-center justify-center space-y-4 pb-10"
    >
      <Aperture class="h-10 w-10 stroke-[1.5] text-foreground/20" />
      <p class="text-sm font-light tracking-[0.2em] text-foreground/30 antialiased">
        {{ t('home.emptyState.poetry') }}
      </p>
    </div>
    <button
      class="group absolute right-8 bottom-8 z-20 flex h-14 cursor-pointer flex-nowrap items-center overflow-hidden rounded-full border border-border/40 shadow-sm transition-all duration-500 hover:shadow-md focus:outline-none active:scale-95"
      style="transition-timing-function: cubic-bezier(0.2, 0.8, 0.2, 1)"
      :class="isOpening ? 'max-w-[240px]' : 'max-w-[56px] hover:max-w-[240px]'"
      :disabled="isOpening"
      @click="handleOpenOutputDirectory"
    >
      <!-- Base Background (叠加色) -->
      <div class="app-background-overlay pointer-events-none absolute inset-0 z-0"></div>

      <!-- Surface Layer -->
      <div class="surface-middle pointer-events-none absolute inset-0 z-0"></div>

      <!-- Hover Overlay -->
      <div
        class="pointer-events-none absolute inset-0 z-0 bg-foreground opacity-0 transition-opacity duration-300 group-hover:opacity-[0.04] dark:group-hover:opacity-[0.08]"
      ></div>

      <div class="relative z-10 flex h-14 w-14 shrink-0 items-center justify-center">
        <FolderOpen
          class="h-5 w-5 text-foreground/80 transition-colors group-hover:text-foreground"
          :class="isOpening ? 'animate-pulse' : ''"
        />
      </div>
      <span
        class="relative z-10 pr-6 text-sm font-medium whitespace-nowrap text-foreground/90 transition-opacity duration-300"
        :class="
          isOpening
            ? 'opacity-100 delay-100'
            : 'opacity-0 group-hover:opacity-100 group-hover:delay-100'
        "
      >
        {{ t('home.outputDir.openButton') }}
      </span>
    </button>
  </div>
</template>

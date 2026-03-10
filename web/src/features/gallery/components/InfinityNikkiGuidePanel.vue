<script setup lang="ts">
import { ref } from 'vue'
import { Button } from '@/components/ui/button'
import {
  Item,
  ItemContent,
  ItemDescription,
  ItemGroup,
  ItemMedia,
  ItemTitle,
} from '@/components/ui/item'
import {
  Accordion,
  AccordionContent,
  AccordionItem,
  AccordionTrigger,
} from '@/components/ui/accordion'
import { ScanText, FolderSymlink } from 'lucide-vue-next'
import { useI18n } from '@/composables/useI18n'
import { useToast } from '@/composables/useToast'
import { useSettingsStore } from '@/features/settings/store'
import type { AppSettings } from '@/features/settings/types'

const settingsStore = useSettingsStore()
const { t } = useI18n()
const { toast } = useToast()

const isSubmitting = ref(false)

type InfinityNikkiGuidePatch = Partial<AppSettings['plugins']['infinityNikki']>

async function saveGuideState(patch: InfinityNikkiGuidePatch) {
  await settingsStore.updateSettings({
    plugins: {
      infinityNikki: {
        ...settingsStore.appSettings.plugins.infinityNikki,
        ...patch,
      },
    },
  })
}

async function handleSkipGuide() {
  if (isSubmitting.value) {
    return
  }

  isSubmitting.value = true
  try {
    await saveGuideState({ galleryGuideSeen: true })
  } catch (error) {
    const message = error instanceof Error ? error.message : String(error)
    toast.error(t('gallery.guide.infinityNikki.saveFailedTitle'), {
      description: message,
    })
  } finally {
    isSubmitting.value = false
  }
}

async function handleEnableMetadataOnly() {
  if (isSubmitting.value) {
    return
  }

  isSubmitting.value = true
  try {
    await saveGuideState({
      galleryGuideSeen: true,
      allowOnlinePhotoMetadataExtract: true,
    })

    toast.success(t('gallery.guide.infinityNikki.metadataTaskStartedTitle'), {
      description: t('gallery.guide.infinityNikki.metadataTaskStartedDescription'),
    })
  } catch (error) {
    const message = error instanceof Error ? error.message : String(error)
    toast.error(t('gallery.guide.infinityNikki.actionFailedTitle'), {
      description: message,
    })
  } finally {
    isSubmitting.value = false
  }
}

async function handleApplyRecommended() {
  if (isSubmitting.value) {
    return
  }

  isSubmitting.value = true
  try {
    await saveGuideState({
      galleryGuideSeen: true,
      allowOnlinePhotoMetadataExtract: true,
      manageScreenshotHardlinks: true,
    })

    toast.success(t('gallery.guide.infinityNikki.recommendedTaskStartedTitle'), {
      description: t('gallery.guide.infinityNikki.recommendedTaskStartedDescription'),
    })
  } catch (error) {
    const message = error instanceof Error ? error.message : String(error)
    toast.error(t('gallery.guide.infinityNikki.actionFailedTitle'), {
      description: message,
    })
  } finally {
    isSubmitting.value = false
  }
}
</script>

<template>
  <div class="mx-auto flex h-full max-w-4xl items-center justify-center p-6">
    <div class="w-full space-y-4 rounded-xl border bg-card/95 p-6 shadow-sm">
      <div class="space-y-2">
        <h2 class="text-xl font-semibold text-foreground">
          {{ t('gallery.guide.infinityNikki.title') }}
        </h2>
        <p class="text-sm leading-6 text-muted-foreground">
          {{ t('gallery.guide.infinityNikki.description') }}
        </p>
      </div>

      <ItemGroup class="gap-4">
        <Item variant="outline" class="items-start">
          <ItemMedia variant="icon" class="mt-0.5">
            <ScanText class="size-4 text-primary" />
          </ItemMedia>
          <ItemContent class="flex-1">
            <ItemTitle>{{ t('gallery.guide.infinityNikki.metadataTitle') }}</ItemTitle>
            <ItemDescription>{{
              t('gallery.guide.infinityNikki.metadataDescription')
            }}</ItemDescription>
          </ItemContent>
        </Item>

        <Item variant="outline" class="items-start">
          <ItemMedia variant="icon" class="mt-0.5">
            <FolderSymlink class="size-4 text-primary" />
          </ItemMedia>
          <ItemContent class="flex-1">
            <ItemTitle>{{ t('gallery.guide.infinityNikki.hardlinksTitle') }}</ItemTitle>
            <ItemDescription>{{
              t('gallery.guide.infinityNikki.hardlinksDescription')
            }}</ItemDescription>
            <Accordion type="single" collapsible class="mt-2 w-full">
              <AccordionItem value="details" class="border-b-0">
                <AccordionTrigger class="py-2 text-xs text-muted-foreground hover:no-underline">
                  {{ t('gallery.guide.infinityNikki.hardlinksDetailsTrigger') }}
                </AccordionTrigger>
                <AccordionContent
                  class="text-xs leading-relaxed whitespace-pre-wrap text-muted-foreground"
                >
                  {{ t('gallery.guide.infinityNikki.hardlinksDetailsContent') }}
                </AccordionContent>
              </AccordionItem>
            </Accordion>
          </ItemContent>
        </Item>
      </ItemGroup>

      <div class="flex flex-wrap gap-3 pt-2">
        <Button :disabled="isSubmitting" @click="handleApplyRecommended">
          {{ t('gallery.guide.infinityNikki.actions.applyRecommended') }}
        </Button>
        <Button variant="secondary" :disabled="isSubmitting" @click="handleEnableMetadataOnly">
          {{ t('gallery.guide.infinityNikki.actions.enableMetadataOnly') }}
        </Button>
        <Button variant="ghost" :disabled="isSubmitting" @click="handleSkipGuide">
          {{ t('gallery.guide.infinityNikki.actions.skipForNow') }}
        </Button>
      </div>
    </div>
  </div>
</template>

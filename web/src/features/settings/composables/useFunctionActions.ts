import { useSettingsStore } from '../store'
import { DEFAULT_APP_SETTINGS } from '../types'
import { storeToRefs } from 'pinia'

export const useFunctionActions = () => {
  const store = useSettingsStore()
  const { appSettings } = storeToRefs(store)

  const updateWindowTitle = async (title: string) => {
    await store.updateSettings({
      ...appSettings.value,
      window: {
        ...appSettings.value.window,
        targetTitle: title,
      },
    })
  }

  const updateScreenshotDir = async (dirPath: string) => {
    await store.updateSettings({
      ...appSettings.value,
      features: {
        ...appSettings.value.features,
        screenshot: {
          ...appSettings.value.features.screenshot,
          screenshotDirPath: dirPath,
        },
      },
    })
  }

  const updateTaskbarLowerOnResize = async (enabled: boolean) => {
    await store.updateSettings({
      ...appSettings.value,
      window: {
        ...appSettings.value.window,
        taskbar: {
          ...appSettings.value.window.taskbar,
          lowerOnResize: enabled,
        },
      },
    })
  }

  const updateLetterboxEnabled = async (enabled: boolean) => {
    await store.updateSettings({
      ...appSettings.value,
      features: {
        ...appSettings.value.features,
        letterbox: {
          ...appSettings.value.features.letterbox,
          enabled,
        },
      },
    })
  }

  const updateRecordingOutputDir = async (dirPath: string) => {
    await store.updateSettings({
      ...appSettings.value,
      features: {
        ...appSettings.value.features,
        recording: {
          ...appSettings.value.features.recording,
          outputDirPath: dirPath,
        },
      },
    })
  }

  const updateRecordingFps = async (fps: number) => {
    await store.updateSettings({
      ...appSettings.value,
      features: {
        ...appSettings.value.features,
        recording: {
          ...appSettings.value.features.recording,
          fps,
        },
      },
    })
  }

  const updateRecordingBitrate = async (bitrate: number) => {
    await store.updateSettings({
      ...appSettings.value,
      features: {
        ...appSettings.value.features,
        recording: {
          ...appSettings.value.features.recording,
          bitrate,
        },
      },
    })
  }

  const updateRecordingEncoderMode = async (encoderMode: 'auto' | 'gpu' | 'cpu') => {
    await store.updateSettings({
      ...appSettings.value,
      features: {
        ...appSettings.value.features,
        recording: {
          ...appSettings.value.features.recording,
          encoderMode,
        },
      },
    })
  }

  const updateRecordingQuality = async (quality: number) => {
    await store.updateSettings({
      ...appSettings.value,
      features: {
        ...appSettings.value.features,
        recording: {
          ...appSettings.value.features.recording,
          quality,
        },
      },
    })
  }

  const updateRecordingRateControl = async (rateControl: 'cbr' | 'vbr') => {
    await store.updateSettings({
      ...appSettings.value,
      features: {
        ...appSettings.value.features,
        recording: {
          ...appSettings.value.features.recording,
          rateControl,
        },
      },
    })
  }

  const updateRecordingCodec = async (codec: 'h264' | 'h265') => {
    await store.updateSettings({
      ...appSettings.value,
      features: {
        ...appSettings.value.features,
        recording: {
          ...appSettings.value.features.recording,
          codec,
        },
      },
    })
  }

  const resetFunctionSettings = async () => {
    await store.updateSettings({
      ...appSettings.value,
      window: {
        ...appSettings.value.window,
        targetTitle: DEFAULT_APP_SETTINGS.window.targetTitle,
        taskbar: {
          ...appSettings.value.window.taskbar,
          lowerOnResize: DEFAULT_APP_SETTINGS.window.taskbar.lowerOnResize,
        },
      },
      features: {
        ...appSettings.value.features,
        screenshot: {
          ...appSettings.value.features.screenshot,
          screenshotDirPath: DEFAULT_APP_SETTINGS.features.screenshot.screenshotDirPath,
        },
        letterbox: {
          ...appSettings.value.features.letterbox,
          enabled: DEFAULT_APP_SETTINGS.features.letterbox.enabled,
        },
        recording: {
          ...appSettings.value.features.recording,
          outputDirPath: DEFAULT_APP_SETTINGS.features.recording.outputDirPath,
          fps: DEFAULT_APP_SETTINGS.features.recording.fps,
          bitrate: DEFAULT_APP_SETTINGS.features.recording.bitrate,
          quality: DEFAULT_APP_SETTINGS.features.recording.quality,
          rateControl: DEFAULT_APP_SETTINGS.features.recording.rateControl,
          encoderMode: DEFAULT_APP_SETTINGS.features.recording.encoderMode,
          codec: DEFAULT_APP_SETTINGS.features.recording.codec,
        },
      },
    })
  }

  return {
    updateWindowTitle,
    updateScreenshotDir,
    updateTaskbarLowerOnResize,
    updateLetterboxEnabled,
    updateRecordingOutputDir,
    updateRecordingFps,
    updateRecordingBitrate,
    updateRecordingQuality,
    updateRecordingRateControl,
    updateRecordingEncoderMode,
    updateRecordingCodec,
    resetFunctionSettings,
  }
}

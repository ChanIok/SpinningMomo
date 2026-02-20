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

  const updateOutputDir = async (dirPath: string) => {
    await store.updateSettings({
      ...appSettings.value,
      features: {
        ...appSettings.value.features,
        outputDirPath: dirPath,
      },
    })
  }

  const updateGameAlbumPath = async (dirPath: string) => {
    await store.updateSettings({
      ...appSettings.value,
      features: {
        ...appSettings.value.features,
        screenshot: {
          ...appSettings.value.features.screenshot,
          gameAlbumPath: dirPath,
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

  // Motion Photo 设置
  const updateMotionPhotoDuration = async (duration: number) => {
    await store.updateSettings({
      ...appSettings.value,
      features: {
        ...appSettings.value.features,
        motionPhoto: {
          ...appSettings.value.features.motionPhoto,
          duration,
        },
      },
    })
  }

  const updateMotionPhotoResolution = async (resolution: number) => {
    await store.updateSettings({
      ...appSettings.value,
      features: {
        ...appSettings.value.features,
        motionPhoto: {
          ...appSettings.value.features.motionPhoto,
          resolution,
        },
      },
    })
  }

  const updateMotionPhotoFps = async (fps: number) => {
    await store.updateSettings({
      ...appSettings.value,
      features: {
        ...appSettings.value.features,
        motionPhoto: {
          ...appSettings.value.features.motionPhoto,
          fps,
        },
      },
    })
  }

  const updateMotionPhotoBitrate = async (bitrate: number) => {
    await store.updateSettings({
      ...appSettings.value,
      features: {
        ...appSettings.value.features,
        motionPhoto: {
          ...appSettings.value.features.motionPhoto,
          bitrate,
        },
      },
    })
  }

  const updateMotionPhotoCodec = async (codec: 'h264' | 'h265') => {
    await store.updateSettings({
      ...appSettings.value,
      features: {
        ...appSettings.value.features,
        motionPhoto: {
          ...appSettings.value.features.motionPhoto,
          codec,
        },
      },
    })
  }

  const updateMotionPhotoAudioSource = async (audioSource: 'none' | 'system' | 'game_only') => {
    await store.updateSettings({
      ...appSettings.value,
      features: {
        ...appSettings.value.features,
        motionPhoto: {
          ...appSettings.value.features.motionPhoto,
          audioSource,
        },
      },
    })
  }

  const updateMotionPhotoAudioBitrate = async (audioBitrate: number) => {
    await store.updateSettings({
      ...appSettings.value,
      features: {
        ...appSettings.value.features,
        motionPhoto: {
          ...appSettings.value.features.motionPhoto,
          audioBitrate,
        },
      },
    })
  }

  const updateMotionPhotoRateControl = async (rateControl: 'cbr' | 'vbr') => {
    await store.updateSettings({
      ...appSettings.value,
      features: {
        ...appSettings.value.features,
        motionPhoto: {
          ...appSettings.value.features.motionPhoto,
          rateControl,
        },
      },
    })
  }

  const updateMotionPhotoQuality = async (quality: number) => {
    await store.updateSettings({
      ...appSettings.value,
      features: {
        ...appSettings.value.features,
        motionPhoto: {
          ...appSettings.value.features.motionPhoto,
          quality,
        },
      },
    })
  }

  // Instant Replay 设置
  const updateReplayBufferDuration = async (duration: number) => {
    await store.updateSettings({
      ...appSettings.value,
      features: {
        ...appSettings.value.features,
        replayBuffer: {
          ...appSettings.value.features.replayBuffer,
          duration,
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

  const updateRecordingQp = async (qp: number) => {
    await store.updateSettings({
      ...appSettings.value,
      features: {
        ...appSettings.value.features,
        recording: {
          ...appSettings.value.features.recording,
          qp,
        },
      },
    })
  }

  const updateRecordingRateControl = async (rateControl: 'cbr' | 'vbr' | 'manual_qp') => {
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

  const updateRecordingAudioSource = async (audioSource: 'none' | 'system' | 'game_only') => {
    await store.updateSettings({
      ...appSettings.value,
      features: {
        ...appSettings.value.features,
        recording: {
          ...appSettings.value.features.recording,
          audioSource,
        },
      },
    })
  }

  const updateRecordingAudioBitrate = async (audioBitrate: number) => {
    await store.updateSettings({
      ...appSettings.value,
      features: {
        ...appSettings.value.features,
        recording: {
          ...appSettings.value.features.recording,
          audioBitrate,
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
      },
      features: {
        ...appSettings.value.features,
        outputDirPath: DEFAULT_APP_SETTINGS.features.outputDirPath,
        screenshot: {
          ...appSettings.value.features.screenshot,
          gameAlbumPath: DEFAULT_APP_SETTINGS.features.screenshot.gameAlbumPath,
        },
        letterbox: {
          ...appSettings.value.features.letterbox,
          enabled: DEFAULT_APP_SETTINGS.features.letterbox.enabled,
        },
        motionPhoto: {
          ...appSettings.value.features.motionPhoto,
          duration: DEFAULT_APP_SETTINGS.features.motionPhoto.duration,
          resolution: DEFAULT_APP_SETTINGS.features.motionPhoto.resolution,
          fps: DEFAULT_APP_SETTINGS.features.motionPhoto.fps,
          bitrate: DEFAULT_APP_SETTINGS.features.motionPhoto.bitrate,
          quality: DEFAULT_APP_SETTINGS.features.motionPhoto.quality,
          rateControl: DEFAULT_APP_SETTINGS.features.motionPhoto.rateControl,
          codec: DEFAULT_APP_SETTINGS.features.motionPhoto.codec,
          audioSource: DEFAULT_APP_SETTINGS.features.motionPhoto.audioSource,
          audioBitrate: DEFAULT_APP_SETTINGS.features.motionPhoto.audioBitrate,
        },
        replayBuffer: {
          ...appSettings.value.features.replayBuffer,
          duration: DEFAULT_APP_SETTINGS.features.replayBuffer.duration,
        },
        recording: {
          ...appSettings.value.features.recording,
          fps: DEFAULT_APP_SETTINGS.features.recording.fps,
          bitrate: DEFAULT_APP_SETTINGS.features.recording.bitrate,
          quality: DEFAULT_APP_SETTINGS.features.recording.quality,
          qp: DEFAULT_APP_SETTINGS.features.recording.qp,
          rateControl: DEFAULT_APP_SETTINGS.features.recording.rateControl,
          encoderMode: DEFAULT_APP_SETTINGS.features.recording.encoderMode,
          codec: DEFAULT_APP_SETTINGS.features.recording.codec,
          audioSource: DEFAULT_APP_SETTINGS.features.recording.audioSource,
          audioBitrate: DEFAULT_APP_SETTINGS.features.recording.audioBitrate,
        },
      },
    })
  }

  const resetCaptureExportSettings = async () => {
    await store.updateSettings({
      ...appSettings.value,
      features: {
        ...appSettings.value.features,
        outputDirPath: DEFAULT_APP_SETTINGS.features.outputDirPath,
        screenshot: {
          ...appSettings.value.features.screenshot,
          gameAlbumPath: DEFAULT_APP_SETTINGS.features.screenshot.gameAlbumPath,
        },
        motionPhoto: {
          ...appSettings.value.features.motionPhoto,
          duration: DEFAULT_APP_SETTINGS.features.motionPhoto.duration,
          resolution: DEFAULT_APP_SETTINGS.features.motionPhoto.resolution,
          fps: DEFAULT_APP_SETTINGS.features.motionPhoto.fps,
          bitrate: DEFAULT_APP_SETTINGS.features.motionPhoto.bitrate,
          quality: DEFAULT_APP_SETTINGS.features.motionPhoto.quality,
          rateControl: DEFAULT_APP_SETTINGS.features.motionPhoto.rateControl,
          codec: DEFAULT_APP_SETTINGS.features.motionPhoto.codec,
          audioSource: DEFAULT_APP_SETTINGS.features.motionPhoto.audioSource,
          audioBitrate: DEFAULT_APP_SETTINGS.features.motionPhoto.audioBitrate,
        },
        replayBuffer: {
          ...appSettings.value.features.replayBuffer,
          duration: DEFAULT_APP_SETTINGS.features.replayBuffer.duration,
        },
        recording: {
          ...appSettings.value.features.recording,
          fps: DEFAULT_APP_SETTINGS.features.recording.fps,
          bitrate: DEFAULT_APP_SETTINGS.features.recording.bitrate,
          quality: DEFAULT_APP_SETTINGS.features.recording.quality,
          qp: DEFAULT_APP_SETTINGS.features.recording.qp,
          rateControl: DEFAULT_APP_SETTINGS.features.recording.rateControl,
          encoderMode: DEFAULT_APP_SETTINGS.features.recording.encoderMode,
          codec: DEFAULT_APP_SETTINGS.features.recording.codec,
          audioSource: DEFAULT_APP_SETTINGS.features.recording.audioSource,
          audioBitrate: DEFAULT_APP_SETTINGS.features.recording.audioBitrate,
        },
      },
    })
  }

  const resetWindowSceneSettings = async () => {
    await store.updateSettings({
      ...appSettings.value,
      window: {
        ...appSettings.value.window,
        targetTitle: DEFAULT_APP_SETTINGS.window.targetTitle,
      },
      features: {
        ...appSettings.value.features,
        letterbox: {
          ...appSettings.value.features.letterbox,
          enabled: DEFAULT_APP_SETTINGS.features.letterbox.enabled,
        },
      },
    })
  }

  return {
    updateWindowTitle,
    updateOutputDir,
    updateGameAlbumPath,
    updateLetterboxEnabled,
    // Motion Photo
    updateMotionPhotoDuration,
    updateMotionPhotoResolution,
    updateMotionPhotoFps,
    updateMotionPhotoBitrate,
    updateMotionPhotoQuality,
    updateMotionPhotoRateControl,
    updateMotionPhotoCodec,
    updateMotionPhotoAudioSource,
    updateMotionPhotoAudioBitrate,
    // Instant Replay
    updateReplayBufferDuration,
    // Recording
    updateRecordingFps,
    updateRecordingBitrate,
    updateRecordingQuality,
    updateRecordingQp,
    updateRecordingRateControl,
    updateRecordingEncoderMode,
    updateRecordingCodec,
    updateRecordingAudioSource,
    updateRecordingAudioBitrate,
    resetFunctionSettings,
    resetCaptureExportSettings,
    resetWindowSceneSettings,
  }
}

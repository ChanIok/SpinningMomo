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

  const updateWindowCenterLockCursor = async (enabled: boolean) => {
    await store.updateSettings({
      ...appSettings.value,
      window: {
        ...appSettings.value.window,
        centerLockCursor: enabled,
      },
    })
  }

  const updateWindowLayeredCaptureWorkaround = async (enabled: boolean) => {
    await store.updateSettings({
      ...appSettings.value,
      window: {
        ...appSettings.value.window,
        enableLayeredCaptureWorkaround: enabled,
      },
    })
  }

  const updateWindowAlignSizeTo8 = async (enabled: boolean) => {
    await store.updateSettings({
      ...appSettings.value,
      window: {
        ...appSettings.value.window,
        alignWindowSizeTo8: enabled,
      },
    })
  }

  const updateWindowUseResolutionShortEdge = async (enabled: boolean) => {
    await store.updateSettings({
      ...appSettings.value,
      window: {
        ...appSettings.value.window,
        useResolutionShortEdge: enabled,
      },
    })
  }

  const updateWindowResetResolution = async (width: number, height: number) => {
    await store.updateSettings({
      ...appSettings.value,
      window: {
        ...appSettings.value.window,
        resetResolution: {
          ...appSettings.value.window.resetResolution,
          width,
          height,
        },
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
        externalAlbumPath: dirPath,
      },
    })
  }

  const updateSavedFileViewAction = async (
    savedFileViewAction: 'default_app' | 'reveal_in_explorer'
  ) => {
    await store.updateSettings({
      ...appSettings.value,
      features: {
        ...appSettings.value.features,
        savedFileViewAction,
      },
    })
  }

  const updateScreenshotFileFormat = async (fileFormat: 'png' | 'jpeg') => {
    await store.updateSettings({
      ...appSettings.value,
      features: {
        ...appSettings.value.features,
        screenshot: {
          ...(appSettings.value.features.screenshot ?? DEFAULT_APP_SETTINGS.features.screenshot),
          fileFormat,
        },
      },
    })
  }

  const updateScreenshotHdrEnabled = async (enableHdr: boolean) => {
    await store.updateSettings({
      ...appSettings.value,
      features: {
        ...appSettings.value.features,
        screenshot: {
          ...(appSettings.value.features.screenshot ?? DEFAULT_APP_SETTINGS.features.screenshot),
          enableHdr,
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
    const rec = appSettings.value.features.recording
    let codec = rec.codec
    if (encoderMode === 'cpu' && codec === 'h265') {
      codec = 'h264'
    }
    // HDR 录制只支持 GPU；切到 CPU 时同步关闭，避免保存出后端必然拒绝的配置。
    const enableHdr = encoderMode === 'cpu' ? false : rec.enableHdr
    await store.updateSettings({
      ...appSettings.value,
      features: {
        ...appSettings.value.features,
        recording: {
          ...rec,
          encoderMode,
          codec,
          enableHdr,
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
    const rec = appSettings.value.features.recording
    let encoderMode = rec.encoderMode
    if (codec === 'h265' && encoderMode === 'cpu') {
      encoderMode = 'auto'
    }
    // HDR 输出是 HEVC Main10；切回 H.264 时必须关闭 HDR。
    const enableHdr = codec === 'h264' ? false : rec.enableHdr
    await store.updateSettings({
      ...appSettings.value,
      features: {
        ...appSettings.value.features,
        recording: {
          ...rec,
          codec,
          encoderMode,
          enableHdr,
        },
      },
    })
  }

  const updateRecordingHdrEnabled = async (enableHdr: boolean) => {
    const rec = appSettings.value.features.recording
    await store.updateSettings({
      ...appSettings.value,
      features: {
        ...appSettings.value.features,
        recording: {
          ...rec,
          enableHdr,
          // 开启 HDR 时主动收敛到唯一支持组合：GPU + H.265。
          codec: enableHdr ? 'h265' : rec.codec,
          encoderMode: enableHdr ? 'gpu' : rec.encoderMode,
        },
      },
    })
  }

  const updateRecordingCaptureClientArea = async (captureClientArea: boolean) => {
    await store.updateSettings({
      ...appSettings.value,
      features: {
        ...appSettings.value.features,
        recording: {
          ...appSettings.value.features.recording,
          captureClientArea,
        },
      },
    })
  }

  const updateRecordingCaptureCursor = async (captureCursor: boolean) => {
    await store.updateSettings({
      ...appSettings.value,
      features: {
        ...appSettings.value.features,
        recording: {
          ...appSettings.value.features.recording,
          captureCursor,
        },
      },
    })
  }

  const updateRecordingAutoRestartOnResize = async (autoRestartOnResize: boolean) => {
    await store.updateSettings({
      ...appSettings.value,
      features: {
        ...appSettings.value.features,
        recording: {
          ...appSettings.value.features.recording,
          autoRestartOnResize,
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
        centerLockCursor: DEFAULT_APP_SETTINGS.window.centerLockCursor,
        alignWindowSizeTo8: DEFAULT_APP_SETTINGS.window.alignWindowSizeTo8,
        useResolutionShortEdge: DEFAULT_APP_SETTINGS.window.useResolutionShortEdge,
        resetResolution: {
          ...appSettings.value.window.resetResolution,
          width: DEFAULT_APP_SETTINGS.window.resetResolution.width,
          height: DEFAULT_APP_SETTINGS.window.resetResolution.height,
        },
      },
      features: {
        ...appSettings.value.features,
        outputDirPath: DEFAULT_APP_SETTINGS.features.outputDirPath,
        externalAlbumPath: DEFAULT_APP_SETTINGS.features.externalAlbumPath,
        savedFileViewAction: DEFAULT_APP_SETTINGS.features.savedFileViewAction,
        screenshot: {
          ...appSettings.value.features.screenshot,
          fileFormat: DEFAULT_APP_SETTINGS.features.screenshot.fileFormat,
          enableHdr: DEFAULT_APP_SETTINGS.features.screenshot.enableHdr,
        },
        letterbox: {
          ...appSettings.value.features.letterbox,
          enabled: DEFAULT_APP_SETTINGS.features.letterbox.enabled,
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
          enableHdr: DEFAULT_APP_SETTINGS.features.recording.enableHdr,
          captureClientArea: DEFAULT_APP_SETTINGS.features.recording.captureClientArea,
          captureCursor: DEFAULT_APP_SETTINGS.features.recording.captureCursor,
          autoRestartOnResize: DEFAULT_APP_SETTINGS.features.recording.autoRestartOnResize,
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
        externalAlbumPath: DEFAULT_APP_SETTINGS.features.externalAlbumPath,
        savedFileViewAction: DEFAULT_APP_SETTINGS.features.savedFileViewAction,
        screenshot: {
          ...appSettings.value.features.screenshot,
          fileFormat: DEFAULT_APP_SETTINGS.features.screenshot.fileFormat,
          enableHdr: DEFAULT_APP_SETTINGS.features.screenshot.enableHdr,
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
          enableHdr: DEFAULT_APP_SETTINGS.features.recording.enableHdr,
          captureClientArea: DEFAULT_APP_SETTINGS.features.recording.captureClientArea,
          captureCursor: DEFAULT_APP_SETTINGS.features.recording.captureCursor,
          autoRestartOnResize: DEFAULT_APP_SETTINGS.features.recording.autoRestartOnResize,
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
        enableLayeredCaptureWorkaround: DEFAULT_APP_SETTINGS.window.enableLayeredCaptureWorkaround,
        alignWindowSizeTo8: DEFAULT_APP_SETTINGS.window.alignWindowSizeTo8,
        useResolutionShortEdge: DEFAULT_APP_SETTINGS.window.useResolutionShortEdge,
        resetResolution: {
          ...appSettings.value.window.resetResolution,
          width: DEFAULT_APP_SETTINGS.window.resetResolution.width,
          height: DEFAULT_APP_SETTINGS.window.resetResolution.height,
        },
      },
    })
  }

  return {
    updateWindowTitle,
    updateWindowCenterLockCursor,
    updateWindowLayeredCaptureWorkaround,
    updateWindowAlignSizeTo8,
    updateWindowUseResolutionShortEdge,
    updateWindowResetResolution,
    updateOutputDir,
    updateGameAlbumPath,
    updateSavedFileViewAction,
    updateScreenshotFileFormat,
    updateScreenshotHdrEnabled,
    resetFunctionSettings,
    updateRecordingFps,
    updateRecordingBitrate,
    updateRecordingQuality,
    updateRecordingQp,
    updateRecordingRateControl,
    updateRecordingEncoderMode,
    updateRecordingCodec,
    updateRecordingHdrEnabled,
    updateRecordingCaptureClientArea,
    updateRecordingCaptureCursor,
    updateRecordingAutoRestartOnResize,
    updateRecordingAudioSource,
    updateRecordingAudioBitrate,
    resetCaptureExportSettings,
    resetWindowSceneSettings,
  }
}

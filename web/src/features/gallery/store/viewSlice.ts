import { computed, type Ref } from 'vue'
import {
  GALLERY_COMPACT_VIEW_SIZE_MAX,
  GALLERY_COMPACT_VIEW_SIZE_MIN,
  GALLERY_VIEW_SIZE_MAX,
  GALLERY_VIEW_SIZE_MIN,
} from '../constants'
import { createDefaultGallerySettings, type GallerySettings } from './persistence'

interface ViewSliceArgs {
  settings: Ref<GallerySettings>
  isCompactWindow: Readonly<Ref<boolean>>
}

interface GalleryViewSizeRange {
  min: number
  max: number
}

function clampViewSize(size: number, range: GalleryViewSizeRange): number {
  return Math.max(range.min, Math.min(range.max, size))
}

function sliderToSize(position: number, range: GalleryViewSizeRange): number {
  const normalized = Math.max(0, Math.min(100, position)) / 100
  return Math.round(range.min + (range.max - range.min) * normalized ** 2)
}

function sizeToSlider(size: number, range: GalleryViewSizeRange): number {
  const normalized = (clampViewSize(size, range) - range.min) / Math.max(1, range.max - range.min)
  return Math.round(Math.sqrt(normalized) * 100)
}

/**
 * 视图状态集中在持久化 view 对象，紧凑尺寸只是基于布局上下文的派生值。
 */
export function createViewSlice(args: ViewSliceArgs) {
  const { settings, isCompactWindow } = args

  const view = computed(() => settings.value.view)

  function getViewSizeRange(): GalleryViewSizeRange {
    return isCompactWindow.value
      ? { min: GALLERY_COMPACT_VIEW_SIZE_MIN, max: GALLERY_COMPACT_VIEW_SIZE_MAX }
      : { min: GALLERY_VIEW_SIZE_MIN, max: GALLERY_VIEW_SIZE_MAX }
  }

  function getEffectiveViewSize(): number {
    return clampViewSize(view.value.size, getViewSizeRange())
  }

  function setViewSize(size: number) {
    const range = getViewSizeRange()
    const validSize = clampViewSize(size, range)
    view.value.size = validSize
    console.log('📏 视图大小调整:', validSize, 'px')
  }

  function setViewSizeFromSlider(sliderPosition: number) {
    const range = getViewSizeRange()
    const validSize = sliderToSize(sliderPosition, range)
    view.value.size = validSize
    console.log('📏 视图大小调整:', validSize, 'px (slider:', sliderPosition, '%)')
  }

  function getSliderPosition(): number {
    return sizeToSlider(getEffectiveViewSize(), getViewSizeRange())
  }

  function increaseViewSize() {
    const currentSlider = getSliderPosition()
    if (currentSlider < 100) {
      setViewSizeFromSlider(Math.min(100, currentSlider + 5))
    }
  }

  function decreaseViewSize() {
    const currentSlider = getSliderPosition()
    if (currentSlider > 0) {
      setViewSizeFromSlider(Math.max(0, currentSlider - 5))
    }
  }

  function resetViewState() {
    settings.value.view = { ...createDefaultGallerySettings().view }
  }

  return {
    view,
    getViewSizeRange,
    getEffectiveViewSize,
    setViewSize,
    setViewSizeFromSlider,
    getSliderPosition,
    increaseViewSize,
    decreaseViewSize,
    resetViewState,
  }
}

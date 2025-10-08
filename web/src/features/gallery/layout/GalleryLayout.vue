<script setup lang="ts">
import { Split } from '@/components/ui/split'
import { useGalleryLayout } from '../composables'
import GallerySidebar from './GallerySidebar.vue'
import GalleryViewer from './GalleryViewer.vue'
import GalleryDetails from './GalleryDetails.vue'

// 使用布局管理
const { isSidebarOpen, isDetailsOpen } = useGalleryLayout()
</script>

<template>
  <!-- 左中右三区域布局 -->
  <div class="h-full w-full">
    <!-- 第一层分割：左侧 + (中右) -->
    <Split direction="horizontal" :default-size="0.2" :min="0.1" :max="0.3">
      <!-- 左侧区域 - 侧边栏 -->
      <template #1>
        <GallerySidebar v-if="isSidebarOpen" />
      </template>

      <!-- 中右区域 -->
      <template #2>
        <!-- 第二层分割：中间 + 右侧 -->
        <Split direction="horizontal" :default-size="0.7" :min="0.5" :max="0.85">
          <!-- 中间区域 - 主要内容 -->
          <template #1>
            <GalleryViewer />
          </template>

          <!-- 右侧区域 - 详情面板 -->
          <template #2>
            <GalleryDetails v-if="isDetailsOpen" />
          </template>
        </Split>
      </template>
    </Split>
  </div>
</template>

<script setup lang="ts">
import { useGalleryData } from '../composables'
import GalleryToolbar from '../components/GalleryToolbar.vue'
import GalleryContent from '../components/GalleryContent.vue'

const galleryData = useGalleryData()
</script>

<template>
  <div class="flex h-full flex-col bg-background">
    <!-- 工具栏 -->
    <GalleryToolbar />

    <!-- 加载状态 -->
    <div v-if="galleryData.isInitialLoading.value" class="flex flex-1 items-center justify-center">
      <div class="text-center">
        <div class="mb-4 text-lg">🔄 正在加载资产...</div>
        <div class="text-sm text-muted-foreground">请稍候</div>
      </div>
    </div>

    <!-- 错误状态 -->
    <div v-else-if="galleryData.error.value" class="flex flex-1 items-center justify-center">
      <div class="text-center">
        <div class="mb-4 text-lg text-red-500">❌ {{ galleryData.error.value }}</div>
        <button @click="galleryData.reload()" class="text-blue-500 hover:underline">
          点击重试
        </button>
      </div>
    </div>

    <!-- 空状态 -->
    <div
      v-else-if="galleryData.assets.value.length === 0"
      class="flex flex-1 items-center justify-center"
    >
      <div class="text-center text-muted-foreground">
        <div class="mb-4 text-4xl">🖼️</div>
        <div class="mb-2 text-lg">暂无资产</div>
        <div class="text-sm">
          <div>请检查后端连接或添加资产目录</div>
          <button @click="galleryData.reload()" class="mt-2 text-blue-500 hover:underline">
            重新加载
          </button>
        </div>
      </div>
    </div>

    <!-- 内容区域 -->
    <div v-else class="flex-1 overflow-hidden">
      <GalleryContent />
    </div>
  </div>
</template>

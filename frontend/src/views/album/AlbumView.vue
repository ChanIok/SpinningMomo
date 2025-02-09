<script setup lang="ts">
import { ref, onMounted } from 'vue'
import { useRouter } from 'vue-router'
import { useMessage } from 'naive-ui'
import type { Album } from '@/types/album'
import { albumAPI } from '@/api/album'
import AlbumCard from '@/components/album/AlbumCard.vue'
import AlbumEditDialog from '@/components/album/AlbumEditDialog.vue'
import { useAlbumSelectionStore } from '@/stores/album-selection'
import { Close, TrashBin } from '@vicons/ionicons5'

const router = useRouter()
const message = useMessage()
const albumSelectionStore = useAlbumSelectionStore()

const albums = ref<Album[]>([])
const loading = ref(false)
const editingAlbum = ref<Album | null>(null)
const showEditDialog = ref(false)
const showDeleteConfirm = ref(false)

// 加载相册列表
async function loadAlbums() {
  try {
    loading.value = true
    albums.value = await albumAPI.getAlbums()
  } catch (error) {
    console.error('Failed to load albums:', error)
    message.error('加载相册列表失败')
  } finally {
    loading.value = false
  }
}

// 打开编辑对话框
function handleEdit(album: Album) {
  editingAlbum.value = album
  showEditDialog.value = true
}

// 保存相册修改
async function handleSave(data: { name: string; description?: string }) {
  if (!editingAlbum.value) return
  
  try {
    await albumAPI.updateAlbum(editingAlbum.value.id, data)
    message.success('相册更新成功')
    loadAlbums() // 重新加载列表
  } catch (error) {
    console.error('Failed to update album:', error)
    message.error('更新相册失败')
  }
}

// 查看相册详情
function handleAlbumClick(album: Album) {
  router.push(`/albums/${album.id}`)
}

// 处理删除确认
async function handleConfirmDelete() {
  try {
    // 批量删除选中的相册
    for (const id of albumSelectionStore.selectedIds) {
      await albumAPI.deleteAlbum(id)
    }
    message.success('删除成功')
    albumSelectionStore.exitSelectionMode()
    loadAlbums() // 重新加载列表
  } catch (error) {
    console.error('Failed to delete albums:', error)
    message.error('删除失败')
  }
}

onMounted(() => {
  loadAlbums()
})
</script>

<template>
  <div class="album-view">
    <div class="header">
      <h1>相册</h1>
    </div>
    
    <div v-if="loading" class="loading-state">
      加载中...
    </div>
    
    <div v-else-if="albums.length === 0" class="empty-state">
      暂无相册
    </div>
    
    <div v-else class="album-grid">
      <album-card
        v-for="album in albums"
        :key="album.id"
        :album="album"
        @click="handleAlbumClick(album)"
        @edit="handleEdit(album)"
      />
    </div>

    <!-- 底部工具栏 -->
    <div
      v-if="albumSelectionStore.isSelectionMode"
      class="selection-toolbar"
    >
      <n-space justify="space-between" align="center">
        <span>已选择 {{ albumSelectionStore.selectedCount }} 个相册</span>
        <n-space>
          <n-button @click="albumSelectionStore.exitSelectionMode" secondary circle>
            <template #icon>
              <n-icon><Close /></n-icon>
            </template>
          </n-button>
          <n-button
            type="error"
            :disabled="albumSelectionStore.selectedCount === 0"
            @click="showDeleteConfirm = true"
          >
            <template #icon>
              <n-icon><TrashBin /></n-icon>
            </template>
            删除相册
          </n-button>
        </n-space>
      </n-space>
    </div>

    <!-- 删除确认对话框 -->
    <n-modal
      v-model:show="showDeleteConfirm"
      preset="dialog"
      type="warning"
      title="确认删除"
      content="确定要删除选中的相册吗？此操作不可恢复。"
      positive-text="确认"
      negative-text="取消"
      @positive-click="handleConfirmDelete"
    />

    <album-edit-dialog
      v-if="editingAlbum"
      v-model:show="showEditDialog"
      :album="editingAlbum"
      @save="handleSave"
    />
  </div>
</template>

<style scoped>
.album-view {
  padding: 24px;
  max-width: 1400px;
  margin: 0 auto;
}

.header {
  margin-bottom: 24px;
}

.header h1 {
  font-size: 2em;
  font-weight: 600;
  color: #333;
  margin: 0;
}

.album-grid {
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(240px, 1fr));
  gap: 20px;
}

.loading-state,
.empty-state {
  text-align: center;
  padding: 48px;
  color: #666;
  font-size: 1.1em;
}

.selection-toolbar {
  position: fixed;
  bottom: 0;
  left: 0;
  right: 0;
  background: rgba(255, 255, 255, 0.95);
  backdrop-filter: blur(10px);
  padding: 12px 16px;
  box-shadow: 0 -2px 12px rgba(0, 0, 0, 0.08);
  z-index: 100;
  animation: slideUp 0.3s ease;
}

@keyframes slideUp {
  from {
    transform: translateY(100%);
  }
  to {
    transform: translateY(0);
  }
}
</style> 
<script setup lang="ts">
import { ref, onMounted, computed, watch } from 'vue'
import { NModal, NCard, NInput, NButton, NList, NListItem, NEmpty, NSpace, useMessage } from 'naive-ui'
import type { Album } from '@/types/album'
import { albumAPI } from '@/api/album'

const props = defineProps<{
  show: boolean
  selectedCount: number
}>()

const emit = defineEmits<{
  (e: 'update:show', value: boolean): void
  (e: 'select', albumId: number): void
}>()

const albums = ref<Album[]>([])
const newAlbumName = ref('')
const loading = ref(false)
const message = useMessage()

// 使用计算属性处理show属性
const dialogVisible = computed({
  get: () => props.show,
  set: (value) => emit('update:show', value)
})

// 获取相册列表
async function fetchAlbums() {
  try {
    loading.value = true
    albums.value = await albumAPI.getAlbums()
  } catch (error) {
    console.error('Failed to fetch albums:', error)
    message.error('获取相册列表失败')
  } finally {
    loading.value = false
  }
}

// 创建新相册
async function handleCreateAlbum() {
  if (!newAlbumName.value.trim()) return
  
  try {
    loading.value = true
    const album = await albumAPI.createAlbum({
      name: newAlbumName.value.trim()
    })
    
    albums.value.unshift(album)
    newAlbumName.value = ''
    message.success('相册创建成功')
    emit('select', album.id)
  } catch (error) {
    console.error('Failed to create album:', error)
    message.error('创建相册失败')
  } finally {
    loading.value = false
  }
}

// 选择相册
function handleSelectAlbum(albumId: number) {
  emit('select', albumId)
  emit('update:show', false)
}

// 关闭弹窗
function handleClose() {
  emit('update:show', false)
}

// 监听show属性变化，当打开弹窗时获取相册列表
watch(() => props.show, (newVal) => {
  if (newVal) {
    fetchAlbums()
  }
})
</script>

<template>
  <n-modal
    v-model:show="dialogVisible"
    preset="card"
    title="选择相册"
    :mask-closable="false"
    @close="handleClose"
  >
    <n-space vertical>
      <!-- 快速创建相册 -->
      <n-card title="创建新相册" size="small">
        <n-space vertical>
          <n-input
            v-model:value="newAlbumName"
            placeholder="输入相册名称"
            @keyup.enter="handleCreateAlbum"
          />
          <n-button
            type="primary"
            block
            :disabled="!newAlbumName.trim()"
            @click="handleCreateAlbum"
          >
            创建并添加
          </n-button>
        </n-space>
      </n-card>

      <!-- 现有相册列表 -->
      <n-card title="现有相册" size="small">
        <template v-if="albums.length">
          <n-list hoverable clickable>
            <n-list-item
              v-for="album in albums"
              :key="album.id"
              @click="handleSelectAlbum(album.id)"
            >
              <n-space justify="space-between">
                <span>{{ album.name }}</span>
                <span class="text-gray-500 text-sm">
                  {{ album.description || '无描述' }}
                </span>
              </n-space>
            </n-list-item>
          </n-list>
        </template>
        <template v-else>
          <n-empty description="暂无相册" />
        </template>
      </n-card>

      <div class="text-center text-gray-500">
        将添加 {{ selectedCount }} 张照片到所选相册
      </div>
    </n-space>
  </n-modal>
</template>

<style scoped>
.text-center {
  text-align: center;
}

.text-gray-500 {
  color: #6b7280;
}

.text-sm {
  font-size: 0.875rem;
}
</style> 
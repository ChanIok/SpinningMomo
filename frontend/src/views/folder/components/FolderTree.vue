<script setup lang="ts">
import { ref, onMounted } from 'vue'
import { NTree } from 'naive-ui'
import type { TreeOption } from 'naive-ui'
import { useFolderStore } from '@/stores/folder'

const emit = defineEmits<{
  (e: 'select', folderId: string, relativePath: string): void
}>()

const folderStore = useFolderStore()
const treeData = ref<TreeOption[]>([])

onMounted(async () => {
  await folderStore.loadFolderTree()
  treeData.value = convertToTreeOptions(folderStore.folderTree)
})

const handleSelect = (keys: string[]) => {
  if (keys.length > 0) {
    const key = keys[0]
    const [folderId, relativePath] = key.split('::')
    emit('select', folderId, relativePath || '')
  }
}

// 转换文件夹树数据为Tree组件所需格式
const convertToTreeOptions = (nodes: any[]): TreeOption[] => {
  return nodes.map(node => ({
    key: `${node.folder_id}::${node.relative_path}`,
    label: node.name,
    children: node.children ? convertToTreeOptions(node.children) : undefined,
    prefix: () => `(${node.photo_count})`
  }))
}
</script>

<template>
  <n-tree
    block-line
    :data="treeData"
    @update:selected-keys="handleSelect"
  />
</template>
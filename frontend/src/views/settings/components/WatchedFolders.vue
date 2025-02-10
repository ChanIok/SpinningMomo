<script setup lang="ts">
import { ref, h } from 'vue'
import type { WatchedFolder } from '@/types/settings'
import { useSettingsStore } from '@/stores'
import { settingsAPI } from '@/api/settings'
import { 
    NSpace, 
    NButton, 
    NDataTable, 
    NPopconfirm,
    NModal,
    NForm,
    NFormItem,
    NInput,
    NSwitch,
    useMessage,
    useDialog
} from 'naive-ui'
import type { DataTableColumns } from 'naive-ui'

const props = defineProps<{
    folders: WatchedFolder[]
}>()

const settingsStore = useSettingsStore()
const message = useMessage()
const dialog = useDialog()

// 表格列定义
const columns: DataTableColumns<WatchedFolder> = [
    {
        title: '路径',
        key: 'path',
        width: 300
    },
    {
        title: '包含子文件夹',
        key: 'include_subfolders',
        width: 120,
        render(row) {
            return row.include_subfolders ? '是' : '否'
        }
    },
    {
        title: '最后扫描时间',
        key: 'last_scan',
        width: 180
    },
    {
        title: '操作',
        key: 'actions',
        width: 100,
        render(row) {
            return h(
                NPopconfirm,
                {
                    onPositiveClick: () => handleDelete(row.path)
                },
                {
                    default: () => '确定要删除这个文件夹吗？',
                    trigger: () => h(
                        NButton,
                        {
                            text: true,
                            type: 'error',
                            size: 'small'
                        },
                        { default: () => '删除' }
                    )
                }
            )
        }
    }
]

// 新文件夹表单
const showAddModal = ref(false)
const newFolder = ref<WatchedFolder>({
    path: '',
    include_subfolders: true,
    last_scan: ''
})

// 删除文件夹
async function handleDelete(path: string) {
    console.log('Deleting folder, path:', path);
    
    const result = await settingsStore.removeWatchedFolder(path)
    if (result.success) {
        message.success('文件夹已删除')
    } else {
        message.error(result.error || '删除文件夹失败')
        // Add detailed error information
        console.error('Failed to delete folder:', path, result);
    }
}

// 添加文件夹
async function handleAdd() {
    if (!newFolder.value.path) {
        message.warning('请输入文件夹路径')
        return
    }

    const result = await settingsStore.addWatchedFolder(newFolder.value)
    if (result.success) {
        message.success('文件夹已添加')
        showAddModal.value = false
        // 重置表单
        newFolder.value = {
            path: '',
            include_subfolders: true,
            last_scan: ''
        }
    } else {
        message.error(result.error || '添加文件夹失败')
    }
}

// 选择文件夹
async function handleSelectFolder() {
    try {
        const result = await settingsAPI.selectFolder();
        console.log(result);
        if (result.isAccessible) {
            newFolder.value.path = result.path;
        } else {
            message.warning('所选文件夹无法访问，请选择其他文件夹');
        }
    } catch (error) {
        dialog.warning({
            title: '提示',
            content: '选择文件夹失败，请手动输入路径',
            positiveText: '确定'
        });
    }
}
</script>

<template>
    <div class="watched-folders">
        <n-space vertical>
            <n-space>
                <n-button type="primary" @click="showAddModal = true">
                    添加文件夹
                </n-button>
            </n-space>

            <n-data-table
                :columns="columns"
                :data="folders"
                :bordered="false"
                :single-line="false"
            />
        </n-space>

        <!-- 添加文件夹对话框 -->
        <n-modal
            v-model:show="showAddModal"
            title="添加监视文件夹"
            preset="dialog"
            positive-text="确定"
            negative-text="取消"
            @positive-click="handleAdd"
        >
            <n-form>
                <n-form-item label="文件夹路径">
                    <n-space>
                        <n-input v-model:value="newFolder.path" placeholder="请输入文件夹路径" />
                        <n-button @click="handleSelectFolder">
                            选择文件夹
                        </n-button>
                    </n-space>
                </n-form-item>

                <n-form-item label="包含子文件夹">
                    <n-switch v-model:value="newFolder.include_subfolders" />
                </n-form-item>
            </n-form>
        </n-modal>
    </div>
</template>

<style scoped>
.watched-folders {
    width: 100%;
}
</style> 
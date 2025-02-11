<script setup lang="ts">
import { ref, h, onMounted } from 'vue'
import { folderAPI } from '@/api/folder'
import type { ProcessingProgress } from '@/types/folder'
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
    NTag,
    useMessage,
    useDialog
} from 'naive-ui'
import type { DataTableColumns } from 'naive-ui'

const message = useMessage()
const dialog = useDialog()

interface FolderInfo {
    path: string
    progress: ProcessingProgress
}

// 文件夹列表
const folders = ref<FolderInfo[]>([])

// 表格列定义
const columns: DataTableColumns<FolderInfo> = [
    {
        title: '路径',
        key: 'path',
        width: 300
    },
    {
        title: '状态',
        key: 'status',
        width: 120,
        render(row: FolderInfo) {
            const status = row.progress.status
            const type = {
                'pending': 'default',
                'processing': 'info',
                'completed': 'success',
                'failed': 'error'
            }[status] as 'default' | 'info' | 'success' | 'error'
            
            const text = {
                'pending': '等待处理',
                'processing': '处理中',
                'completed': '已完成',
                'failed': '处理失败'
            }[status]
            
            return h(NTag, { type }, { default: () => text })
        }
    },
    {
        title: '进度',
        key: 'progress',
        width: 180,
        render(row: FolderInfo) {
            if (row.progress.total_files === 0) return '暂无文件'
            return `${row.progress.processed_files}/${row.progress.total_files}`
        }
    },
    {
        title: '操作',
        key: 'actions',
        width: 200,
        render(row: FolderInfo) {
            return h(NSpace, {}, {
                default: () => [
                    h(NPopconfirm, {
                        onPositiveClick: () => handleDelete(row.path)
                    }, {
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
                    }),
                    h(
                        NButton,
                        {
                            text: true,
                            type: 'primary',
                            size: 'small',
                            onClick: () => handleReprocess(row.path)
                        },
                        { default: () => '重新处理' }
                    )
                ]
            })
        }
    }
]

// 新文件夹对话框
const showAddModal = ref(false)
const newFolderPath = ref('')

// 加载文件夹列表
async function loadFolders() {
    try {
        folders.value = await folderAPI.getAllFolders()
    } catch (error) {
        message.error('加载文件夹列表失败')
    }
}

// 删除文件夹
async function handleDelete(path: string) {
    try {
        await folderAPI.removeFolder(path)
        message.success('文件夹已删除')
        loadFolders()
    } catch (error) {
        message.error('删除文件夹失败')
    }
}

// 重新处理文件夹
async function handleReprocess(path: string) {
    try {
        await folderAPI.reprocessFolder(path)
        message.success('开始重新处理文件夹')
        loadFolders()
    } catch (error) {
        message.error('重新处理文件夹失败')
    }
}

// 选择文件夹
async function handleSelectFolder() {
    try {
        const result = await folderAPI.selectFolder()
        if (result.isAccessible) {
            newFolderPath.value = result.path
        } else {
            message.warning('所选文件夹无法访问，请选择其他文件夹')
        }
    } catch (error) {
        message.error('选择文件夹失败')
    }
}

// 添加文件夹
async function handleAdd() {
    if (!newFolderPath.value) {
        message.warning('请选择文件夹')
        return
    }

    try {
        await folderAPI.addFolder(newFolderPath.value)
        message.success('文件夹已添加')
        showAddModal.value = false
        newFolderPath.value = ''
        loadFolders()
    } catch (error) {
        message.error('添加文件夹失败')
    }
}

// 组件加载时获取文件夹列表
onMounted(() => {
    loadFolders()
})
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
                        <n-input v-model:value="newFolderPath" placeholder="请选择文件夹" readonly />
                        <n-button @click="handleSelectFolder">
                            选择文件夹
                        </n-button>
                    </n-space>
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
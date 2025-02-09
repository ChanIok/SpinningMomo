<script setup lang="ts">
import { computed } from 'vue'
import type { InterfaceSettings } from '@/types/settings'
import { 
    NForm, 
    NFormItem, 
    NSelect,
    NInputNumber,
    NSpace
} from 'naive-ui'

const props = defineProps<{
    settings: InterfaceSettings
}>()

const emit = defineEmits<{
    (e: 'update:settings', value: InterfaceSettings): void
}>()

// 主题选项
const themeOptions = [
    { label: '浅色', value: 'light' },
    { label: '深色', value: 'dark' }
]

// 语言选项
const languageOptions = [
    { label: '简体中文', value: 'zh-CN' },
    { label: '繁体中文', value: 'zh-TW' },
    { label: 'English', value: 'en-US' }
]

// 视图模式选项
const viewModeOptions = [
    { label: '网格', value: 'grid' },
    { label: '列表', value: 'list' }
]

// 计算属性：用于双向绑定
const theme = computed({
    get: () => props.settings.theme,
    set: (value) => {
        emit('update:settings', {
            ...props.settings,
            theme: value as 'light' | 'dark'
        })
    }
})

const language = computed({
    get: () => props.settings.language,
    set: (value) => {
        emit('update:settings', {
            ...props.settings,
            language: value
        })
    }
})

const defaultViewMode = computed({
    get: () => props.settings.default_view_mode,
    set: (value) => {
        emit('update:settings', {
            ...props.settings,
            default_view_mode: value as 'grid' | 'list'
        })
    }
})

const gridColumns = computed({
    get: () => props.settings.grid_columns,
    set: (value) => {
        emit('update:settings', {
            ...props.settings,
            grid_columns: value
        })
    }
})
</script>

<template>
    <div class="interface-settings">
        <n-form label-placement="left" label-width="120">
            <n-form-item label="主题">
                <n-select
                    v-model:value="theme"
                    :options="themeOptions"
                    style="width: 200px"
                />
            </n-form-item>

            <n-form-item label="语言">
                <n-select
                    v-model:value="language"
                    :options="languageOptions"
                    style="width: 200px"
                />
            </n-form-item>

            <n-form-item label="默认视图模式">
                <n-select
                    v-model:value="defaultViewMode"
                    :options="viewModeOptions"
                    style="width: 200px"
                />
            </n-form-item>

            <n-form-item label="网格列数">
                <n-space align="center">
                    <n-input-number
                        v-model:value="gridColumns"
                        :min="1"
                        :max="12"
                        style="width: 200px"
                    />
                    <span>列</span>
                </n-space>
            </n-form-item>
        </n-form>
    </div>
</template>

<style scoped>
.interface-settings {
    width: 100%;
    max-width: 800px;
}
</style> 
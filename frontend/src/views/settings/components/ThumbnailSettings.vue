<script setup lang="ts">
import { computed } from 'vue'
import type { ThumbnailSettings } from '@/types/settings'
import { 
    NForm, 
    NFormItem, 
    NInputNumber, 
    NSlider,
    NInput,
    NSpace
} from 'naive-ui'

const props = defineProps<{
    settings: ThumbnailSettings
}>()

const emit = defineEmits<{
    (e: 'update:settings', value: ThumbnailSettings): void
}>()

// 计算属性：用于双向绑定
const thumbnailWidth = computed({
    get: () => props.settings.size.width,
    set: (value) => {
        emit('update:settings', {
            ...props.settings,
            size: {
                ...props.settings.size,
                width: value
            }
        })
    }
})

const thumbnailHeight = computed({
    get: () => props.settings.size.height,
    set: (value) => {
        emit('update:settings', {
            ...props.settings,
            size: {
                ...props.settings.size,
                height: value
            }
        })
    }
})

const quality = computed({
    get: () => props.settings.quality,
    set: (value) => {
        emit('update:settings', {
            ...props.settings,
            quality: value
        })
    }
})

const storageLocation = computed({
    get: () => props.settings.storage_location,
    set: (value) => {
        emit('update:settings', {
            ...props.settings,
            storage_location: value
        })
    }
})
</script>

<template>
    <div class="thumbnail-settings">
        <n-form label-placement="left" label-width="120">
            <n-form-item label="缩略图宽度">
                <n-space align="center">
                    <n-slider
                        v-model:value="thumbnailWidth"
                        :min="100"
                        :max="500"
                        :step="10"
                        style="width: 200px"
                    />
                    <n-input-number
                        v-model:value="thumbnailWidth"
                        :min="100"
                        :max="500"
                        size="small"
                    />
                    <span>像素</span>
                </n-space>
            </n-form-item>

            <n-form-item label="缩略图高度">
                <n-space align="center">
                    <n-slider
                        v-model:value="thumbnailHeight"
                        :min="100"
                        :max="500"
                        :step="10"
                        style="width: 200px"
                    />
                    <n-input-number
                        v-model:value="thumbnailHeight"
                        :min="100"
                        :max="500"
                        size="small"
                    />
                    <span>像素</span>
                </n-space>
            </n-form-item>

            <n-form-item label="图片质量">
                <n-space align="center">
                    <n-slider
                        v-model:value="quality"
                        :min="1"
                        :max="100"
                        :step="1"
                        style="width: 200px"
                    />
                    <n-input-number
                        v-model:value="quality"
                        :min="1"
                        :max="100"
                        size="small"
                    />
                    <span>%</span>
                </n-space>
            </n-form-item>

            <n-form-item label="存储位置">
                <n-input v-model:value="storageLocation" placeholder="请输入缩略图存储路径" />
            </n-form-item>
        </n-form>
    </div>
</template>

<style scoped>
.thumbnail-settings {
    width: 100%;
    max-width: 800px;
}
</style> 
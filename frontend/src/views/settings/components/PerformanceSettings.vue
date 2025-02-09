<script setup lang="ts">
import { computed } from 'vue'
import type { PerformanceSettings } from '@/types/settings'
import { 
    NForm, 
    NFormItem, 
    NInputNumber,
    NSwitch,
    NSpace
} from 'naive-ui'

const props = defineProps<{
    settings: PerformanceSettings
}>()

const emit = defineEmits<{
    (e: 'update:settings', value: PerformanceSettings): void
}>()

// 计算属性：用于双向绑定
const scanThreads = computed({
    get: () => props.settings.scan_threads,
    set: (value) => {
        emit('update:settings', {
            ...props.settings,
            scan_threads: value
        })
    }
})

const cacheSize = computed({
    get: () => props.settings.cache_size,
    set: (value) => {
        emit('update:settings', {
            ...props.settings,
            cache_size: value
        })
    }
})

const preloadImages = computed({
    get: () => props.settings.preload_images,
    set: (value) => {
        emit('update:settings', {
            ...props.settings,
            preload_images: value
        })
    }
})
</script>

<template>
    <div class="performance-settings">
        <n-form label-placement="left" label-width="120">
            <n-form-item label="扫描线程数">
                <n-space align="center">
                    <n-input-number
                        v-model:value="scanThreads"
                        :min="1"
                        :max="32"
                        style="width: 200px"
                    />
                    <span>个线程</span>
                </n-space>
            </n-form-item>

            <n-form-item label="缓存大小">
                <n-space align="center">
                    <n-input-number
                        v-model:value="cacheSize"
                        :min="100"
                        :max="10000"
                        :step="100"
                        style="width: 200px"
                    />
                    <span>张图片</span>
                </n-space>
            </n-form-item>

            <n-form-item label="预加载图片">
                <n-switch v-model:value="preloadImages" />
            </n-form-item>
        </n-form>
    </div>
</template>

<style scoped>
.performance-settings {
    width: 100%;
    max-width: 800px;
}
</style> 
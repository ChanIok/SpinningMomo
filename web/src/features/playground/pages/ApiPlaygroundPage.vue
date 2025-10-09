<script setup lang="ts">
import { onMounted, ref } from 'vue'
// import { Splitter, SplitterContent, SplitterPanel } from '@/components/ui/split' // 不使用split组件
import ApiMethodList from '../components/ApiMethodList.vue'
import ApiTestPanel from '../components/ApiTestPanel.vue'
import { useApiMethods } from '../composables/useApiMethods'
import type { ApiMethod, ApiTestResponse } from '../types'

const { methods, loading, error, fetchMethods } = useApiMethods()

const selectedMethod = ref<ApiMethod | null>(null)
const lastResponse = ref<ApiTestResponse | null>(null)

// 页面加载时获取API方法列表
onMounted(() => {
  fetchMethods()
})

// 选择方法
const handleSelectMethod = (method: ApiMethod) => {
  selectedMethod.value = method
}

// 接收响应
const handleResponse = (response: ApiTestResponse) => {
  lastResponse.value = response
}

// 刷新方法列表
const handleRefresh = () => {
  fetchMethods()
}
</script>

<template>
  <div class="flex h-full flex-col bg-background">
    <!-- 页面标题 -->
    <div class="border-b p-4">
      <h1 class="text-2xl font-bold">API 测试工具</h1>
      <p class="mt-1 text-sm text-muted-foreground">测试后端API方法，查看响应结果</p>
    </div>

    <!-- 错误提示 -->
    <div v-if="error" class="border-l-4 border-red-500 bg-red-50 p-4 text-red-700">
      <div class="flex">
        <div class="flex-shrink-0">
          <svg class="h-5 w-5 text-red-400" viewBox="0 0 20 20" fill="currentColor">
            <path
              fill-rule="evenodd"
              d="M10 18a8 8 0 100-16 8 8 0 000 16zM8.707 7.293a1 1 0 00-1.414 1.414L8.586 10l-1.293 1.293a1 1 0 101.414 1.414L10 11.414l1.293 1.293a1 1 0 001.414-1.414L11.414 10l1.293-1.293a1 1 0 00-1.414-1.414L10 8.586 8.707 7.293z"
              clip-rule="evenodd"
            />
          </svg>
        </div>
        <div class="ml-3">
          <h3 class="text-sm font-medium text-red-800">获取API方法失败</h3>
          <div class="mt-2 text-sm text-red-700">
            {{ error }}
          </div>
          <div class="mt-4">
            <div class="-mx-2 -my-1.5 flex">
              <button
                type="button"
                @click="handleRefresh"
                class="px-3 py-2 text-sm font-medium text-red-800 hover:bg-red-100 focus:ring-2 focus:ring-red-600 focus:ring-offset-2 focus:ring-offset-red-50 focus:outline-none"
              >
                重试
              </button>
            </div>
          </div>
        </div>
      </div>
    </div>

    <!-- 主要内容 -->
    <div class="flex-1 overflow-y-auto p-4">
      <div class="flex h-full gap-4">
        <!-- 左侧方法列表 -->
        <div class="w-72 overflow-hidden rounded-lg border">
          <ApiMethodList
            :methods="methods"
            :loading="loading"
            :selected-method="selectedMethod?.name"
            @select="handleSelectMethod"
            @refresh="handleRefresh"
          />
        </div>

        <!-- 右侧测试面板 -->
        <div class="flex-1 overflow-hidden rounded-lg border">
          <ApiTestPanel :method="selectedMethod" @response="handleResponse" />
        </div>
      </div>
    </div>
  </div>
</template>

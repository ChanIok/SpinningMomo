<script setup lang="ts">
import { computed, watch } from 'vue'
import { useRoute, useRouter } from 'vue-router'
import { Tabs, TabsContent, TabsList, TabsTrigger } from '@/components/ui/tabs'
import ApiPlaygroundPage from './ApiPlaygroundPage.vue'
import IntegrationTestPage from './IntegrationTestPage.vue'

const route = useRoute()
const router = useRouter()

// 从 query 参数获取当前 tab，默认为 'api'
const activeTab = computed({
  get: () => (route.query.tab as string) || 'api',
  set: (value: string) => {
    router.replace({
      query: { ...route.query, tab: value },
    })
  },
})

// 监听 tab 变化，更新页面标题
watch(
  activeTab,
  (tab) => {
    document.title = tab === 'api' ? 'API 测试工具' : '集成测试工具'
  },
  { immediate: true }
)
</script>

<template>
  <div class="flex h-full flex-col bg-background">
    <!-- 标题和 Tabs -->
    <div class="flex items-center gap-6 border-b p-4">
      <Tabs v-model="activeTab" class="flex-shrink-0">
        <TabsList>
          <TabsTrigger value="api">API 测试</TabsTrigger>
          <TabsTrigger value="integration">集成测试</TabsTrigger>
        </TabsList>
      </Tabs>

      <div class="flex-1">
        <p class="text-muted-foreground">开发者测试工具集</p>
      </div>
    </div>

    <!-- Tab 内容 -->
    <Tabs v-model="activeTab" class="flex flex-1 flex-col overflow-hidden">
      <div class="hidden"></div>

      <TabsContent
        value="api"
        class="flex-1 overflow-hidden data-[state=active]:flex data-[state=active]:flex-col"
      >
        <ApiPlaygroundPage />
      </TabsContent>

      <TabsContent
        value="integration"
        class="flex-1 overflow-hidden data-[state=active]:flex data-[state=active]:flex-col"
      >
        <IntegrationTestPage />
      </TabsContent>
    </Tabs>
  </div>
</template>

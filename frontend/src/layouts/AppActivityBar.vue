<template>
  <div id="app-activity-bar" class="min-w-16 flex flex-col">
    <!-- 菜单项 -->
    <ul class="flex-1">
      <!-- 分隔线处理 -->
      <div v-for="(item, index) in menuItems" :key="index">
        <!-- 分隔线 -->
        <div v-if="item.type === 'divider'" class="mx-2 my-3 border-t border-gray-200"></div>

        <!-- 菜单项 -->
        <li
          v-else
          @click="item.key && handleMenuSelect(item.key)"
          class="relative h-12 mx-2 mb-2 rounded-md cursor-pointer flex justify-center items-center transition-colors group"
          :class="{
            'bg-gray-200': activeKey === item.key,
            'hover:bg-gray-100': item.key !== undefined,
          }"
        >
          <!-- 图标 -->
          <component
            v-if="item.icon"
            :is="item.icon"
            class="w-6 h-6 text-gray-600"
            :class="{ 'text-[#ff9f4f]': activeKey === item.key }"
          />

          <!-- 悬停时显示的标签文本 -->
          <div
            v-if="item.label && item.key"
            class="absolute left-[60px] px-3 py-2 bg-gray-800 text-white text-sm rounded-md opacity-0 invisible group-hover:opacity-100 group-hover:visible transition-all duration-200 whitespace-nowrap z-50 transform -translate-y-1/2 top-1/2"
          >
            {{ item.label }}
            <!-- 小三角指示箭头 -->
            <div
              class="absolute w-2 h-2 bg-gray-800 transform rotate-45 -left-1 top-1/2 -translate-y-1/2"
            ></div>
          </div>
        </li>
      </div>
    </ul>
  </div>
</template>

<script setup lang="ts">
import { ref, watch } from 'vue'
import { useRouter } from 'vue-router'
import {
  ImageOutline as ImageIcon,
  ImagesOutline as AlbumIcon,
  CalendarOutline as CalendarIcon,
  LocationOutline as LocationIcon,
  FolderOutline as FileIcon,
  SettingsOutline as SettingsIcon,
} from '@vicons/ionicons5'

const router = useRouter()
const activeKey = ref<string | null>(null)

// 初始化activeKey为当前路由路径
// 例如，如果当前路径是/screenshots，则activeKey应为'screenshots'
const currentPath = router.currentRoute.value.path
const pathSegment = currentPath.split('/')[1] // 获取路径的第一部分
if (pathSegment) {
  activeKey.value = pathSegment
}

// 菜单项定义
interface MenuItem {
  label?: string
  key?: string
  icon?: any
  type?: string
}

const menuItems: MenuItem[] = [
  {
    label: '截图',
    key: 'screenshots',
    icon: ImageIcon,
  },
  {
    label: '相册',
    key: 'albums',
    icon: AlbumIcon,
  },
  {
    label: '日历',
    key: 'calendar',
    icon: CalendarIcon,
  },
  {
    label: '地点',
    key: 'places',
    icon: LocationIcon,
  },
  {
    label: '文件夹',
    key: 'folders',
    icon: FileIcon,
  },
  {
    type: 'divider',
  },
  {
    label: '设置',
    key: 'settings',
    icon: SettingsIcon,
  },
]

function handleMenuSelect(key: string) {
  router.push(`/${key}`)
}

// 监听路由变化，更新activeKey
watch(
  () => router.currentRoute.value.path,
  newPath => {
    const pathSegment = newPath.split('/')[1]
    if (pathSegment) {
      activeKey.value = pathSegment
    }
  }
)
</script>

<style scoped>
/* 使用TailwindCSS类，无需额外样式 */
</style>
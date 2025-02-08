<script setup lang="ts">
import { h, ref } from 'vue'
import { useRouter } from 'vue-router'
import {
  NLayoutSider,
  NMenu,
  NIcon,
  type MenuOption
} from 'naive-ui'
import {
  ImageOutline as ImageIcon,
  ImagesOutline as AlbumIcon,
  CalendarOutline as CalendarIcon,
  LocationOutline as LocationIcon,
  BookmarkOutline as TagIcon,
  FolderOutline as FileIcon
} from '@vicons/ionicons5'

const router = useRouter()
const activeKey = ref<string | null>(null)
const collapsed = ref(false)

function renderIcon(icon: any) {
  return () => h(NIcon, null, { default: () => h(icon) })
}

const menuOptions: MenuOption[] = [
  {
    label: '截图',
    key: 'screenshots',
    icon: renderIcon(ImageIcon)
  },
  {
    label: '相册',
    key: 'albums',
    icon: renderIcon(AlbumIcon)
  },
  {
    label: '日历',
    key: 'calendar',
    icon: renderIcon(CalendarIcon)
  },
  {
    label: '地点',
    key: 'places',
    icon: renderIcon(LocationIcon)
  },
  {
    label: '标签',
    key: 'tags',
    icon: renderIcon(TagIcon)
  },
  {
    label: '文件夹',
    key: 'folders',
    icon: renderIcon(FileIcon)
  }
]

function handleMenuSelect(key: string) {
  router.push(`/${key}`)
}
</script>

<template>
  <n-layout-sider
    bordered
    collapse-mode="width"
    :collapsed-width="64"
    :width="240"
    show-trigger
    class="app-sidebar"
    @collapse="collapsed = true"
    @expand="collapsed = false"
  >
    <n-menu
      v-model:value="activeKey"
      :options="menuOptions"
      :collapsed="collapsed"
      :collapsed-width="64"
      :collapsed-icon-size="22"
      @update:value="handleMenuSelect"
    />
  </n-layout-sider>
</template>

<style scoped>
.app-sidebar {
  height: 100vh;
  background-color: var(--n-color);
  border-right: 1px solid var(--n-border-color);
}

.sidebar-header {
  height: 64px;
  display: flex;
  align-items: center;
  padding: 0 20px;
  border-bottom: 1px solid var(--n-border-color);
}

.app-title {
  margin: 0;
  font-size: 1.2rem;
  font-weight: 400;
}
</style> 
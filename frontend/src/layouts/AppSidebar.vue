<template>
  <n-layout-sider
    collapse-mode="width"
    :collapsed-width="72"
    :width="200"
    show-trigger
    class="h-full rounded-xl m-4"
    :style="{ backgroundColor: 'rgb(252, 252, 252)' }"
    :collapsed="collapsed"
    @collapse="collapsed = true"
    @expand="collapsed = false"
  >
    <n-menu
      v-model:value="activeKey"
      :options="menuOptions"
      :collapsed="collapsed"
      :collapsed-width="66"
      :collapsed-icon-size="24"
      @update:value="handleMenuSelect"
      class="custom-menu"
    />
  </n-layout-sider>
</template>

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
  FolderOutline as FileIcon,
  SettingsOutline as SettingsIcon
} from '@vicons/ionicons5'

const router = useRouter()
const activeKey = ref<string | null>(null)
// 默认为收缩状态
const collapsed = ref(true)

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
  },
  {
    type: 'divider'
  },
  {
    label: '设置',
    key: 'settings',
    icon: renderIcon(SettingsIcon)
  }
]

function handleMenuSelect(key: string) {
  router.push(`/${key}`)
}
</script>

<style scoped>
.custom-menu :deep(.n-menu-item) {
  height: 54px;
  padding: 4px;
  box-sizing: border-box;
}
</style>
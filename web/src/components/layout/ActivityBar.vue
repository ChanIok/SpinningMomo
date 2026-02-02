<script setup lang="ts">
import { computed } from 'vue'
import { useRoute, useRouter } from 'vue-router'
import { Home, Images, Settings, Info } from 'lucide-vue-next'
import {
  Sidebar,
  SidebarContent,
  SidebarGroup,
  SidebarGroupContent,
  SidebarMenu,
  SidebarMenuItem,
  SidebarMenuButton,
} from '@/components/ui/sidebar'

interface MenuItem {
  key: string
  title: string
  icon: any
}

const baseMenuItems: (MenuItem | { type: 'divider' })[] = [
  { title: '主页', key: 'home', icon: Home },
  { title: '图库', key: 'gallery', icon: Images },
  { title: '设置', key: 'settings', icon: Settings },
  { type: 'divider' },
  { title: '关于', key: 'about', icon: Info },
]

const menuItems = computed(() => {
  if (import.meta.env.PROD) {
    return baseMenuItems.filter((item) => {
      if ('key' in item) {
        return item.key !== 'gallery'
      }
      return true
    })
  }
  return baseMenuItems
})

const router = useRouter()
const route = useRoute()

const activeKey = computed(() => {
  const firstSegment = route.path.split('/')[1]
  return firstSegment || 'home'
})

const handleMenuSelect = (key: string) => {
  router.push(`/${key}`)
}
</script>

<template>
  <Sidebar collapsible="none" class="w-14 border-r border-panel pt-2">
    <SidebarContent>
      <SidebarGroup>
        <SidebarGroupContent class="px-0">
          <SidebarMenu>
            <template
              v-for="(item, index) in menuItems"
              :key="'key' in item ? item.key : `divider-${index}`"
            >
              <!-- Divider -->
              <div v-if="'type' in item && item.type === 'divider'" class="mx-2 my-3" />

              <!-- Menu Item -->
              <SidebarMenuItem v-else-if="'key' in item">
                <SidebarMenuButton
                  :tooltip="item.title"
                  :is-active="activeKey === item.key"
                  @click="handleMenuSelect(item.key)"
                  class="h-10 w-10 [&>svg]:mx-auto [&>svg]:h-5 [&>svg]:w-5"
                >
                  <component :is="item.icon" :stroke-width="1.8" />
                </SidebarMenuButton>
              </SidebarMenuItem>
            </template>
          </SidebarMenu>
        </SidebarGroupContent>
      </SidebarGroup>
    </SidebarContent>
  </Sidebar>
</template>

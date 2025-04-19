<script setup lang="ts">
import { NConfigProvider, NMessageProvider, NDialogProvider, GlobalThemeOverrides } from 'naive-ui'

// 定义主题颜色覆盖
// 可以根据需要自定义更多颜色和组件样式
const themeOverrides: GlobalThemeOverrides = {
  common: {
    primaryColor: '#ff9f4f',
    primaryColorHover: '#ffb06a',
    primaryColorPressed: '#f59440',
    primaryColorSuppl: '#ffc088',
    infoColor: '#ff9f4f',
    successColor: '#18a058',
    warningColor: '#f0a020',
    errorColor: '#d03050',
    borderRadius: '0.5rem',
  },
}
</script>

<template>
  <n-config-provider :theme-overrides="themeOverrides">
    <n-message-provider>
      <n-dialog-provider>
        <router-view />
      </n-dialog-provider>
    </n-message-provider>
  </n-config-provider>
</template>

<style>
html,
body {
  margin: 0;
  padding: 0;
  height: 100%;
  width: 100%;
  overflow: hidden;
  display: flex;
}

#app {
  height: 100%;
  width: 100%;
}
</style>

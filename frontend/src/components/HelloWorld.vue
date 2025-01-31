#pragma once
<script setup lang="ts">
import { ref, onMounted } from 'vue'
import axios from 'axios'

defineProps<{ msg: string }>()

const count = ref(0)
const albums = ref<any[]>([])
const loading = ref(false)
const error = ref('')

// 创建axios实例
const api = axios.create({
  baseURL: 'http://localhost:51206/api',
  timeout: 5000,
  headers: {
    'Content-Type': 'application/json'
  }
})

const fetchAlbums = async () => {
  loading.value = true
  error.value = ''
  try {
    const response = await api.get('/albums')
    albums.value = response.data
  } catch (e) {
    error.value = axios.isAxiosError(e) ? e.response?.data?.error || e.message : '未知错误'
  } finally {
    loading.value = false
  }
}

onMounted(() => {
  fetchAlbums()
})
</script>

<template>
  <h1>{{ msg }}</h1>

  <div class="card">
    <button type="button" @click="count++">count is {{ count }}</button>
  </div>

  <div class="albums">
    <h2>相册列表</h2>
    <button @click="fetchAlbums" :disabled="loading">刷新</button>
    
    <div v-if="loading">加载中...</div>
    <div v-else-if="error" class="error">{{ error }}</div>
    <div v-else-if="albums.length === 0">暂无相册</div>
    <ul v-else>
      <li v-for="album in albums" :key="album.id">
        {{ album.name }} - {{ album.description || '无描述' }}
      </li>
    </ul>
  </div>

  <p>
    Check out
    <a href="https://vuejs.org/guide/quick-start.html#local" target="_blank"
      >create-vue</a
    >, the official Vue + Vite starter
  </p>
  <p>
    Learn more about IDE Support for Vue in the
    <a
      href="https://vuejs.org/guide/scaling-up/tooling.html#ide-support"
      target="_blank"
      >Vue Docs Scaling up Guide</a
    >.
  </p>
  <p class="read-the-docs">Click on the Vite and Vue logos to learn more</p>
</template>

<style scoped>
.albums {
  margin-top: 2rem;
  padding: 1rem;
  border: 1px solid #ddd;
  border-radius: 8px;
}

.error {
  color: red;
  margin: 1rem 0;
}

ul {
  text-align: left;
  list-style: none;
  padding: 0;
}

li {
  padding: 0.5rem 0;
  border-bottom: 1px solid #eee;
}

button {
  margin: 1rem 0;
}

.read-the-docs {
  color: #888;
}
</style>

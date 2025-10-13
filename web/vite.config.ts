import { defineConfig } from 'vite'
import vue from '@vitejs/plugin-vue'
import tailwindcss from '@tailwindcss/vite'
import { analyzer } from 'vite-bundle-analyzer'
import { fileURLToPath, URL } from 'node:url'

// https://vite.dev/config/
export default defineConfig({
  plugins: [vue(), tailwindcss(), analyzer({ analyzerMode: 'static', fileName: '../stats' })],
  resolve: {
    alias: {
      '@': fileURLToPath(new URL('./src', import.meta.url)),
    },
  },
  server: {
    proxy: {
      '/rpc': {
        target: 'http://localhost:51206',
        changeOrigin: true,
        secure: false,
      },

      '/static': {
        target: 'http://localhost:51206',
        changeOrigin: true,
        secure: false,
      },
    },
  },
})

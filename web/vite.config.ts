import { defineConfig } from 'vite'
import vue from '@vitejs/plugin-vue'
import tailwindcss from '@tailwindcss/vite'
import { analyzer } from 'vite-bundle-analyzer'
import { fileURLToPath, URL } from 'node:url'

// https://vite.dev/config/
export default defineConfig(({ command, mode }) => ({
  plugins: [
    vue(),
    tailwindcss(),
    ...(mode === 'analyze' ? [analyzer({ analyzerMode: 'static', fileName: '../stats' })] : []),
  ],
  resolve: {
    alias: {
      '@': fileURLToPath(new URL('./src', import.meta.url)),
    },
  },
  esbuild: command === 'build' ? { pure: ['console.log', 'console.debug'] } : undefined,
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
      '/downloads': {
        target: 'http://localhost:51206',
        changeOrigin: true,
        secure: false,
      },
      '/sse': {
        target: 'http://localhost:51206',
        changeOrigin: true,
        secure: false,
      },
    },
  },
}))

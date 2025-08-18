import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react-swc'
import tailwindcss from '@tailwindcss/vite'
import { fileURLToPath, URL } from 'node:url'

// https://vite.dev/config/
export default defineConfig({
  plugins: [react(), tailwindcss()],
  resolve: {
    alias: {
      '@': fileURLToPath(new URL('./src', import.meta.url)),
    },
  },
  server: {
    proxy: {
      '/rpc': {
        target: 'http://localhost:51205',
        changeOrigin: true,
        secure: false,
      },
      '/sse': {
        target: 'http://localhost:51205',
        changeOrigin: true,
        secure: false,
      },
    },
  },
})

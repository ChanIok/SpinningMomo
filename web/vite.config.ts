import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react-swc'
import tailwindcss from '@tailwindcss/vite'
import { analyzer } from 'vite-bundle-analyzer'
import { fileURLToPath, URL } from 'node:url'

// https://vite.dev/config/
export default defineConfig({
  plugins: [react(), tailwindcss(), analyzer({ analyzerMode: 'static', fileName: '../stats' })],
  resolve: {
    alias: {
      '@': fileURLToPath(new URL('./src', import.meta.url)),
    },
  },
  build: {
    rollupOptions: {
      output: {
        manualChunks: {
          'react-vendor': ['react', 'react-dom'],
          'router-vendor': ['react-router'],
          'state-vendor': ['zustand'],
          'ui-vendor': [
            '@radix-ui/react-alert-dialog',
            '@radix-ui/react-dialog',
            '@radix-ui/react-label',
            '@radix-ui/react-scroll-area',
            '@radix-ui/react-select',
            '@radix-ui/react-separator',
            '@radix-ui/react-slider',
            '@radix-ui/react-slot',
            '@radix-ui/react-switch',
            '@radix-ui/react-tooltip',
          ],
          'utils-vendor': ['clsx', 'tailwind-merge', 'class-variance-authority'],
          vendor: ['lucide-react', 'next-themes', 'sonner'],
        },
      },
    },
  },
  server: {
    proxy: {
      '/rpc': {
        target: 'http://localhost:51206',
        changeOrigin: true,
        secure: false,
      },
      '/sse': {
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

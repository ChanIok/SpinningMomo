import { defineConfig } from 'vitepress'

export default defineConfig({
  title: 'SpinningMomo',
  description: '一个为《无限暖暖》提升摄影体验的窗口调整工具',

  head: [
    ['link', { rel: 'icon', href: '/logo.png' }],
    ['link', { rel: 'apple-touch-icon', href: '/logo.png' }],
  ],

  locales: {
    root: {
      label: '简体中文',
      lang: 'zh-CN',
    },
    en: {
      label: 'English',
      lang: 'en-US',
      link: '/en/'
    }
  },

  themeConfig: {
    logo: '/logo.png',
    siteTitle: 'SpinningMomo',
    
    // 社交链接
    socialLinks: [
      { icon: 'github', link: 'https://github.com/ChanIok/SpinningMomo' }
    ],

    // 导航栏
    nav: [
      { text: '指南', link: '/zh/guide/getting-started' },
      { text: '进阶', link: '/zh/advanced/custom-settings' },
      { text: '下载', link: 'https://github.com/ChanIok/SpinningMomo/releases' }
    ],

    sidebar: {
      '/zh/': [
        {
          text: '指南',
          items: [
            { text: '项目介绍', link: '/zh/guide/introduction' },
            { text: '快速开始', link: '/zh/guide/getting-started' },
            { text: '基本功能', link: '/zh/guide/features' },
          ]
        },
        {
          text: '进阶使用',
          items: [
            { text: '自定义设置', link: '/zh/advanced/custom-settings' },
            { text: '常见问题', link: '/zh/advanced/troubleshooting' },
          ]
        }
      ],
      '/en/': [
        {
          text: 'Guide',
          items: [
            { text: 'Introduction', link: '/en/guide/introduction' },
            { text: 'Getting Started', link: '/en/guide/getting-started' },
            { text: 'Features', link: '/en/guide/features' },
          ]
        },
        {
          text: 'Advanced',
          items: [
            { text: 'Custom Settings', link: '/en/advanced/custom-settings' },
            { text: 'Troubleshooting', link: '/en/advanced/troubleshooting' },
          ]
        }
      ]
    }
  }
}) 
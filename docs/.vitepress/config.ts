import { defineConfig } from "vitepress";

const baseEnv = process.env.VITE_BASE_PATH || "/";
const base = baseEnv.endsWith("/") ? baseEnv : `${baseEnv}/`;
const withBasePath = (p: string) => `${base}${p.replace(/^\//, "")}`;

export default defineConfig({
  title: "SpinningMomo",
  description: "《无限暖暖》游戏摄影与录像工具",

  // 允许通过环境变量自定义基础路径，默认为根路径
  base,

  head: [
    ["link", { rel: "icon", href: withBasePath("/logo.png") }],
    ["link", { rel: "apple-touch-icon", href: withBasePath("/logo.png") }],
  ],

  // 忽略死链接检查
  ignoreDeadLinks: true,

  locales: {
    root: {
      label: "简体中文",
      lang: "zh-CN",
    },
    en: {
      label: "English",
      lang: "en-US",
      link: "/en/",
      themeConfig: {
        nav: [
          { text: "Guide", link: "/en/guide/getting-started" },
          { text: "Legal", link: "/en/about/legal" },
          {
            text: "Version",
            items: [
              { text: "v2.0 (Current)", link: "/en/" },
              { text: "v0.7.7 (Legacy)", link: "/v0/en/" },
            ],
          },
        ],
        sidebar: {
          "/en/": [
            {
              text: "Guide",
              items: [{ text: "Getting Started", link: "/en/guide/getting-started" }],
            },
            {
              text: "Features",
              items: [
                { text: "Window & Resolution", link: "/en/features/window" },
                { text: "Screenshots", link: "/en/features/screenshot" },
                { text: "Video Recording", link: "/en/features/recording" },
              ],
            },
            {
              text: "Developer",
              items: [{ text: "Architecture", link: "/en/developer/architecture" }],
            },
            {
              text: "About",
              items: [
                { text: "Legal & Privacy", link: "/en/about/legal" },
                { text: "Open Source Credits", link: "/en/about/credits" },
              ],
            },
          ],
        },
      },
    },
  },

  themeConfig: {
    logo: withBasePath("/logo.png"),
    siteTitle: "SpinningMomo",

    // 社交链接
    socialLinks: [
      { icon: "github", link: "https://github.com/ChanIok/SpinningMomo" },
    ],

    // 导航栏
    nav: [
      { text: "指南", link: "/zh/guide/getting-started" },
      { text: "开发者", link: "/zh/developer/architecture" },
      {
        text: "版本",
        items: [
          { text: "v2.0 (当前)", link: "/" },
          { text: "v0.7.7 (旧版)", link: "/v0/index.md" }
        ]
      },
      {
        text: "下载",
        link: "https://github.com/ChanIok/SpinningMomo/releases",
      },
    ],

    sidebar: {
      "/zh/": [
        {
          text: "🚀 快速上手",
          items: [
            { text: "安装与运行", link: "/zh/guide/getting-started" },
          ],
        },
        {
          text: "⚡ 功能",
          items: [
            { text: "比例与分辨率调整", link: "/zh/features/window" },
            { text: "超清截图", link: "/zh/features/screenshot" },
            { text: "视频录制", link: "/zh/features/recording" },
          ],
        },
        {
          text: "🛠️ 开发者指南",
          items: [
            { text: "架构与构建", link: "/zh/developer/architecture" },
          ],
        },
        { 
          text: "📄 关于", 
          items: [
            { text: "法律与隐私", link: "/zh/about/legal" },
            { text: "开源鸣谢", link: "/zh/about/credits" },
          ] 
        },
      ],
      // 保留旧版本的配置
      "/v0/zh/": [
        {
          text: "指南 (v0.7.7)",
          items: [
            { text: "项目介绍", link: "/v0/zh/guide/introduction" },
            { text: "快速开始", link: "/v0/zh/guide/getting-started" },
            { text: "基本功能", link: "/v0/zh/guide/features" },
          ],
        },
        {
          text: "进阶使用",
          items: [
            { text: "自定义设置", link: "/v0/zh/advanced/custom-settings" },
            { text: "常见问题", link: "/v0/zh/advanced/troubleshooting" },
          ],
        },
      ],
      "/v0/en/": [
        {
          text: "Guide (v0.7.7)",
          items: [{ text: "Overview", link: "/v0/en/" }],
        },
        {
          text: "Legal",
          items: [
            { text: "Legal & Privacy Notice", link: "/v0/en/legal/notice" },
            { text: "Third-Party Licenses", link: "/v0/en/credits" }
          ],
        },
      ]
    },
  },
});

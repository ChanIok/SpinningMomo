import { defineConfig } from "vitepress";

const baseEnv = process.env.VITE_BASE_PATH || "/";
const base = baseEnv.endsWith("/") ? baseEnv : `${baseEnv}/`;
const withBasePath = (p: string) => `${base}${p.replace(/^\//, "")}`;

export default defineConfig({
  title: "SpinningMomo",
  description: "一个为《无限暖暖》提升摄影体验的窗口调整工具",

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
          { text: "Guide", link: "/en/" },
          { text: "Legal", link: "/en/legal/notice" },
        ],
        sidebar: {
          "/en/": [
            {
              text: "Guide",
              items: [{ text: "Overview", link: "/en/" }],
            },
            {
              text: "Legal",
              items: [{ text: "Legal & Privacy Notice", link: "/en/legal/notice" }],
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
      { text: "进阶", link: "/zh/advanced/custom-settings" },
      { text: "法律与隐私", link: "/zh/legal/notice" },
      {
        text: "下载",
        link: "https://github.com/ChanIok/SpinningMomo/releases",
      },
    ],

    sidebar: {
      "/zh/": [
        {
          text: "指南",
          items: [
            { text: "项目介绍", link: "/zh/guide/introduction" },
            { text: "快速开始", link: "/zh/guide/getting-started" },
            { text: "基本功能", link: "/zh/guide/features" },
          ],
        },
        {
          text: "进阶使用",
          items: [
            { text: "自定义设置", link: "/zh/advanced/custom-settings" },
            { text: "常见问题", link: "/zh/advanced/troubleshooting" },
          ],
        },
        { text: "法律与隐私", items: [{ text: "法律与隐私说明", link: "/zh/legal/notice" }] },
      ],
    },
  },
});

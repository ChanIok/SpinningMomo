import { defineConfig } from "vitepress";
import type { HeadConfig } from "vitepress";
import {
  SITE_ORIGIN,
  getBilingualPathnames,
  isLegacyDocPath,
  mdRelativeToPathname,
  pageLocale,
  toAbsoluteUrl,
} from "./seo";

const baseEnv = process.env.VITE_BASE_PATH || "/";
const base = baseEnv.endsWith("/") ? baseEnv : `${baseEnv}/`;
const withBasePath = (p: string) => `${base}${p.replace(/^\//, "")}`;
const SITE_NAME = "SpinningMomo";
const SITE_NAME_ZH = "旋转吧大喵";

export default defineConfig({
  title: SITE_NAME,
  description: "《无限暖暖》游戏摄影与录像工具",

  // 允许通过环境变量自定义基础路径，默认为根路径
  base,

  head: [
    ["link", { rel: "icon", href: withBasePath("/logo.png") }],
    ["link", { rel: "apple-touch-icon", href: withBasePath("/logo.png") }],
    ["meta", { property: "og:site_name", content: SITE_NAME }],
    ["meta", { name: "application-name", content: SITE_NAME }],
    ["meta", { name: "apple-mobile-web-app-title", content: SITE_NAME }],
  ],

  // 忽略死链接检查
  ignoreDeadLinks: true,

  sitemap: {
    hostname: SITE_ORIGIN,
    transformItems(items) {
      // VitePress 在此使用相对路径（如 v0/zh/...），不含前导 /v0
      return items.filter((item) => item.url !== "v0" && !item.url.startsWith("v0/"));
    },
  },

  async transformHead(ctx): Promise<HeadConfig[]> {
    const { pageData, siteData, title, description } = ctx;
    const relativePath = pageData.relativePath;
    if (!relativePath || pageData.isNotFound) {
      return [];
    }

    const siteBase = siteData.base ?? "/";
    const pathname = mdRelativeToPathname(relativePath);
    const canonical = toAbsoluteUrl(SITE_ORIGIN, siteBase, pathname);

    const head: HeadConfig[] = [
      ["link", { rel: "canonical", href: canonical }],
    ];

    if (isLegacyDocPath(relativePath)) {
      head.push(["meta", { name: "robots", content: "noindex, follow" }]);
    }

    const bilingual = getBilingualPathnames(relativePath);
    if (bilingual) {
      const zhUrl = toAbsoluteUrl(SITE_ORIGIN, siteBase, bilingual.zhPathname);
      const enUrl = toAbsoluteUrl(SITE_ORIGIN, siteBase, bilingual.enPathname);
      head.push(["link", { rel: "alternate", hreflang: "zh-CN", href: zhUrl }]);
      head.push(["link", { rel: "alternate", hreflang: "en-US", href: enUrl }]);
      head.push(["link", { rel: "alternate", hreflang: "x-default", href: enUrl }]);
    }

    head.push(["meta", { property: "og:title", content: title }]);
    head.push(["meta", { property: "og:site_name", content: SITE_NAME }]);
    head.push(["meta", { property: "og:description", content: description }]);
    head.push(["meta", { property: "og:url", content: canonical }]);
    head.push(["meta", { property: "og:type", content: "website" }]);

    const loc = pageLocale(relativePath);
    if (loc) {
      head.push([
        "meta",
        { property: "og:locale", content: loc.replace("-", "_") },
      ]);
      head.push([
        "meta",
        {
          property: "og:locale:alternate",
          content: loc === "zh-CN" ? "en_US" : "zh_CN",
        },
      ]);
    }

    head.push(["meta", { name: "twitter:card", content: "summary" }]);
    head.push(["meta", { name: "twitter:title", content: title }]);
    head.push(["meta", { name: "twitter:description", content: description }]);

    if (relativePath === "index.md" || relativePath === "en/index.md") {
      const websiteJsonLd = {
        "@context": "https://schema.org",
        "@type": "WebSite",
        name: SITE_NAME,
        alternateName: SITE_NAME_ZH,
        url: SITE_ORIGIN,
      };
      head.push([
        "script",
        { type: "application/ld+json" },
        JSON.stringify(websiteJsonLd),
      ]);
    }

    return head;
  },

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

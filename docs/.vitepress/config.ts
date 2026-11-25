import { defineConfig } from "vitepress";
import type { HeadConfig } from "vitepress";
import {
  SITE_ORIGIN,
  getBilingualPathnames,
  mdRelativeToPathname,
  pageLocale,
  toAbsoluteUrl,
} from "./seo.ts";

const baseEnv = process.env.VITE_BASE_PATH || "/";
const base = baseEnv.endsWith("/") ? baseEnv : `${baseEnv}/`;
const is_canonical_build = base === "/";
const withBasePath = (p: string) => `${base}${p.replace(/^\//, "")}`;
const SITE_NAME = "旋转吧大喵";
const SITE_NAME_EN = "SpinningMomo";
const SITE_DESCRIPTION_ZH = "面向《无限暖暖》的游戏摄影、录制与图库工具";
const SITE_DESCRIPTION_EN = "Game photography, recording, and gallery tool for Infinity Nikki";

export default defineConfig({
  title: SITE_NAME,
  description: SITE_DESCRIPTION_ZH,

  // Cloudflare Pages 等托管支持无 .html 的干净 URL
  cleanUrls: true,

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

  sitemap: is_canonical_build ? { hostname: SITE_ORIGIN } : undefined,

  async transformHead(ctx): Promise<HeadConfig[]> {
    const { pageData, title, description } = ctx;
    const relativePath = pageData.relativePath;
    if (!relativePath || pageData.isNotFound) {
      return [];
    }

    const pathname = mdRelativeToPathname(relativePath);
    const canonical = toAbsoluteUrl(SITE_ORIGIN, "/", pathname);

    const head: HeadConfig[] = [
      ["link", { rel: "canonical", href: canonical }],
    ];

    if (!is_canonical_build) {
      head.push(["meta", { name: "robots", content: "noindex, nofollow" }]);
    }

    const bilingual = getBilingualPathnames(relativePath);
    if (bilingual) {
      const zhUrl = toAbsoluteUrl(SITE_ORIGIN, "/", bilingual.zhPathname);
      const enUrl = toAbsoluteUrl(SITE_ORIGIN, "/", bilingual.enPathname);
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

    head.push(["meta", { name: "twitter:card", content: "summary" }]);
    head.push(["meta", { name: "twitter:title", content: title }]);
    head.push(["meta", { name: "twitter:description", content: description }]);

    if (relativePath === "index.md" || relativePath === "en/index.md") {
      const websiteJsonLd = {
        "@context": "https://schema.org",
        "@type": "WebSite",
        name: SITE_NAME,
        alternateName: SITE_NAME_EN,
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
      title: SITE_NAME,
      description: SITE_DESCRIPTION_ZH,
    },
    en: {
      label: "English",
      lang: "en-US",
      link: "/en/",
      title: SITE_NAME_EN,
      description: SITE_DESCRIPTION_EN,
      themeConfig: {
        siteTitle: SITE_NAME_EN,

        // 语言级的 themeConfig 与根配置浅合并，界面文案要重新给一遍
        outline: { label: "On this page", level: [2, 3] },
        docFooter: { prev: "Previous page", next: "Next page" },
        skipToContentLabel: "Skip to content",
        returnToTopLabel: "Return to top",
        sidebarMenuLabel: "Menu",
        navMenuLabel: "Menu",
        extraMenuLabel: "More",
        langMenuLabel: "Change language",
        darkModeSwitchLabel: "Appearance",
        lightModeSwitchTitle: "Switch to light theme",
        darkModeSwitchTitle: "Switch to dark theme",
        notFound: {
          code: "404",
          title: "PAGE NOT FOUND",
          quote: "Check that the URL is correct.",
          linkText: "Take me home",
          linkLabel: "Go to home",
        },

        search: {
          provider: "local",
          options: {
            translations: {
              button: { buttonText: "Search", buttonAriaLabel: "Search" },
              modal: {
                displayDetails: "Display detailed list",
                resetButtonTitle: "Reset search",
                backButtonTitle: "Close search",
                noResultsText: "No results for",
                footer: {
                  selectText: "to select",
                  selectKeyAriaLabel: "enter",
                  navigateText: "to navigate",
                  navigateUpKeyAriaLabel: "up arrow",
                  navigateDownKeyAriaLabel: "down arrow",
                  closeText: "to close",
                  closeKeyAriaLabel: "escape",
                },
              },
            },
          },
        },
        footer: {
          message: `<a href="${withBasePath("/en/about/legal")}">Legal & Privacy</a> · <a href="${withBasePath("/en/about/credits")}">Open Source Credits</a>`,
          copyright: "© 2026 InfinityMomo · GPL-3.0",
        },
        nav: [
          { text: "Guide", link: "/en/guide/getting-started" },
          {
            text: "Download",
            link: "https://github.com/ChanIok/SpinningMomo/releases/latest",
          },
          { text: "Legal", link: "/en/about/legal" },
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
                { text: "Window Adjustment", link: "/en/features/window" },
                { text: "Screenshots", link: "/en/features/screenshot" },
                { text: "Video Recording", link: "/en/features/recording" },
                { text: "HDR Support", link: "/en/features/hdr" },
                { text: "Gallery", link: "/en/features/gallery" },
                { text: "LAN Access", link: "/en/features/lan" },
                { text: "ADB Mode", link: "/en/features/adb" },
              ],
            },
            {
              text: "Help",
              items: [
                { text: "Tips", link: "/en/guide/tips" },
                { text: "FAQ", link: "/en/guide/faq" },
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
    siteTitle: SITE_NAME,

    /*
     * VitePress 2.0 不再内置界面多语言：目录标题、翻页、开关等文案都要显式配置，
     * 否则中文站会露出英文。
     */
    outline: { label: "本页目录", level: [2, 3] },
    docFooter: { prev: "上一篇", next: "下一篇" },
    skipToContentLabel: "跳到正文",
    returnToTopLabel: "回到顶部",
    sidebarMenuLabel: "菜单",
    navMenuLabel: "导航",
    extraMenuLabel: "更多",
    langMenuLabel: "切换语言",
    darkModeSwitchLabel: "外观",
    lightModeSwitchTitle: "切换到浅色模式",
    darkModeSwitchTitle: "切换到深色模式",

    notFound: {
      code: "404",
      title: "页面不存在",
      quote: "请检查链接是否正确。",
      linkText: "回到首页",
      linkLabel: "回到首页",
    },

    /*
     * 语言切换的落点。默认会机械替换语言前缀，但更新日志只有中文版，
     * 切到英文时回到英文首页。
     * 注意：这个函数会被序列化进客户端，只能使用入参，不能引用模块作用域变量；
     * 返回的路径不带 base，由 VPLink 统一补。
     */
    i18nRouting(_data, route, targetLocale) {
      const rel = String(route.data.relativePath).replace(/\.md$/, "");

      if (rel === "changelog") {
        return targetLocale === "root" ? "/" : `/${targetLocale}/`;
      }
      if (targetLocale === "root") {
        return `/${rel.replace(/^en\//, "").replace(/(^|\/)index$/, "")}`;
      }
      const en = rel.startsWith("en/") ? rel : `en/${rel}`;
      return `/${en.replace(/(^|\/)index$/, "")}`;
    },

    // 右上角只保留一个紧凑的搜索入口，Ctrl+K 唤起
    search: {
      provider: "local",
      options: {
        translations: {
          button: { buttonText: "搜索", buttonAriaLabel: "搜索" },
          modal: {
            displayDetails: "显示详细列表",
            resetButtonTitle: "清除查询",
            backButtonTitle: "关闭搜索",
            noResultsText: "没有找到结果",
            footer: {
              selectText: "选择",
              selectKeyAriaLabel: "回车",
              navigateText: "切换",
              navigateUpKeyAriaLabel: "上箭头",
              navigateDownKeyAriaLabel: "下箭头",
              closeText: "关闭",
              closeKeyAriaLabel: "Esc",
            },
          },
        },
      },
    },

    footer: {
      message: `<a href="${withBasePath("/about/legal")}">法律与隐私</a> · <a href="${withBasePath("/about/credits")}">开源鸣谢</a>`,
      copyright: "© 2026 InfinityMomo · GPL-3.0",
    },

    // 社交链接
    socialLinks: [
      { icon: "github", link: "https://github.com/ChanIok/SpinningMomo" },
    ],

    // 导航栏
    nav: [
      { text: "指南", link: "/guide/getting-started" },
      { text: "更新日志", link: "/changelog" },
    ],

    sidebar: {
      "/": [
        {
          text: "快速上手",
          items: [
            { text: "安装与运行", link: "/guide/getting-started" },
          ],
        },
        {
          text: "功能",
          items: [
            { text: "窗口调整", link: "/features/window" },
            { text: "超清截图", link: "/features/screenshot" },
            { text: "视频录制", link: "/features/recording" },
            { text: "HDR 支持", link: "/features/hdr" },
            { text: "图库", link: "/features/gallery" },
            { text: "局域网访问", link: "/features/lan" },
            { text: "ADB 模式", link: "/features/adb" },
          ],
        },
        {
          text: "帮助",
          items: [
            { text: "使用技巧", link: "/guide/tips" },
            { text: "常见问题", link: "/guide/faq" },
          ],
        },
        {
          text: "开发者指南",
          items: [
            { text: "架构与构建", link: "/developer/architecture" },
          ],
        },
        {
          text: "关于",
          items: [
            { text: "法律与隐私", link: "/about/legal" },
            { text: "开源鸣谢", link: "/about/credits" },
          ]
        },
      ],
    },
  },
});

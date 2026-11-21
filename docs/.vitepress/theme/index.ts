import type { Theme } from "vitepress";
import DefaultTheme from "vitepress/theme";

import HomeHero from "./components/HomeHero.vue";
import ChangelogBody from "./components/ChangelogBody.vue";

// 顺序有意义：令牌 → 品牌映射 → 营销页
import "./styles/tokens.css";
import "./styles/brand.css";
import "./styles/marketing.css";

export default {
  extends: DefaultTheme,
  enhanceApp({ app }) {
    app.component("HomeHero", HomeHero);
    app.component("ChangelogBody", ChangelogBody);
  },
} satisfies Theme;

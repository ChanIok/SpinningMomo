<script setup lang="ts">
import { computed, ref } from "vue";
import { withBase } from "vitepress";
import { releases } from "../data/releases";

const props = withDefaults(defineProps<{ lang?: "zh" | "en" }>(), { lang: "zh" });

const copy = {
  zh: {
    name: "旋转吧大喵",
    nameEn: "SpinningMomo",
    tagline: "《无限暖暖》游戏摄影与录像工具",
    cta: "快速开始",
    ctaHref: "/guide/getting-started",
    meta: (version: string) => `v${version} · Windows 10 1903+ · 64 位`,
    groupLabel: "切换演示内容",
    tabs: [
      {
        tab: "构图",
        ratio: "2.333",
        label: "21:9",
        title: "自由构图",
        text: "任意窗口比例，画面不再被裁切",
      },
      {
        tab: "清晰度",
        ratio: "1.778",
        label: "12K",
        title: "超清截图",
        text: "绕过游戏原生限制，最高 11520×6480",
      },
      {
        tab: "录制",
        ratio: "0.5625",
        label: "9:16",
        title: "竖屏录制",
        text: "自定义比例下直接录，不必后期裁切",
      },
    ],
  },
  en: {
    name: "SpinningMomo",
    nameEn: "旋转吧大喵",
    tagline: "Infinity Nikki game photography and recording tool",
    cta: "Get Started",
    ctaHref: "/en/guide/getting-started",
    meta: (version: string) => `v${version} · Windows 10 1903+ · 64-bit`,
    groupLabel: "Switch demo",
    tabs: [
      {
        tab: "Framing",
        ratio: "2.333",
        label: "21:9",
        title: "Free framing",
        text: "Any window ratio, nothing gets cropped",
      },
      {
        tab: "Detail",
        ratio: "1.778",
        label: "12K",
        title: "Ultra-high-res capture",
        text: "Bypasses native limits, up to 11520×6480",
      },
      {
        tab: "Record",
        ratio: "0.5625",
        label: "9:16",
        title: "Vertical recording",
        text: "Record at any ratio, no cropping afterwards",
      },
    ],
  },
} as const;

const t = computed(() => copy[props.lang]);
const index = ref(0);
const active = computed(() => t.value.tabs[index.value]);
const version = releases[0]?.version ?? "";
const ctaHref = computed(() =>
  t.value.ctaHref.startsWith("http") ? t.value.ctaHref : withBase(t.value.ctaHref)
);
</script>

<template>
  <section class="hero">
    <h1 class="title">{{ t.name }}</h1>
    <p class="title-en" :lang="lang === 'zh' ? 'en' : 'zh'">{{ t.nameEn }}</p>
    <p class="lede">{{ t.tagline }}</p>

    <div class="visual">
      <div class="stage" :style="{ '--ratio': active.ratio }">
        <div class="frame">
          <span class="grid" aria-hidden="true" />
          <span class="marks" aria-hidden="true" />
          <span class="frame-label">{{ active.label }}</span>
        </div>
      </div>

      <div class="switcher" role="group" :aria-label="t.groupLabel">
        <button
          v-for="(item, i) in t.tabs"
          :key="item.tab"
          type="button"
          :aria-pressed="i === index"
          @click="index = i"
        >
          {{ item.tab }}
        </button>
      </div>

      <p class="caption" aria-live="polite">
        <span class="caption-title">{{ active.title }}</span>
        <span class="caption-sep">·</span>
        <span>{{ active.text }}</span>
      </p>
    </div>

    <div class="actions">
      <a class="btn" :href="ctaHref">{{ t.cta }}</a>
      <p class="meta">{{ t.meta(version) }}</p>
    </div>
  </section>
</template>

<style scoped>
.hero {
  display: grid;
  grid-template-columns: minmax(0, 0.92fr) minmax(0, 1.08fr);
  grid-template-areas:
    "title   visual"
    "en      visual"
    "lede    visual"
    "actions visual";
  align-content: center;
  align-items: start;
  column-gap: clamp(32px, 6vw, 76px);
  width: 100%;
  max-width: var(--shell);
  /* 一屏：扣掉顶栏与页脚 */
  min-height: calc(100svh - var(--vp-nav-height) - 84px);
  margin-inline: auto;
  padding-inline: var(--gutter);
  padding-block: clamp(40px, 8vh, 80px);
}

.title {
  grid-area: title;
  font-size: clamp(2.6rem, 4.4vw, 3.5rem);
  font-weight: 600;
  line-height: 1.12;
  letter-spacing: 0.015em;
  color: var(--brand);
}

.title-en {
  grid-area: en;
  margin-top: clamp(6px, 0.9vh, 10px);
  font-size: clamp(2.6rem, 4.4vw, 3.5rem);
  font-weight: 600;
  line-height: 1.12;
  /* 与小号副标题时要的字距相反：放大到与标题同级后必须收紧 */
  letter-spacing: 0;
  color: var(--ink);
}

.lede {
  grid-area: lede;
  margin-top: clamp(16px, 2.2vh, 22px);
  max-width: 26em;
  font-size: 1.5rem;
  line-height: 1.6;
  color: var(--ink-2);
}

/* ---------- 下载入口 ---------- */

.actions {
  grid-area: actions;
  margin-top: clamp(34px, 5.6vh, 54px);
}

.meta {
  margin-top: 14px;
  font-size: 12.5px;
  color: var(--ink-3);
  letter-spacing: 0.01em;
  font-variant-numeric: tabular-nums;
}

/* ---------- 画幅演示 ---------- */

.visual {
  grid-area: visual;
  --frame-h: clamp(180px, 26vh, 240px);
  display: flex;
  flex-direction: column;
  align-items: center;
  align-self: start;
  margin-top: 6px;
  min-width: 0;
}

.stage {
  position: relative;
  display: flex;
  align-items: center;
  justify-content: center;
  width: 100%;
  padding-block: 18px 0;
  /* 让框体按栏宽收敛，21:9 也不会顶出栅格 */
  container-type: inline-size;
}

.frame {
  --cap: calc(100cqw / var(--ratio, 1.778));
  position: relative;
  height: min(var(--frame-h), var(--cap));
  width: calc(min(var(--frame-h), var(--cap)) * var(--ratio, 1.778));
  border: 1px solid var(--line-strong);
  border-radius: 8px;
  background: var(--frame-bg);
  transition: width var(--dur-slow) var(--ease-out);
  will-change: width;
}

/* 三分线：摄影工具最有辨识度的几何，也让空框看起来像取景器 */
.grid {
  position: absolute;
  inset: 0;
  border-radius: inherit;
  background-image:
    linear-gradient(to right, var(--grid-line) 1px, transparent 1px),
    linear-gradient(to bottom, var(--grid-line) 1px, transparent 1px);
  background-size: 33.3333% 33.3333%;
}

.marks {
  position: absolute;
  inset: 0;
}

/* 四角裁切标记 */
.frame::before,
.frame::after,
.marks::before,
.marks::after {
  content: "";
  position: absolute;
  width: 13px;
  height: 13px;
  border: 0 solid var(--brand);
  opacity: 0.6;
}

.frame::before {
  top: -1px;
  left: -1px;
  border-top-width: 2px;
  border-left-width: 2px;
}

.frame::after {
  top: -1px;
  right: -1px;
  border-top-width: 2px;
  border-right-width: 2px;
}

.marks::before {
  bottom: -1px;
  left: -1px;
  border-bottom-width: 2px;
  border-left-width: 2px;
}

.marks::after {
  bottom: -1px;
  right: -1px;
  border-bottom-width: 2px;
  border-right-width: 2px;
}

.frame-label {
  position: absolute;
  top: 9px;
  left: 11px;
  font-family: var(--font-mono);
  font-size: 10.5px;
  letter-spacing: 0.12em;
  color: var(--ink-3);
  user-select: none;
}

.switcher {
  display: flex;
  gap: clamp(10px, 1.8vw, 22px);
  margin-top: 18px;
}

.switcher button {
  padding: 5px 1px;
  border: 0;
  border-bottom: 2px solid transparent;
  background: none;
  font-size: 13px;
  color: var(--ink-3);
  cursor: pointer;
  transition:
    color var(--dur-fast) var(--ease-out),
    border-color var(--dur-fast) var(--ease-out);
}

.switcher button:hover {
  color: var(--ink-2);
}

.switcher button[aria-pressed="true"] {
  color: var(--ink);
  border-bottom-color: var(--brand);
}

.caption {
  margin-top: 12px;
  min-height: 1.6em;
  font-size: 13px;
  color: var(--ink-3);
  text-align: center;
  text-wrap: balance;
}

.caption-title {
  color: var(--ink-2);
}

.caption-sep {
  margin-inline: 6px;
  color: var(--line-strong);
}

/* ---------- 竖向排布 ---------- */

@media (max-width: 900px) {
  .hero {
    grid-template-columns: minmax(0, 1fr);
    grid-template-areas:
      "title"
      "en"
      "lede"
      "visual"
      "actions";
    align-content: start;
    min-height: 0;
    padding-block: clamp(24px, 5vh, 40px) clamp(36px, 7vh, 56px);
  }

  .visual {
    /* 按最宽的 21:9 留出余量，任何状态下都不会顶出视口 */
    --frame-h: min(230px, calc((100vw - var(--gutter) * 2 - 32px) / 2.4));
    margin-block: clamp(24px, 4.5vh, 40px) 0;
  }

  /* 移动端给主视觉一块浅色承载面，避免细线框孤零零浮在底色里 */
  .stage {
    padding: 30px 16px;
    border-radius: 16px;
    background: var(--bg-soft);
  }

  .actions {
    margin-top: clamp(30px, 5.5vh, 44px);
  }

  .actions .btn {
    width: 100%;
  }

  .meta {
    text-align: center;
  }
}
</style>

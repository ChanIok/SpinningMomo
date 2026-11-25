<script setup lang="ts">
import { SITE } from "../data/site";
</script>

<template>
  <div class="download-links">
    <a
      class="download-card"
      :href="SITE.releases"
      target="_blank"
      rel="noopener noreferrer"
    >
      <div class="card-head">
        <span class="card-title">GitHub Releases</span>
        <svg class="card-arrow" viewBox="0 0 16 16" fill="none" aria-hidden="true">
          <path
            d="M4.5 11.5L11.5 4.5M11.5 4.5H6M11.5 4.5V10"
            stroke="currentColor"
            stroke-width="1.5"
            stroke-linecap="round"
            stroke-linejoin="round"
          />
        </svg>
      </div>
      <p class="card-desc">官方发布页 · 国内访问可能受限</p>
    </a>

    <a
      class="download-card is-primary"
      :href="SITE.pan"
      target="_blank"
      rel="noopener noreferrer"
    >
      <div class="card-head">
        <span class="card-title">百度网盘</span>
        <svg class="card-arrow" viewBox="0 0 16 16" fill="none" aria-hidden="true">
          <path
            d="M4.5 11.5L11.5 4.5M11.5 4.5H6M11.5 4.5V10"
            stroke="currentColor"
            stroke-width="1.5"
            stroke-linecap="round"
            stroke-linejoin="round"
          />
        </svg>
      </div>
      <p class="card-desc">
        国内备用 · 提取码 <code class="card-code">{{ SITE.panCode }}</code>
      </p>
    </a>
  </div>
</template>

<style scoped>
.download-links {
  display: grid;
  grid-template-columns: repeat(2, minmax(0, 1fr));
  gap: 12px;
  margin: 16px 0;
}

.download-links a.download-card {
  display: flex;
  flex-direction: column;
  gap: 6px;
  padding: 14px 16px;
  border-radius: var(--radius-md);
  border: 1px solid var(--line);
  background: var(--bg-soft);
  color: inherit;
  font-weight: inherit;
  text-decoration: none;
  transition:
    border-color var(--dur-fast) var(--ease-out),
    background-color var(--dur-fast) var(--ease-out),
    transform var(--dur-fast) var(--ease-out);
}

.download-links a.download-card:hover {
  color: inherit;
  text-decoration: none;
  border-color: var(--line-strong);
  transform: translateY(-1px);
}

.download-links a.download-card.is-primary {
  background: var(--brand-wash);
  border-color: var(--brand-line);
}

.download-links a.download-card.is-primary:hover {
  border-color: var(--brand);
}

.card-head {
  display: flex;
  align-items: center;
  gap: 8px;
}

.card-title {
  font-size: 15px;
  font-weight: 600;
  line-height: 1.3;
  color: var(--ink);
}

.card-arrow {
  width: 15px;
  height: 15px;
  margin-left: auto;
  flex-shrink: 0;
  color: var(--ink-3);
  transition:
    color var(--dur-fast) var(--ease-out),
    transform var(--dur-fast) var(--ease-out);
}

.download-card.is-primary .card-arrow {
  color: var(--brand-ink);
}

.download-card:hover .card-arrow {
  color: var(--brand-ink);
  transform: translate(1px, -1px);
}

.download-links p.card-desc {
  margin: 0;
  font-size: 13px;
  line-height: 1.5;
  color: var(--ink-2);
}

.download-links code.card-code {
  padding: 1px 6px;
  border-radius: 4px;
  border: 0;
  background: color-mix(in srgb, var(--brand) 13%, transparent);
  color: var(--brand-ink);
  font-family: var(--font-mono);
  font-size: 12px;
  font-weight: 500;
}

@media (max-width: 640px) {
  .download-links {
    grid-template-columns: minmax(0, 1fr);
  }
}
</style>

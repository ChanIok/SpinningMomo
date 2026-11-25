<script setup lang="ts">
import { SITE } from "../data/site";
import { releases } from "../data/releases";

const shown = releases.slice(0, 6);
</script>

<template>
  <div class="doc">
    <header class="doc-head">
      <h1>更新日志</h1>
    </header>

    <ol class="releases">
      <li v-for="(release, index) in shown" :id="`v${release.version}`">
        <div class="release-meta">
          <h2 class="version tnum">
            v{{ release.version }}
            <span v-if="index === 0" class="tag">最新</span>
          </h2>
          <time class="tnum" :datetime="release.date">{{ release.date }}</time>
          <a class="release-link" :href="release.url" target="_blank" rel="noopener noreferrer">
            GitHub
          </a>
        </div>

        <div class="release-body">
          <section v-for="group in release.groups" :key="group.title" class="group">
            <h3>{{ group.title }}</h3>
            <ul>
              <li v-for="item in group.items" :key="item">{{ item }}</li>
            </ul>
          </section>
        </div>
      </li>
    </ol>

    <p class="doc-foot">
      更早的版本请前往
      <a :href="`${SITE.repo}/releases`" target="_blank" rel="noopener noreferrer">GitHub Releases</a>。
    </p>
  </div>
</template>

<style scoped>
.releases {
  margin-top: 32px;
}

.releases > li {
  display: grid;
  grid-template-columns: 148px minmax(0, 1fr);
  gap: 32px;
  padding-block: 34px 38px;
  scroll-margin-top: 24px;
}

.releases > li:first-child {
  padding-block-start: 0;
}

.releases > li + li {
  border-top: 1px solid var(--line);
}

.release-meta {
  display: flex;
  flex-direction: column;
  align-items: flex-start;
  gap: 4px;
  padding-top: 2px;
}

.version {
  display: flex;
  align-items: center;
  gap: 8px;
  font-family: var(--font-mono);
  font-size: 13.5px;
  font-weight: 500;
  letter-spacing: 0.02em;
}

.release-meta time {
  font-size: 12.5px;
  color: var(--ink-3);
}

.tag {
  padding: 1px 6px;
  border-radius: 999px;
  background: var(--brand-wash);
  color: var(--brand-ink);
  font-family: var(--font-sans);
  font-size: 10.5px;
  font-weight: 400;
  letter-spacing: 0.04em;
}

.release-link {
  font-size: 12.5px;
  color: var(--ink-3);
  transition: color var(--dur-fast) var(--ease-out);
}

.release-link:hover {
  color: var(--brand-ink);
}

.group + .group {
  margin-top: 22px;
}

.group h3 {
  font-size: 12.5px;
  font-weight: 500;
  letter-spacing: 0.06em;
  color: var(--ink-3);
}

.group ul {
  margin-top: 8px;
}

.group li {
  position: relative;
  padding: 4px 0 4px 15px;
  font-size: 14px;
  line-height: 1.75;
  color: var(--ink-2);
}

.group li::before {
  content: "";
  position: absolute;
  top: 0.86em;
  left: 2px;
  width: 3px;
  height: 3px;
  border-radius: 50%;
  background: var(--line-strong);
}

@media (max-width: 700px) {
  .releases > li {
    grid-template-columns: minmax(0, 1fr);
    gap: 14px;
    padding-block: 26px 30px;
  }

  .release-meta {
    flex-direction: row;
    align-items: center;
    gap: 10px;
  }

  .release-link {
    margin-left: auto;
  }
}
</style>

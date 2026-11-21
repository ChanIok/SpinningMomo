/**
 * 从 GitHub Releases 拉取更新日志，生成 .vitepress/theme/data/releases.ts。
 *
 * 构建时不联网：CI 里 GitHub 可达性不稳定，站点应始终能用仓库内的快照构建。
 * 需要刷新时手动运行 `pnpm --filter docs run sync:releases`。
 */
import { writeFile } from "node:fs/promises";
import path from "node:path";
import { fileURLToPath } from "node:url";

const REPO = "ChanIok/SpinningMomo";
const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const OUT = path.join(ROOT, ".vitepress/theme/data/releases.ts");
const LIMIT = 12;

/** 把 Release Notes 的 Markdown 拆成「分组 + 条目」 */
function parseBody(body) {
  const groups = [];
  let section = null;
  let current = null;

  const open = (title) => {
    current = { title, items: [] };
    groups.push(current);
  };

  for (const raw of body.split(/\r?\n/)) {
    const line = raw.trim();
    if (!line || line.startsWith("**Full Changelog**")) continue;

    if (line.startsWith("### ")) {
      const sub = line.slice(4).trim();
      open(section ? `${section} · ${sub}` : sub);
      continue;
    }

    if (line.startsWith("## ")) {
      section = line.slice(3).trim();
      current = null;
      continue;
    }

    if (line.startsWith("- ")) {
      if (!current) open(section ?? "改动");
      current.items.push(line.slice(2).trim());
    }
  }

  return groups.filter((group) => group.items.length > 0);
}

const response = await fetch(`https://api.github.com/repos/${REPO}/releases?per_page=${LIMIT}`, {
  headers: {
    Accept: "application/vnd.github+json",
    "User-Agent": "spinningmomo-docs",
    ...(process.env.GITHUB_TOKEN ? { Authorization: `Bearer ${process.env.GITHUB_TOKEN}` } : {}),
  },
});

if (!response.ok) {
  throw new Error(`GitHub API ${response.status} ${response.statusText}`);
}

const payload = await response.json();

const releases = payload
  .filter((release) => !release.draft && !release.prerelease)
  .map((release) => ({
    version: String(release.tag_name).replace(/^v/, ""),
    tag: release.tag_name,
    name: release.name,
    date: String(release.published_at).slice(0, 10),
    url: release.html_url,
    groups: parseBody(release.body ?? ""),
  }))
  .filter((release) => release.groups.length > 0);

const banner = `// 由 scripts/sync-releases.mjs 生成，请勿手动编辑。
// 数据来源：https://github.com/${REPO}/releases
`;

const source = `${banner}
export interface ReleaseGroup {
  title: string;
  items: string[];
}

export interface Release {
  version: string;
  tag: string;
  name: string;
  date: string;
  url: string;
  groups: ReleaseGroup[];
}

export const releases: Release[] = ${JSON.stringify(releases, null, 2)};
`;

await writeFile(OUT, source, "utf8");
console.log(`wrote ${releases.length} releases -> ${path.relative(ROOT, OUT)}`);

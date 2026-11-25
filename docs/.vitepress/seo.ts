/** 正式站点的绝对源（canonical / hreflang / sitemap），不含尾斜杠 */
export const SITE_ORIGIN = "https://spin.infinitymomo.com";

export function mdRelativeToPathname(relativePath: string): string {
  const p = relativePath.replace(/\\/g, "/");
  if (p === "index.md") return "/";
  if (p.endsWith("/index.md")) {
    const dir = p.slice(0, -"/index.md".length);
    return `/${dir}/`;
  }
  if (p.endsWith(".md")) {
    return `/${p.slice(0, -3)}`;
  }
  return `/${p}`;
}

export function toAbsoluteUrl(siteOrigin: string, base: string, pathname: string): string {
  const origin = siteOrigin.replace(/\/$/, "");
  const normalizedBase =
    base === "/" || base === ""
      ? ""
      : base.endsWith("/")
        ? base.slice(0, -1)
        : base;
  const path = pathname.startsWith("/") ? pathname : `/${pathname}`;
  return `${origin}${normalizedBase}${path}`;
}

/** 只有中文的单页（更新日志）：没有英文对页，不输出 hreflang */
const SINGLE_LOCALE_PAGES = new Set(["changelog.md"]);

/** 返回中英对页的 canonical 路径；单语页面返回 null。 */
export function getBilingualPathnames(relativePath: string): {
  zhPathname: string;
  enPathname: string;
} | null {
  const p = relativePath.replace(/\\/g, "/");
  if (SINGLE_LOCALE_PAGES.has(p)) return null;

  if (p === "index.md") {
    return { zhPathname: "/", enPathname: "/en/" };
  }
  if (p === "en/index.md") {
    return { zhPathname: "/", enPathname: "/en/" };
  }
  if (p.startsWith("en/")) {
    const rest = p.slice("en/".length);
    return {
      zhPathname: mdRelativeToPathname(rest),
      enPathname: mdRelativeToPathname(p),
    };
  }
  if (p.endsWith(".md")) {
    return {
      zhPathname: mdRelativeToPathname(p),
      enPathname: mdRelativeToPathname(`en/${p}`),
    };
  }
  return null;
}

export function pageLocale(relativePath: string): "zh-CN" | "en-US" {
  const p = relativePath.replace(/\\/g, "/");
  return p.startsWith("en/") ? "en-US" : "zh-CN";
}

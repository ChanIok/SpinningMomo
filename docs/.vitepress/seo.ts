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

/** v0 文档：不参与 hreflang，且应 noindex */
export function isLegacyDocPath(relativePath: string): boolean {
  return relativePath.replace(/\\/g, "/").startsWith("v0/");
}

/**
 * 返回当前 v2 页面对应的中英 canonical 路径（含前导 /，已考虑 index.md）。
 * 若无对页（非 v2 双语结构），返回 null。
 */
export function getBilingualPathnames(relativePath: string): {
  zhPathname: string;
  enPathname: string;
} | null {
  const p = relativePath.replace(/\\/g, "/");
  if (isLegacyDocPath(p)) return null;

  if (p === "index.md") {
    return { zhPathname: "/", enPathname: "/en/" };
  }
  if (p === "en/index.md") {
    return { zhPathname: "/", enPathname: "/en/" };
  }
  if (p.startsWith("zh/")) {
    const rest = p.slice("zh/".length);
    return {
      zhPathname: mdRelativeToPathname(p),
      enPathname: mdRelativeToPathname(`en/${rest}`),
    };
  }
  if (p.startsWith("en/")) {
    const rest = p.slice("en/".length);
    return {
      zhPathname: mdRelativeToPathname(`zh/${rest}`),
      enPathname: mdRelativeToPathname(p),
    };
  }
  return null;
}

export function pageLocale(relativePath: string): "zh-CN" | "en-US" | null {
  const p = relativePath.replace(/\\/g, "/");
  if (isLegacyDocPath(p)) return null;
  if (p === "index.md") return "zh-CN";
  if (p.startsWith("en/")) return "en-US";
  if (p.startsWith("zh/")) return "zh-CN";
  return null;
}

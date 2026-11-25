<script setup lang="ts">
import { computed, onBeforeUnmount, onMounted, ref } from "vue";
import { withBase } from "vitepress";
import { releases } from "../data/releases";

type ModeId = "ratio" | "superres" | "record" | "gallery";

interface HeroTab {
  id: ModeId;
  tab: string;
  ratio: number;
  text: string;
  linkLabel: string;
  href: string;
}

interface RatioPreset {
  label: string;
  ratio: number;
  dim: string;
}

interface ResPreset {
  id: "4k" | "8k" | "12k";
  label: string;
  dim: string;
  mp: string;
  screenScale: number;
}

interface GalleryCardItem {
  id: number;
  row: 1 | 2;
  grow: number;
  res: string;
  focal: string;
  aperture: string;
  swatches: readonly [string, string, string, string, string];
  tone: string;
}

// 复用自 web/src/assets/momo-outline.svg，保持官网与客户端视觉一致
const MOMO_OUTLINE_PATH =
  "M831.142 470.376C832.559 470.121 832.083 470.162 833.708 470.092C983.03 463.675 1138.44 507.296 1251.43 645.307C1251.67 645.888 1255.42 649.442 1255.7 649.292C1265.67 644.075 1274.37 637.704 1284.7 633.131C1304.36 624.431 1324.81 619.352 1345.5 618.036C1401.15 614.849 1460.7 635.401 1500.72 684.407C1541.41 734.229 1543.25 790.988 1550.8 856.393C1559.12 882.45 1578.49 903.642 1577.02 935.234C1573.79 1005 1546.64 1112.5 1513.01 1165.09C1488.03 1204.16 1413.35 1219.96 1376 1223.13C1341.13 1226.08 1312.44 1210.69 1286.89 1177.74C1223.83 1258.8 1173.78 1303.3 1089.28 1334.18C1114.34 1371.78 1155.45 1453.79 1159.13 1506.03C1160.84 1530.37 1143.9 1544.57 1134.22 1558.7L1133.6 1559.62C1140.15 1576.43 1147.88 1593.41 1154.08 1610.81C1159 1624.37 1164.02 1638.93 1168.87 1652.41C1172.73 1663.14 1189.86 1703.42 1187.43 1715.24C1181.28 1745.19 1161.28 1773.54 1149.44 1800.15C1146.97 1805.71 1143.32 1812.17 1141.33 1818.23C1137.56 1835.86 1142.73 1854.46 1136.08 1872.19C1128.05 1893.4 1110.26 1909.64 1093.32 1915.82C1063.31 1926.77 1031.91 1936.87 1002.98 1916.9C995 1919.61 987.375 1922.64 979.3 1925.18C966.308 1929.25 952.912 1931.71 939.979 1935.41C934.484 1936.31 912.804 1938.72 909.163 1941.86C905.249 1945.24 908.68 1987.98 892.638 2009.92C876.289 2032.27 848.678 2038.44 826.33 2038.19C808.351 2037.99 785.292 2034.53 772.318 2016.34C764.729 2005.71 763.729 1996.01 763.992 1982.11C743.548 1967.4 729.644 1938.06 714.336 1926C701.391 1915.8 668.219 1908.2 652.546 1900.51C628.203 1888.56 624.753 1892.65 622.646 1852.86C615.924 1741.48 571.687 1639.86 573.074 1526.74C573.21 1515.65 573.758 1494.38 572.417 1484.13C561.599 1483.53 545.115 1488.34 532.17 1488.44C512.256 1488.58 480.138 1493.92 463.39 1476.76C461.291 1472.77 452.364 1451.12 452.066 1447.48C450.914 1433.39 450.645 1409.6 459.475 1400.91C478.473 1382.22 498.711 1370.89 518.457 1355.57C553.299 1328.52 587.834 1301.7 624.269 1279.11C584.199 1259.53 554.484 1240.89 520.486 1205.16C509.511 1193.63 498.565 1176.18 487.309 1167.67C482.844 1164.3 461.935 1161.38 455.68 1160.53C407.588 1153.95 374.021 1139.31 335.715 1098.22C297.768 1057.52 266.705 1000.15 260.272 934.12C255.156 881.597 279.154 865.759 289.936 823.221C298.047 789.459 296.577 735.436 315.725 708.083C333.04 683.349 365.377 647.952 386.793 629.391C419.336 601.188 461.515 593.45 499.388 596.153C515.316 597.29 534.726 600.17 550.452 597.883C554.708 596.832 579.862 575.992 585.21 571.751C597.018 562.522 608.984 553.657 621.098 545.164C671.539 510.664 725.888 487.448 781.974 476.442C798.174 473.401 814.891 472.369 831.142 470.376Z M835.878 487.69C842.299 486.453 870.719 486.869 878.511 487.089C974.952 489.808 1077.1 516.291 1160.81 581.823C1181.93 598.307 1202.15 616.754 1221.33 637.026C1230.67 647.043 1239.29 659.418 1249.23 668.26C1251.75 670.498 1254.07 670.552 1256.97 669.636C1262.59 667.858 1268.07 663.555 1273.24 660.201C1284.06 653.182 1295.5 645.848 1307.35 642.579C1364.41 626.827 1422.21 635.512 1471.42 679.144C1504.97 708.885 1524.14 756.74 1529.01 809.69C1530.68 827.852 1530.54 848.501 1535.41 865.68C1541.43 886.903 1556.14 899.227 1559.37 922.849C1565.84 970.232 1529.4 1101.28 1508.57 1140.15C1504.18 1148.34 1499.49 1156.52 1493.52 1162.79C1471.08 1186.37 1430.11 1197.1 1401.9 1203.05C1371.32 1209.5 1341.37 1208.87 1315.36 1183.94C1306.62 1175.56 1299.04 1165.29 1290.3 1156.93L1289.36 1156.05C1287.43 1155.9 1282.21 1155.41 1281.02 1156.85C1268.1 1172.41 1253.63 1191.37 1240.77 1206.51C1201.69 1252.23 1156.18 1287.12 1106.88 1309.15C1095.76 1314.16 1071.77 1319.87 1064.23 1324.52C1063.7 1326.81 1064.56 1328.46 1065.92 1330.53C1097.49 1378.64 1132.25 1441.8 1141.95 1505.3C1144.81 1524 1122.82 1543.06 1115.26 1555.34C1116.55 1566.13 1122.74 1578.55 1126.45 1588.38C1132.97 1605.63 1168.11 1696.51 1169.4 1708.01C1171.23 1724.29 1142.39 1776.56 1134.34 1793.02C1130.82 1800.2 1126.21 1807.79 1124.1 1815.96C1118.94 1835.94 1127.21 1854.36 1116.23 1874.46C1100.85 1902.6 1049.63 1915.91 1025.6 1909.94C1018.47 1908.17 1010.75 1896.72 1003.65 1897.55C995.899 1898.45 987.33 1904.43 979.614 1907.02C963.676 1912.36 947.045 1915.99 930.8 1919.23C928.604 1918.9 899.182 1924.63 897.147 1926.86C882.939 1942.38 894.603 1981.04 878.696 1999.88C857.111 2025.44 808.775 2030.26 785.844 2006.32C779.619 1999.69 780.795 1982.11 780.882 1971.75C750.298 1953.56 747.388 1930.31 721.74 1910.04C709.831 1900.62 648.413 1884.33 641.657 1873.66C639.971 1865.7 639.275 1850.78 638.772 1842.06C635.911 1792.51 625.552 1747.32 614.46 1700.18C600.105 1639.17 588.579 1576.44 589.905 1511.97C590.187 1498.28 589.151 1483.76 590.564 1470.22C590.993 1466.2 592.187 1464.88 589.652 1461.56C586.785 1461.46 565.549 1466.47 560.414 1467.35C543.837 1470.11 527.158 1471.62 510.455 1471.88C485.082 1472.58 466.561 1472.98 468.09 1430.26C468.807 1410.23 477.139 1406.58 488.887 1398.31C533.578 1366.87 576.762 1331.95 621.765 1301.26C626.742 1297.87 654.313 1281.46 656.621 1278.45L656.466 1276.87C644.755 1267.7 612.563 1257.05 599.414 1246.93C591.938 1241.17 584.991 1237.95 578.11 1232.78C563.948 1222.12 549.801 1209.35 536.533 1196.73C523.413 1184.26 509.73 1162.45 496.736 1151.99C492.94 1148.93 470.483 1145.66 464.746 1144.87C415.642 1138.06 379.3 1124.88 341.519 1079.22C310.132 1041.3 280.548 989.011 277.021 930.328C275.198 900.004 283.032 883.496 294.705 858.942C296.153 854.628 301.573 845.942 302.187 841.709C303.839 830.305 307.873 819.879 309.821 808.889C315.418 777.315 314.004 740.092 331.365 714.588C341.01 700.42 354.212 688.198 364.976 675.37C411.393 621.72 450.049 607.425 511.2 614.169C521.218 615.274 550.399 619.407 558.054 614.495C577.609 601.948 598.523 583.294 618.227 569.095C687.221 519.376 757.369 492.549 835.878 487.69Z";

// 复用 web/src/assets/zongzi-momo.svg 的外轮廓，以描边控制线宽
const ZONGZI_MOMO_OUTER_PATH =
  "M1394.82 42.4357C1400.16 41.6537 1408.66 42.0382 1414.31 42.102C1421.54 48.5148 1425.86 51.2328 1428.85 61.748C1439.15 104.165 1433.23 148.819 1434.01 192.11C1434.47 218.315 1439.16 244.924 1444.71 270.309C1488 468.332 1630.17 663.099 1552.56 875.347C1548.58 886.242 1490.73 944.159 1479.55 955.451C1484.14 966.113 1485.36 969.612 1487.34 981.304C1494.7 977.679 1505.77 971.103 1512.19 965.585C1535.96 945.155 1543.18 929.574 1574.6 929.55C1597.86 929.532 1614.14 936.931 1631.79 955.904C1640.63 965.417 1645.68 980.362 1645.46 994.289C1644.99 1023.89 1632.94 1037.15 1616.68 1056.17C1616.15 1060.66 1615.64 1064.91 1614.59 1069.29C1612.01 1080.3 1607.31 1090.47 1600.85 1098.99C1590.29 1112.95 1578.91 1118.29 1568.15 1129.57C1529.61 1169.98 1490.12 1213.05 1444.68 1242.72C1442.51 1266.8 1438.26 1275.81 1430.09 1297.35C1440.01 1316.22 1448.92 1346.94 1453.51 1368.15C1473.49 1340.07 1501.85 1313.61 1532.54 1304.68C1604.92 1283.61 1693.72 1317.42 1714.43 1409.73C1730.07 1479.77 1709.39 1565.08 1636.13 1560.27C1597.12 1557.71 1576.11 1531.95 1567.43 1488.57C1543.58 1546.04 1534.56 1612.37 1500.39 1664.55C1478.7 1697.67 1449.12 1726.91 1418.24 1747.38C1395.05 1833.96 1329.93 1882.15 1261.35 1911.5C1256.11 1947.14 1256.12 1988.94 1246.61 2023.82C1242.34 2039.5 1225.83 2057.11 1212.41 2063.41C1187.83 2074.95 1151.56 2073.62 1125.03 2071.69C1119.07 2071.65 1108.27 2066.35 1102.98 2066.72C1068.22 2069.13 1011.77 2082.8 993.46 2035.5C988.76 2023.36 990.547 2003.74 995.258 1991.21C969.555 1951.4 944.647 1917.96 932.975 1868.94C927.33 1853.72 925.941 1820.92 919.678 1807.48C901.803 1769.12 886.755 1742.91 876.263 1699.65C875.79 1695.32 874.602 1690.08 873.982 1685.58C869.492 1652.98 869.807 1619.86 869.02 1586.85C800.963 1617.51 735.726 1570.24 690.204 1512.63C671.788 1513.99 662.114 1514.16 648.611 1497.2C634.204 1479.1 631.566 1466.28 631.962 1442.13C634.471 1438.31 637.495 1434.33 640.184 1430.63C638.854 1420.21 638.77 1415.41 638.647 1404.76C616.978 1408.1 594.763 1407.53 572.988 1407.63C523.063 1407.79 461.721 1389.97 418.994 1358.19C345.027 1303.18 273.334 1238.98 188.672 1207.44C163.826 1198.18 141.447 1194.67 115.364 1195.17C91.8501 1195.62 68.4742 1204.63 51.8504 1180.21L51.9427 1160.53C81.501 1115.35 230.399 1071.92 277.368 1054.04C304.573 1043.67 330.44 1031.95 357.806 1019.33C354.062 1003.69 352.487 985.5 349.022 970.716C340.4 933.92 332.651 905.304 327.596 867.483C325.26 865.917 322.934 864.332 320.617 862.728C305.448 852.167 287.311 834.603 277.029 817.437C272.934 810.598 269.479 802.552 265.078 795.663C260.516 788.523 253.025 780.687 248.94 773.586C223.874 730.023 200.045 678.357 189.468 627.134C183.708 599.238 179.971 562.751 193.649 537.531C200.746 524.446 211.302 512.762 222.004 503.879C272.21 462.205 345.507 464.346 404.34 464.97C419.519 465.132 434.795 466.46 449.971 465.762C463.002 465.162 476.152 461.124 489.042 458.679C493.871 457.763 511.103 456.555 514.115 455.034C533.589 445.206 553.865 425.317 572.267 412.495C593.608 397.625 614.47 383.934 636.335 370.736C694.151 335.839 760.091 319.149 823.24 303.617C836.431 300.373 850.877 297.808 863.659 294.254C890.278 263.178 913.471 228.962 942.239 199.767C992.539 149.52 1057.68 106.782 1123.15 144.167C1171.73 171.914 1185.5 207.428 1217.84 252.149C1242.76 189.426 1272.57 152.204 1319.38 111.892C1331.49 101.466 1343.96 91.8208 1355.88 81.0833C1368.87 69.3764 1380.38 50.8659 1394.82 42.4357Z";

const props = withDefaults(defineProps<{ lang?: "zh" | "en" }>(), { lang: "zh" });

const copy = {
  zh: {
    name: "旋转吧大喵",
    nameEn: "SpinningMomo",
    tagline: ["面向《无限暖暖》的", "游戏摄影、录制与图库工具"],
    cta: "快速开始",
    ctaHref: "/guide/getting-started",
    meta: (version: string) => `v${version} · Windows 10 1903+ · 64 位`,
    groupLabel: "切换核心功能演示",
    screenBounds: "1080P 物理屏幕",
    recordSplit: "自动切段",
    gallery: {
      todayGroup: "昨天",
      todayCount: "4 项",
      earlierGroup: "12月6日",
      earlierCount: "5 项",
      sectionNikki: "无限暖暖",
      resLabel: "分辨率",
      focalLabel: "焦距",
      apertureLabel: "光圈",
      codeLabel: "相机参数码",
      copyLabel: "复制",
    },
    tabs: [
      {
        id: "superres",
        tab: "超清",
        ratio: 1.778,
        text: "12K+ 及自定义超屏渲染，配合叠加层与预览窗自由构图",
        linkLabel: "超清截图",
        href: "/features/screenshot",
      },
      {
        id: "ratio",
        tab: "比例",
        ratio: 0.75,
        text: "自由切换 21:9、3:4、9:16 等比例，突破原生横屏限制",
        linkLabel: "窗口调整",
        href: "/features/window",
      },
      {
        id: "record",
        tab: "录制",
        ratio: 1.778,
        text: "显卡硬编直录自定义比例 MP4，换尺寸自动切段",
        linkLabel: "视频录制",
        href: "/features/recording",
      },
      {
        id: "gallery",
        tab: "图库",
        ratio: 1.56,
        text: "自适应时间流、主色调筛选与相机参数解析",
        linkLabel: "探索图库",
        href: "/features/gallery",
      },
    ] satisfies HeroTab[],
  },
  en: {
    name: "SpinningMomo",
    nameEn: "旋转吧大喵",
    tagline: ["Game photography, recording, and gallery tool for Infinity Nikki"],
    cta: "Get Started",
    ctaHref: "/en/guide/getting-started",
    meta: (version: string) => `v${version} · Windows 10 1903+ · 64-bit`,
    groupLabel: "Switch feature preview",
    screenBounds: "1080P DISPLAY",
    recordSplit: "AUTO SPLIT",
    gallery: {
      todayGroup: "Yesterday",
      todayCount: "4",
      earlierGroup: "Dec 6",
      earlierCount: "5",
      sectionNikki: "Infinity Nikki",
      resLabel: "Size",
      focalLabel: "Focal",
      apertureLabel: "Aperture",
      codeLabel: "Camera Code",
      copyLabel: "Copy",
    },
    tabs: [
      {
        id: "superres",
        tab: "Super-Res",
        ratio: 1.778,
        text: "12K+ & custom super-resolution with live framing preview",
        linkLabel: "Screenshots",
        href: "/en/features/screenshot",
      },
      {
        id: "ratio",
        tab: "Ratio",
        ratio: 0.75,
        text: "Switch between 21:9, 3:4, and 9:16 beyond widescreen",
        linkLabel: "Window guide",
        href: "/en/features/window",
      },
      {
        id: "record",
        tab: "Record",
        ratio: 1.778,
        text: "Direct GPU MP4 capture in custom ratios with auto-split",
        linkLabel: "Recording",
        href: "/en/features/recording",
      },
      {
        id: "gallery",
        tab: "Gallery",
        ratio: 1.56,
        text: "Adaptive timeline, palette filter, and camera metadata",
        linkLabel: "Explore gallery",
        href: "/en/features/gallery",
      },
    ] satisfies HeroTab[],
  },
} as const;

const ratioPresets: readonly RatioPreset[] = [
  { label: "3:4", ratio: 0.75, dim: "2880 × 3840" },
  { label: "1:1", ratio: 1, dim: "2560 × 2560" },
  { label: "3:2", ratio: 1.5, dim: "3840 × 2560" },
  { label: "21:9", ratio: 2.333, dim: "5040 × 2160" },
];

const resPresets: readonly ResPreset[] = [
  { id: "4k", label: "4K", dim: "3840 × 2160", mp: "8.3 MP", screenScale: 0.5 },
  { id: "8k", label: "8K", dim: "7680 × 4320", mp: "33.2 MP", screenScale: 0.32 },
  { id: "12k", label: "12K", dim: "11520 × 6480", mp: "74.6 MP", screenScale: 0.23 },
];

const galleryCards: readonly GalleryCardItem[] = [
  {
    id: 0,
    row: 1,
    grow: 1.48,
    res: "3840 × 2160",
    focal: "15.5 mm",
    aperture: "f / 5.0",
    swatches: ["#2c2420", "#4b5263", "#d8d1c8", "#7c6c60", "#b87a3d"],
    tone: "var(--gallery-tone-0)",
  },
  {
    id: 1,
    row: 1,
    grow: 1.0,
    res: "2880 × 3840",
    focal: "35.0 mm",
    aperture: "f / 2.8",
    swatches: ["#1e2636", "#3d4f6b", "#9fb1c8", "#c8b9a6", "#d9822b"],
    tone: "var(--gallery-tone-1)",
  },
  {
    id: 2,
    row: 2,
    grow: 1.0,
    res: "2560 × 2560",
    focal: "50.0 mm",
    aperture: "f / 1.8",
    swatches: ["#3e352d", "#8d7b68", "#c7b9a5", "#e7dfd3", "#c66900"],
    tone: "var(--gallery-tone-2)",
  },
  {
    id: 3,
    row: 2,
    grow: 1.62,
    res: "5040 × 2160",
    focal: "24.0 mm",
    aperture: "f / 4.0",
    swatches: ["#2b3230", "#556861", "#98a9a1", "#dce2de", "#c98b44"],
    tone: "var(--gallery-tone-3)",
  },
];

const AUTO_INTERVAL_MS = 7000;

const t = computed(() => copy[props.lang]);
const index = ref(0);
const active = computed(() => t.value.tabs[index.value]);
const version = releases[0]?.version ?? "";

const resolveHref = (href: string) =>
  href.startsWith("http") ? href : withBase(href);

const ctaHref = computed(() => resolveHref(t.value.ctaHref));
const activeHref = computed(() => resolveHref(active.value.href));

/* ---------- 比例态：比例档位切换 ---------- */

const ratioPresetIndex = ref(0);
const currentRatioPreset = computed(() => ratioPresets[ratioPresetIndex.value]);

/* ---------- 超清态：分辨率档位切换 ---------- */

const resPresetIndex = ref(1); // 默认 8K，对比最直观
const currentResPreset = computed(() => resPresets[resPresetIndex.value]);

/* ---------- 背景鼠标显影与镜头刻度环 ---------- */

const heroRef = ref<HTMLElement | null>(null);
const heroSpot = ref({ x: 68, y: 48, active: false });

function onHeroPointerMove(e: PointerEvent) {
  const el = heroRef.value;
  if (!el) return;
  const rect = el.getBoundingClientRect();
  heroSpot.value = {
    x: ((e.clientX - rect.left) / rect.width) * 100,
    y: ((e.clientY - rect.top) / rect.height) * 100,
    active: true,
  };
}

function onHeroPointerLeave() {
  heroSpot.value.active = false;
}

/* ---------- 录制态：计时与换尺寸自动切段 ---------- */

const recSeconds = ref(0);
const recLandscape = ref(true);
const recSegment = ref(1);
const showSplitHint = ref(false);
let recTimer: number | null = null;
let splitHintTimer: number | null = null;

const recTimecode = computed(() => {
  const total = recSeconds.value;
  const m = Math.floor(total / 600);
  const s = Math.floor((total % 600) / 10);
  const ds = total % 10;
  return `${String(m).padStart(2, "0")}:${String(s).padStart(2, "0")}.${ds}`;
});

function toggleRecordOrientation() {
  recLandscape.value = !recLandscape.value;
  recSegment.value = (recSegment.value % 9) + 1;
  recSeconds.value = 0;
  showSplitHint.value = true;
  if (splitHintTimer !== null) window.clearTimeout(splitHintTimer);
  splitHintTimer = window.setTimeout(() => {
    showSplitHint.value = false;
  }, 1800);
  restartAutoCycle();
}

/* ---------- 图库态：双栏联动选中 ---------- */

const activeGalleryIndex = ref(0);
const activeGalleryCard = computed(() => galleryCards[activeGalleryIndex.value]);
const row1Cards = galleryCards.filter((c) => c.row === 1);
const row2Cards = galleryCards.filter((c) => c.row === 2);

/* ---------- 动态画框比例与交互 ---------- */

const effectiveRatio = computed(() => {
  if (active.value.id === "ratio") {
    return currentRatioPreset.value.ratio;
  }
  if (active.value.id === "record") {
    return recLandscape.value ? 1.778 : 0.5625;
  }
  return active.value.ratio;
});

function cycleRatioPreset() {
  ratioPresetIndex.value = (ratioPresetIndex.value + 1) % ratioPresets.length;
  restartAutoCycle();
}

function selectResPreset(i: number) {
  resPresetIndex.value = i;
  restartAutoCycle();
}

function onFrameClick() {
  if (active.value.id === "ratio") {
    cycleRatioPreset();
  } else if (active.value.id === "superres") {
    selectResPreset((resPresetIndex.value + 1) % resPresets.length);
  } else if (active.value.id === "record") {
    toggleRecordOrientation();
  }
}

/* ---------- 自动轮播与悬停暂停 ---------- */

const isPaused = ref(false);
const cycleKey = ref(0);
let autoTimer: number | null = null;
let prefersReducedMotion = false;

function stopAutoCycle() {
  if (autoTimer !== null) {
    window.clearInterval(autoTimer);
    autoTimer = null;
  }
}

function startAutoCycle() {
  stopAutoCycle();
  if (prefersReducedMotion || isPaused.value) return;
  autoTimer = window.setInterval(() => {
    index.value = (index.value + 1) % t.value.tabs.length;
    cycleKey.value += 1;
  }, AUTO_INTERVAL_MS);
}

function restartAutoCycle() {
  cycleKey.value += 1;
  startAutoCycle();
}

function selectTab(i: number) {
  index.value = i;
  restartAutoCycle();
}

function onVisualEnter() {
  isPaused.value = true;
  stopAutoCycle();
}

function onVisualLeave() {
  isPaused.value = false;
  restartAutoCycle();
}

onMounted(() => {
  prefersReducedMotion =
    typeof window !== "undefined" &&
    window.matchMedia("(prefers-reduced-motion: reduce)").matches;
  startAutoCycle();

  if (!prefersReducedMotion) {
    recTimer = window.setInterval(() => {
      if (active.value.id === "record") {
        recSeconds.value = (recSeconds.value + 1) % 36000;
      }
    }, 100);
  }
});

onBeforeUnmount(() => {
  stopAutoCycle();
  if (recTimer !== null) window.clearInterval(recTimer);
  if (splitHintTimer !== null) window.clearTimeout(splitHintTimer);
});
</script>

<template>
  <section
    ref="heroRef"
    class="hero"
    :style="{
      '--spot-x': `${heroSpot.x}%`,
      '--spot-y': `${heroSpot.y}%`,
      '--spot-opacity': heroSpot.active ? '1' : '0',
    }"
    @pointermove="onHeroPointerMove"
    @pointerleave="onHeroPointerLeave"
  >
    <!-- 通栏背景：点阵 + 制图参考线 + 交点微标 + 鼠标柔光显影 -->
    <div class="ambient" aria-hidden="true">
      <div class="ambient-dots" />
      <div class="ambient-dots-lit" />
      <div class="ambient-spotlight" />

      <!-- 横向通栏参考线（左右边缘渐隐） -->
      <span class="guide-h top" />
      <span class="guide-h bottom" />

      <!-- 纵向参考线与交点微标（精准锚定内容容器两侧） -->
      <div class="guide-rail">
        <span class="guide-v left" />
        <span class="guide-v right" />

        <svg class="guide-node star-tl" viewBox="0 0 14 14" width="13" height="13">
          <path d="M7 0.5 L8.7 5.3 L13.5 7 L8.7 8.7 L7 13.5 L5.3 8.7 L0.5 7 L5.3 5.3 Z" />
        </svg>
        <svg class="guide-node cross-tr" viewBox="0 0 12 12" width="11" height="11">
          <path d="M6 0.5 V11.5 M0.5 6 H11.5" />
        </svg>
        <svg class="guide-node cross-bl" viewBox="0 0 12 12" width="11" height="11">
          <path d="M6 0.5 V11.5 M0.5 6 H11.5" />
        </svg>
        <svg class="guide-node star-br" viewBox="0 0 14 14" width="12" height="12">
          <path d="M7 0.5 L8.7 5.3 L13.5 7 L8.7 8.7 L7 13.5 L5.3 8.7 L0.5 7 L5.3 5.3 Z" />
        </svg>
      </div>
    </div>

    <div class="hero-inner">
      <!-- 左侧主文案与入口 -->
      <div class="hero-copy">
        <h1 class="title">{{ t.name }}</h1>
        <p class="title-en" :lang="lang === 'zh' ? 'en' : 'zh'">{{ t.nameEn }}</p>
        <p class="lede">
          <template v-for="(line, li) in t.tagline" :key="li"><br v-if="li > 0" class="lede-break" />{{ line }}</template>
        </p>

        <div class="actions">
          <a class="btn" :href="ctaHref">{{ t.cta }}</a>
          <p class="meta">{{ t.meta(version) }}</p>
        </div>
      </div>

      <!-- 右侧：极简光学取景舞台 × 真实功能演示 -->
      <div
        class="visual"
        :class="`mode-${active.id}`"
        @mouseenter="onVisualEnter"
        @mouseleave="onVisualLeave"
      >
        <!-- 暗色模式下锚定于舞台中心的柔和底光 -->
        <div class="stage-aura" aria-hidden="true" />

        <div class="stage" :style="{ '--ratio': String(effectiveRatio) }">
          <!-- 正交舞台锚点：固定外围坐标十字与中心标尺刻痕 -->
          <div class="stage-reticle" aria-hidden="true">
            <span class="reticle-corner tl" />
            <span class="reticle-corner tr" />
            <span class="reticle-corner bl" />
            <span class="reticle-corner br" />
            <span class="reticle-tick top" />
            <span class="reticle-tick bottom" />
            <span class="reticle-tick left" />
            <span class="reticle-tick right" />
          </div>
        <div
          class="frame"
          :class="{
            'is-gallery': active.id === 'gallery',
            'is-interactive': active.id !== 'gallery',
          }"
          @click="onFrameClick"
        >
          <!-- 超清 / 比例 / 录制共用：极简大喵轮廓主体及各自的取景辅助 -->
          <template v-if="active.id !== 'gallery'">
            <div class="momo-canvas" aria-hidden="true">
              <svg
                class="momo-svg"
                :viewBox="active.id === 'record' ? '0 0 1800 2120' : '0 0 1508 1775'"
                fill="currentColor"
                focusable="false"
              >
                <path
                  v-if="active.id === 'record'"
                  :d="ZONGZI_MOMO_OUTER_PATH"
                  fill="none"
                  stroke="currentColor"
                  stroke-width="22"
                  stroke-linejoin="round"
                />
                <g v-else transform="matrix(1.06787 0 0 1.06787 -216.778 -448.507)">
                  <path fill-rule="evenodd" :d="MOMO_OUTLINE_PATH" />
                </g>
              </svg>
            </div>

            <span v-if="active.id !== 'superres'" class="thirds-grid" aria-hidden="true" />

            <!-- 超清态：内嵌物理 1080P 屏幕参照虚线框，直观表现突破屏幕边界 -->
            <div
              v-if="active.id === 'superres'"
              class="screen-bounds"
              :style="{
                width: `${currentResPreset.screenScale * 100}%`,
                height: `${currentResPreset.screenScale * 100}%`,
              }"
              aria-hidden="true"
            >
              <span class="screen-bounds-tag">{{ t.screenBounds }}</span>
            </div>

            <!-- 左上角状态标识 -->
            <span v-if="active.id === 'ratio'" class="hud-top-left tnum">
              {{ currentRatioPreset.label }}
            </span>

            <span v-else-if="active.id === 'superres'" class="hud-top-left tnum">
              <span class="hud-accent">{{ currentResPreset.label }}</span>
              <span class="hud-sep">·</span>
              <span>{{ currentResPreset.dim }}</span>
            </span>

            <span v-else-if="active.id === 'record'" class="hud-top-left rec-badge tnum">
              <span class="rec-dot" aria-hidden="true" />
              <span>{{ recTimecode }}</span>
            </span>

            <!-- 右上角纯文本指标（仅超清态保留像素量） -->
            <span v-if="active.id === 'superres'" class="hud-top-right tnum">
              {{ currentResPreset.mp }}
            </span>

            <!-- 底部信息栏 -->
            <div v-if="active.id === 'ratio'" class="hud-bottom-right tnum">
              {{ currentRatioPreset.dim }}
            </div>

            <div v-else-if="active.id === 'superres'" class="hud-bottom-bar">
              <div class="res-switches" role="group">
                <button
                  v-for="(preset, pi) in resPresets"
                  :key="preset.id"
                  type="button"
                  class="res-switch"
                  :aria-pressed="pi === resPresetIndex"
                  @click.stop="selectResPreset(pi)"
                >
                  {{ preset.label }}
                </button>
              </div>
              <span class="hud-mono tnum">OVERLAY</span>
            </div>

            <div v-else-if="active.id === 'record'" class="hud-bottom-bar">
              <span class="hud-mono tnum">
                <template v-if="showSplitHint">
                  <span class="hud-accent">{{ t.recordSplit }} #0{{ recSegment }}</span>
                </template>
                <template v-else>
                  {{ recLandscape ? "16:9" : "9:16" }}
                </template>
              </span>
              <span v-if="!showSplitHint" class="hud-mono">MP4 · GPU</span>
            </div>
          </template>

          <!-- 图库态：极简双栏照片流 + 色彩与相机参数 -->
          <div v-else class="gallery-app">
            <div class="g-body">
              <!-- 主区：日期分组自适应照片流 -->
              <div class="g-main">
                <div class="g-timeline">
                  <div class="g-date-row">
                    <span class="g-date-label">{{ t.gallery.todayGroup }}</span>
                    <span class="g-date-count tnum">{{ t.gallery.todayCount }}</span>
                  </div>

                  <!-- 自适应比例行 1 -->
                  <div class="g-adaptive-row">
                    <button
                      v-for="card in row1Cards"
                      :key="card.id"
                      type="button"
                      class="g-photo"
                      :class="{ active: activeGalleryIndex === card.id }"
                      :style="{
                        flex: String(card.grow),
                        '--tone': card.tone,
                      }"
                      @mouseenter="activeGalleryIndex = card.id"
                      @focus="activeGalleryIndex = card.id"
                      @click.stop="activeGalleryIndex = card.id"
                    />
                  </div>

                  <!-- 自适应比例行 2 -->
                  <div class="g-adaptive-row">
                    <button
                      v-for="card in row2Cards"
                      :key="card.id"
                      type="button"
                      class="g-photo"
                      :class="{ active: activeGalleryIndex === card.id }"
                      :style="{
                        flex: String(card.grow),
                        '--tone': card.tone,
                      }"
                      @mouseenter="activeGalleryIndex = card.id"
                      @focus="activeGalleryIndex = card.id"
                      @click.stop="activeGalleryIndex = card.id"
                    />
                  </div>

                  <!-- 下一日期分组露出，暗示连续时间流 -->
                  <div class="g-date-row is-muted" aria-hidden="true">
                    <span class="g-date-label">{{ t.gallery.earlierGroup }}</span>
                    <span class="g-date-count tnum">{{ t.gallery.earlierCount }}</span>
                  </div>
                </div>
              </div>

              <!-- 右栏：主色调 + 分辨率 + 无限暖暖相机参数 -->
              <aside class="g-inspector">
                <!-- 主色调五色圆点 -->
                <div class="g-swatches" aria-hidden="true">
                  <span
                    v-for="(sw, si) in activeGalleryCard.swatches"
                    :key="`${activeGalleryCard.id}-${si}`"
                    class="g-swatch"
                    :style="{ background: sw }"
                  />
                </div>

                <div class="g-meta-list">
                  <div class="g-meta-row">
                    <span>{{ t.gallery.resLabel }}</span>
                    <span class="g-meta-val tnum">{{ activeGalleryCard.res }}</span>
                  </div>
                </div>

                <div class="g-meta-section">{{ t.gallery.sectionNikki }}</div>
                <div class="g-meta-list">
                  <div class="g-meta-row">
                    <span>{{ t.gallery.codeLabel }}</span>
                    <span class="g-copy-chip">{{ t.gallery.copyLabel }}</span>
                  </div>
                  <div class="g-meta-row">
                    <span>{{ t.gallery.focalLabel }}</span>
                    <span class="g-meta-val tnum">{{ activeGalleryCard.focal }}</span>
                  </div>
                  <div class="g-meta-row">
                    <span>{{ t.gallery.apertureLabel }}</span>
                    <span class="g-meta-val tnum">{{ activeGalleryCard.aperture }}</span>
                  </div>
                </div>
              </aside>
            </div>
          </div>
        </div>
      </div>

      <!-- 极简无框下划线切换栏 -->
      <div class="mode-tabs" role="tablist" :aria-label="t.groupLabel">
        <button
          v-for="(item, i) in t.tabs"
          :key="item.id"
          type="button"
          role="tab"
          class="mode-tab"
          :class="{ 'is-paused': i === index && isPaused }"
          :aria-selected="i === index"
          @click="selectTab(i)"
        >
          <span>{{ item.tab }}</span>
          <span
            v-if="i === index && !isPaused"
            :key="`${cycleKey}-${i}`"
            class="tab-progress"
            aria-hidden="true"
          />
        </button>
      </div>

      <!-- 单行简洁说明 + 文档直达链 -->
      <p class="caption" aria-live="polite">
        <span>{{ active.text }}</span>
        <a class="caption-link" :href="activeHref">
          {{ active.linkLabel }}<span aria-hidden="true"> →</span>
        </a>
      </p>
      </div>
    </div>
  </section>
</template>

<style scoped>
.hero {
  position: relative;
  --guide-y: clamp(44px, 11vh, 96px);
  display: flex;
  align-items: center;
  flex: 1 0 auto;
  width: 100%;
  overflow: clip;
}

.hero-inner {
  position: relative;
  z-index: 1;
  display: grid;
  grid-template-columns: minmax(0, 0.86fr) minmax(0, 1.14fr);
  grid-template-areas: "copy visual";
  align-items: center;
  column-gap: clamp(32px, 5.2vw, 72px);
  width: 100%;
  max-width: var(--shell);
  margin-inline: auto;
  padding-inline: var(--gutter);
  padding-block: clamp(36px, 7vh, 72px);
}

.hero-copy {
  grid-area: copy;
  display: flex;
  flex-direction: column;
  align-items: flex-start;
  align-self: stretch;
  min-width: 0;
  padding-top: 14px;
}

/* ---------- 通栏背景层：点阵 + 制图参考线 + 交点微标 + 鼠标显影 ---------- */

.ambient {
  position: absolute;
  inset: 0;
  pointer-events: none;
  z-index: 0;
  -webkit-mask-image: linear-gradient(
    to bottom,
    transparent 0%,
    #000 9%,
    #000 91%,
    transparent 100%
  );
  mask-image: linear-gradient(
    to bottom,
    transparent 0%,
    #000 9%,
    #000 91%,
    transparent 100%
  );
}

.ambient-dots,
.ambient-dots-lit {
  position: absolute;
  inset: 0;
  background-size: 24px 24px;
}

.ambient-dots {
  background-image: radial-gradient(var(--hero-dot) 1px, transparent 1px);
  -webkit-mask-image: radial-gradient(ellipse 64% 76% at 56% 50%, #000 22%, transparent 80%);
  mask-image: radial-gradient(ellipse 64% 76% at 56% 50%, #000 22%, transparent 80%);
}

.ambient-dots-lit {
  background-image: radial-gradient(var(--hero-dot-lit) 1.1px, transparent 1.1px);
  opacity: calc(var(--spot-opacity, 0) * var(--hero-spot-enabled, 1));
  transition: opacity 380ms var(--ease-out);
  -webkit-mask-image: radial-gradient(
    220px circle at var(--spot-x, 68%) var(--spot-y, 48%),
    #000 0%,
    transparent 100%
  );
  mask-image: radial-gradient(
    220px circle at var(--spot-x, 68%) var(--spot-y, 48%),
    #000 0%,
    transparent 100%
  );
}

.ambient-spotlight {
  position: absolute;
  inset: 0;
  opacity: calc(var(--spot-opacity, 0) * var(--hero-spot-enabled, 1));
  transition: opacity 420ms var(--ease-out);
  background: radial-gradient(
    360px circle at var(--spot-x, 68%) var(--spot-y, 48%),
    var(--brand-wash),
    transparent 72%
  );
}

/* 横向通栏制图线 */
.guide-h {
  position: absolute;
  left: 0;
  right: 0;
  height: 1px;
  background: linear-gradient(
    to right,
    transparent 0%,
    var(--hero-line) 12%,
    var(--hero-line) 88%,
    transparent 100%
  );
}

.guide-h.top {
  top: var(--guide-y);
}

.guide-h.bottom {
  bottom: var(--guide-y);
}

/* 纵向参考线容器：与内容区对齐并留出透气边距 */
.guide-rail {
  position: absolute;
  inset-block: 0;
  left: 50%;
  transform: translateX(-50%);
  width: min(calc(100% - 24px), calc(var(--shell) + 56px));
}

.guide-v {
  position: absolute;
  top: 0;
  bottom: 0;
  width: 1px;
  background: linear-gradient(
    to bottom,
    transparent 0%,
    var(--hero-line) 12%,
    var(--hero-line) 88%,
    transparent 100%
  );
}

.guide-v.left {
  left: 0;
}

.guide-v.right {
  right: 0;
}

.guide-node {
  position: absolute;
  display: block;
}

.guide-node.star-tl,
.guide-node.star-br {
  fill: var(--brand);
  opacity: 0.42;
  animation: node-calm-breathe 6.4s ease-in-out infinite;
}

.guide-node.star-tl {
  top: var(--guide-y);
  left: 0;
  transform: translate(-50%, -50%);
}

.guide-node.star-br {
  bottom: var(--guide-y);
  right: 0;
  transform: translate(50%, 50%);
  animation-delay: -3.2s;
}

.guide-node.cross-tr,
.guide-node.cross-bl {
  fill: none;
  stroke: var(--line-strong);
  stroke-width: 1;
  opacity: 0.85;
}

.guide-node.cross-tr {
  top: var(--guide-y);
  right: 0;
  transform: translate(50%, -50%);
}

.guide-node.cross-bl {
  bottom: var(--guide-y);
  left: 0;
  transform: translate(-50%, 50%);
}

@keyframes node-calm-breathe {
  0%,
  100% {
    opacity: 0.26;
  }
  50% {
    opacity: 0.56;
  }
}

/* ---------- 左侧排版 ---------- */

.title,
.title-en,
.lede,
.actions,
.visual {
  position: relative;
  z-index: 1;
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
  margin-top: clamp(4px, 0.7vh, 6px);
  font-size: clamp(2.5rem, 4.2vw, 3.35rem);
  font-weight: 600;
  line-height: 1.1;
  letter-spacing: -0.015em;
  color: var(--ink);
}

.lede {
  grid-area: lede;
  margin-top: clamp(26px, 3.8vh, 36px);
  max-width: 22em;
  font-size: clamp(1.15rem, 1.5vw, 1.2rem);
  line-height: 1.7;
  color: var(--ink-2);
  text-wrap: balance;
}

.lede-break {
  display: inline;
}

.actions {
  grid-area: actions;
  margin-top: auto;
  padding-top: clamp(32px, 4.5vh, 48px);
}

.meta {
  margin-top: clamp(18px, 2.4vh, 24px);
  font-size: 12.5px;
  line-height: 1.6;
  color: var(--ink-3);
  letter-spacing: 0.01em;
  font-variant-numeric: tabular-nums;
}

/* ---------- 右侧极简取景舞台 ---------- */

.visual {
  grid-area: visual;
  --frame-h: clamp(232px, 33vh, 294px);
  display: flex;
  flex-direction: column;
  align-items: center;
  align-self: center;
  min-width: 0;
}

/* 暗色模式下锚定在取景舞台背后的柔和底光 */
.stage-aura {
  position: absolute;
  left: 50%;
  top: calc((var(--frame-h) + 28px) / 2);
  width: clamp(440px, 40vw, 580px);
  aspect-ratio: 1;
  transform: translate(-50%, -50%);
  background: radial-gradient(
    circle,
    var(--brand-wash) 0%,
    transparent 68%
  );
  opacity: var(--stage-aura-opacity);
  pointer-events: none;
  z-index: 0;
}

.stage,
.mode-tabs,
.caption {
  position: relative;
  z-index: 1;
}

.stage {
  position: relative;
  display: flex;
  align-items: center;
  justify-content: center;
  width: 100%;
  height: calc(var(--frame-h) + 28px);
  container-type: inline-size;
}

/* 正交舞台锚点：四角精密十字 + 四边轴心微标 */
.stage-reticle {
  position: absolute;
  inset: 0;
  pointer-events: none;
  z-index: 0;
}

.reticle-corner {
  position: absolute;
  width: 9px;
  height: 9px;
  color: var(--line-strong);
  opacity: 0.65;
}

.reticle-corner::before,
.reticle-corner::after {
  content: "";
  position: absolute;
  background: currentColor;
}

.reticle-corner::before {
  left: 50%;
  top: 0;
  bottom: 0;
  width: 1px;
  transform: translateX(-50%);
}

.reticle-corner::after {
  top: 50%;
  left: 0;
  right: 0;
  height: 1px;
  transform: translateY(-50%);
}

.reticle-corner.tl {
  top: 0;
  left: 0;
}

.reticle-corner.tr {
  top: 0;
  right: 0;
}

.reticle-corner.bl {
  bottom: 0;
  left: 0;
}

.reticle-corner.br {
  bottom: 0;
  right: 0;
}

.reticle-tick {
  position: absolute;
  background: var(--brand);
  opacity: 0.4;
}

.reticle-tick.top,
.reticle-tick.bottom {
  left: 50%;
  width: 1px;
  height: 7px;
  transform: translateX(-50%);
}

.reticle-tick.top {
  top: 1px;
}

.reticle-tick.bottom {
  bottom: 1px;
}

.reticle-tick.left,
.reticle-tick.right {
  top: 50%;
  width: 7px;
  height: 1px;
  transform: translateY(-50%);
}

.reticle-tick.left {
  left: 1px;
}

.reticle-tick.right {
  right: 1px;
}

.frame {
  --cap: calc((100cqw - 36px) / var(--ratio, 1.778));
  position: relative;
  height: min(var(--frame-h), var(--cap));
  width: calc(min(var(--frame-h), var(--cap)) * var(--ratio, 1.778));
  border: 1px solid color-mix(in srgb, var(--line) 80%, transparent);
  border-radius: 4px;
  background: var(--frame-bg);
  overflow: hidden;
  user-select: none;
  transition:
    width 560ms var(--ease-out),
    height 560ms var(--ease-out),
    border-color var(--dur-fast) var(--ease-out);
  will-change: width, height;
}

.frame.is-interactive {
  cursor: pointer;
}

.frame.is-interactive:hover {
  border-color: var(--brand-line);
}

.frame.is-gallery {
  background: var(--frame-surface);
}

/* 大喵极简轮廓画布 */
.momo-canvas {
  position: absolute;
  inset: 0;
  z-index: 1;
  display: flex;
  align-items: center;
  justify-content: center;
  padding: 24px 20px 26px;
  color: var(--momo-ink);
  pointer-events: none;
  transition:
    transform 560ms var(--ease-out),
    color 300ms var(--ease-out);
}

.momo-svg {
  height: 100%;
  width: auto;
  max-width: 82%;
  display: block;
}

/* 三分参考线只画框内两横两竖，外沿由画框本身绘制。 */
.thirds-grid {
  --grid-stroke: linear-gradient(var(--grid-line), var(--grid-line));
  position: absolute;
  inset: 0;
  pointer-events: none;
  opacity: 0.7;
  background:
    var(--grid-stroke) calc(100% / 3) 0 / 1px 100% no-repeat,
    var(--grid-stroke) calc(200% / 3) 0 / 1px 100% no-repeat,
    var(--grid-stroke) 0 calc(100% / 3) / 100% 1px no-repeat,
    var(--grid-stroke) 0 calc(200% / 3) / 100% 1px no-repeat;
}

/* 超清态：物理屏幕虚线参照框（退居大喵轮廓后方，标注引出至右侧空白区） */
.screen-bounds {
  position: absolute;
  left: 50%;
  top: 50%;
  transform: translate(-50%, -50%);
  border: 1px dashed color-mix(in srgb, var(--brand) 34%, transparent);
  border-radius: 3px;
  background: color-mix(in srgb, var(--brand) 5%, transparent);
  pointer-events: none;
  z-index: 0;
  transition:
    width 460ms var(--ease-out),
    height 460ms var(--ease-out);
}

.screen-bounds-tag {
  position: absolute;
  left: calc(100% + 12px);
  top: 50%;
  transform: translateY(-50%);
  font-family: var(--font-mono);
  font-size: 9.5px;
  line-height: 1.25;
  letter-spacing: 0.05em;
  color: var(--ink-3);
  white-space: nowrap;
}

.screen-bounds-tag::before {
  content: "";
  position: absolute;
  right: calc(100% + 4px);
  top: 50%;
  width: 6px;
  height: 1px;
  background: color-mix(in srgb, var(--brand) 38%, transparent);
  transform: translateY(-50%);
}

/* 取景框 HUD 元素 */
.hud-top-left {
  position: absolute;
  top: 12px;
  left: 15px;
  z-index: 4;
  display: inline-flex;
  align-items: center;
  gap: 5px;
  font-family: var(--font-mono);
  font-size: 10.5px;
  font-weight: 500;
  letter-spacing: 0.06em;
  color: var(--ink-2);
  user-select: none;
}

.hud-top-right {
  position: absolute;
  top: 12px;
  right: 15px;
  z-index: 4;
  font-family: var(--font-mono);
  font-size: 10px;
  letter-spacing: 0.06em;
  color: var(--ink-3);
  user-select: none;
}

.hud-accent {
  color: var(--brand-ink);
  font-weight: 600;
}

.hud-sep {
  opacity: 0.45;
}

.hud-bottom-right {
  position: absolute;
  right: 15px;
  bottom: 12px;
  z-index: 4;
  font-family: var(--font-mono);
  font-size: 10px;
  letter-spacing: 0.06em;
  color: var(--ink-3);
  user-select: none;
}

.hud-bottom-bar {
  position: absolute;
  left: 15px;
  right: 15px;
  bottom: 11px;
  z-index: 4;
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 8px;
}

.hud-mono {
  font-family: var(--font-mono);
  font-size: 9.5px;
  letter-spacing: 0.06em;
  color: var(--ink-3);
  user-select: none;
  white-space: nowrap;
}

.res-switches {
  display: inline-flex;
  align-items: center;
  gap: 10px;
}

.res-switch {
  padding: 2px 0;
  border: 0;
  background: transparent;
  font-family: var(--font-mono);
  font-size: 10px;
  font-weight: 500;
  letter-spacing: 0.05em;
  color: var(--ink-3);
  cursor: pointer;
  transition: color var(--dur-fast) var(--ease-out);
}

.res-switch:hover {
  color: var(--ink);
}

.res-switch[aria-pressed="true"] {
  color: var(--brand-ink);
  font-weight: 600;
}

.rec-badge {
  color: var(--ink);
}

.rec-dot {
  width: 7px;
  height: 7px;
  border-radius: 999px;
  background: var(--rec-dot);
  animation: rec-pulse 1.3s ease-in-out infinite;
}

@keyframes rec-pulse {
  0%,
  100% {
    opacity: 1;
    transform: scale(1);
  }
  50% {
    opacity: 0.35;
    transform: scale(0.84);
  }
}

/* ---------- 图库态：极简双栏画布 ---------- */

.gallery-app {
  width: 100%;
  height: 100%;
  font-size: 10px;
  color: var(--ink-2);
  user-select: none;
}

.g-body {
  display: grid;
  grid-template-columns: minmax(0, 1fr) 32%;
  height: 100%;
}

/* 主区：日期自适应照片流 */
.g-main {
  display: flex;
  flex-direction: column;
  min-width: 0;
  padding: 12px 14px;
  overflow: hidden;
}

.g-timeline {
  display: flex;
  flex-direction: column;
  gap: 7px;
  flex: 1;
  min-height: 0;
}

.g-date-row {
  display: flex;
  align-items: baseline;
  justify-content: space-between;
  line-height: 1.2;
}

.g-date-label {
  font-size: 10px;
  font-weight: 500;
  color: var(--ink);
}

.g-date-count {
  font-size: 9px;
  color: var(--ink-3);
}

.g-date-row.is-muted {
  margin-top: 2px;
  opacity: 0.5;
}

.g-adaptive-row {
  display: flex;
  gap: 7px;
  flex: 1;
  min-height: 48px;
}

.g-photo {
  border-radius: 4px;
  border: 1.5px solid transparent;
  background: var(--tone);
  padding: 0;
  cursor: pointer;
  transition:
    border-color var(--dur-fast) var(--ease-out),
    transform var(--dur-fast) var(--ease-out);
}

.g-photo:hover {
  border-color: var(--brand-line);
}

.g-photo.active {
  border-color: var(--brand);
}

/* 右栏：色彩与相机参数 */
.g-inspector {
  display: flex;
  flex-direction: column;
  justify-content: center;
  gap: 10px;
  padding: 14px 13px;
  background: var(--frame-subtle);
  overflow: hidden;
}

.g-swatches {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 4px;
  padding-bottom: 2px;
}

.g-swatch {
  width: 12px;
  height: 12px;
  border-radius: 999px;
  box-shadow: inset 0 0 0 1px color-mix(in srgb, var(--ink) 12%, transparent);
  transition: background 240ms var(--ease-out);
}

.g-meta-section {
  margin-top: 4px;
  font-size: 10px;
  font-weight: 400;
  color: var(--ink);
  letter-spacing: 0.01em;
}

.g-meta-list {
  display: flex;
  flex-direction: column;
  gap: 6px;
}

.g-meta-row {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 4px;
  font-size: 9px;
  line-height: 1.3;
  color: var(--ink-3);
}

.g-meta-val {
  color: var(--ink);
  font-family: var(--font-mono);
  font-size: 9px;
  white-space: nowrap;
}

.g-copy-chip {
  padding: 1px 6px;
  border-radius: 4px;
  background: var(--brand-wash);
  font-size: 8.5px;
  font-weight: 500;
  color: var(--brand-ink);
}

/* ---------- 极简无框下划线切换栏 ---------- */

.mode-tabs {
  display: inline-flex;
  align-items: center;
  gap: 6px;
  margin-top: 20px;
  border-bottom: 1px solid color-mix(in srgb, var(--line) 80%, transparent);
  user-select: none;
}

.mode-tab {
  position: relative;
  padding: 8px 14px 10px;
  border: 0;
  background: transparent;
  font-size: 13.5px;
  font-weight: 500;
  line-height: 1.3;
  color: var(--ink-3);
  cursor: pointer;
  white-space: nowrap;
  transition: color var(--dur-fast) var(--ease-out);
}

.mode-tab:hover {
  color: var(--ink);
}

.mode-tab[aria-selected="true"] {
  color: var(--ink);
}

.mode-tab[aria-selected="true"]::after {
  content: "";
  position: absolute;
  left: 10px;
  right: 10px;
  bottom: -1px;
  height: 2px;
  border-radius: 2px;
  background: var(--brand-line);
}

.mode-tab.is-paused[aria-selected="true"]::after {
  background: var(--brand);
}

.tab-progress {
  position: absolute;
  left: 10px;
  right: 10px;
  bottom: -1px;
  height: 2px;
  border-radius: 2px;
  background: var(--brand);
  transform-origin: left center;
  animation: progress-fill 7000ms linear 1 forwards;
}

@keyframes progress-fill {
  from {
    transform: scaleX(0);
    opacity: 0.55;
  }
  to {
    transform: scaleX(1);
    opacity: 1;
  }
}

/* ---------- 单行简洁说明与直达链 ---------- */

.caption {
  margin-top: 12px;
  min-height: 1.6em;
  font-size: 13px;
  line-height: 1.6;
  color: var(--ink-3);
  text-align: center;
  text-wrap: balance;
}

.caption-link {
  display: inline-block;
  margin-left: 8px;
  color: var(--brand-ink);
  font-weight: 500;
  text-decoration: none;
  white-space: nowrap;
  transition: color var(--dur-fast) var(--ease-out);
}

.caption-link:hover {
  color: var(--brand);
}

/* ---------- 减少动效偏好 ---------- */

@media (prefers-reduced-motion: reduce) {
  .rec-dot,
  .tab-progress,
  .guide-node.star-tl,
  .guide-node.star-br {
    animation: none !important;
  }
}

/* ---------- 窄屏竖向排布 ---------- */

@media (max-width: 900px) {
  .hero-inner {
    grid-template-columns: minmax(0, 1fr);
    grid-template-areas:
      "title"
      "en"
      "lede"
      "visual"
      "actions";
    align-content: start;
    padding-block: clamp(24px, 5vh, 40px) clamp(36px, 7vh, 56px);
  }

  .hero-copy {
    display: contents;
  }

  .guide-h,
  .guide-rail {
    display: none;
  }

  .lede {
    max-width: none;
  }

  .lede-break {
    display: none;
  }

  .visual {
    --frame-h: min(244px, calc((100vw - var(--gutter) * 2 - 24px) / 1.56));
    margin-block: clamp(24px, 4.5vh, 38px) 0;
    width: 100%;
  }

  .stage {
    height: calc(var(--frame-h) + 22px);
  }

  .actions {
    margin-top: clamp(28px, 5vh, 42px);
    padding-top: 0;
  }

  .actions .btn {
    width: 100%;
  }

  .meta {
    text-align: center;
  }
}

@media (max-width: 520px) {
  .screen-bounds-tag {
    left: calc(100% + 7px);
    font-size: 8.5px;
    letter-spacing: 0.03em;
    white-space: normal;
    width: max-content;
    max-width: 52px;
  }

  .screen-bounds-tag::before {
    right: calc(100% + 2px);
    width: 4px;
  }

  .g-body {
    grid-template-columns: minmax(0, 1fr) 38%;
  }

  .mode-tab {
    padding: 7px 11px 9px;
    font-size: 13px;
  }
}
</style>

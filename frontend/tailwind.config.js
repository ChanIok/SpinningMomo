/** @type {import('tailwindcss').Config} */
export default {
  content: [
    "./index.html",
    "./src/**/*.{vue,js,ts,jsx,tsx}",
  ],
  theme: {
    extend: {
      colors: {
        // 可以根据NaiveUI的主题色调整
        primary: {
          DEFAULT: 'var(--n-primary-color)',
        },
        // 背景色变量
        background: {
          DEFAULT: 'var(--n-color)',
          hover: 'var(--n-color-hover)',
        },
      },
      spacing: {
        // 定义常用间距
        'page': '1rem', // 页面四周留白
      },
    },
  },
  plugins: [],
}

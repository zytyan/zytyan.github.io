import { defineConfig } from 'vitepress';
import mathjax3 from 'markdown-it-mathjax3';

export default defineConfig({
  title: 'Ytyan的博客',
  description: 'Ytyan的技术博客',
  lang: 'zh-CN',
  cleanUrls: true,
  markdown: {
    config(md) {
      md.use(mathjax3);
    },
  },
  themeConfig: {
    nav: [
      { text: '首页', link: '/' },
    ],
    search: {
      provider: 'local',
    },
    outline: {
      label: '目录',
      level: [2, 4],
    },
    docFooter: {
      prev: '上一篇',
      next: '下一篇',
    },
    lastUpdated: {
      text: '最后更新',
      formatOptions: {
        dateStyle: 'short',
        timeStyle: 'medium',
      },
    },
  },
});

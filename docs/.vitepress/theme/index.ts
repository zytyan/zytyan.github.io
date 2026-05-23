import DefaultTheme from 'vitepress/theme';
import { h } from 'vue';
import CollapsibleOutline from './CollapsibleOutline.vue';
import './style.css';

export default {
  extends: DefaultTheme,
  Layout() {
    return h(DefaultTheme.Layout, null, {
      'aside-outline-before': () => h(CollapsibleOutline),
    });
  },
};

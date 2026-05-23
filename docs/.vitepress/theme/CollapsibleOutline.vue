<script setup lang="ts">
import { defineComponent, h, nextTick, onMounted, onUnmounted, PropType, ref } from 'vue';
import { onContentUpdated } from 'vitepress';

type OutlineItem = {
  id: string;
  title: string;
  level: number;
  children: OutlineItem[];
};

const items = ref<OutlineItem[]>([]);
const activeId = ref('');
const openIds = ref(new Set<string>());

let headings: HTMLHeadingElement[] = [];

const OutlineNode = defineComponent({
  name: 'OutlineNode',
  props: {
    item: {
      type: Object as PropType<OutlineItem>,
      required: true,
    },
    activeId: {
      type: String,
      required: true,
    },
    openIds: {
      type: Object as PropType<Set<string>>,
      required: true,
    },
  },
  emits: ['toggle'],
  setup(props, { emit }) {
    return () => {
      const hasChildren = props.item.children.length > 0;
      const isOpen = props.openIds.has(props.item.id);
      const isActive = props.activeId === props.item.id;

      return h('li', {
        class: [
          'custom-outline-item',
          `level-${props.item.level}`,
          hasChildren && 'has-children',
          isOpen && 'is-open',
          isActive && 'is-active',
        ],
      }, [
        hasChildren && h('button', {
          class: 'custom-outline-toggle',
          type: 'button',
          'aria-label': isOpen ? '折叠目录' : '展开目录',
          'aria-expanded': String(isOpen),
          onClick: (event: Event) => {
            event.preventDefault();
            event.stopPropagation();
            emit('toggle', props.item.id);
          },
        }),
        h('a', {
          class: 'custom-outline-link',
          href: `#${props.item.id}`,
          title: props.item.title,
        }, props.item.title),
        hasChildren && isOpen && h('ul', { class: 'custom-outline-list nested' }, props.item.children.map((child) => h(OutlineNode, {
          key: child.id,
          item: child,
          activeId: props.activeId,
          openIds: props.openIds,
          onToggle: (id: string) => emit('toggle', id),
        }))),
      ]);
    };
  },
});

function headingTitle(heading: HTMLHeadingElement) {
  return Array.from(heading.childNodes)
    .filter((node) => !(node instanceof HTMLElement && node.classList.contains('header-anchor')))
    .map((node) => node.textContent || '')
    .join('')
    .trim();
}

function buildTree(source: HTMLHeadingElement[]) {
  const roots: OutlineItem[] = [];
  const stack: OutlineItem[] = [];

  source.forEach((heading) => {
    const item: OutlineItem = {
      id: heading.id,
      title: headingTitle(heading),
      level: Number(heading.tagName[1]),
      children: [],
    };

    while (stack.length && stack[stack.length - 1].level >= item.level) {
      stack.pop();
    }

    const parent = stack[stack.length - 1];
    if (parent) {
      parent.children.push(item);
    } else {
      roots.push(item);
    }

    stack.push(item);
  });

  return roots;
}

function collectParentIds(tree: OutlineItem[], id: string, parents: string[] = []): string[] {
  for (const item of tree) {
    if (item.id === id) return parents;
    const found = collectParentIds(item.children, id, [...parents, item.id]);
    if (found.length) return found;
  }

  return [];
}

function syncActive() {
  if (!headings.length) return;

  const top = window.scrollY + 96;
  let current = headings[0];

  for (const heading of headings) {
    if (heading.offsetTop <= top) {
      current = heading;
    } else {
      break;
    }
  }

  activeId.value = current.id;
  openIds.value = new Set([...openIds.value, ...collectParentIds(items.value, current.id)]);
}

function refreshOutline() {
  headings = Array.from(document.querySelectorAll<HTMLHeadingElement>('.VPDoc :where(h2,h3,h4)'))
    .filter((heading) => heading.id && headingTitle(heading));

  items.value = buildTree(headings);

  const firstWithChildren = items.value.find((item) => item.children.length);
  openIds.value = new Set(firstWithChildren ? [firstWithChildren.id] : []);

  nextTick(syncActive);
}

function toggle(id: string) {
  const next = new Set(openIds.value);
  if (next.has(id)) {
    next.delete(id);
  } else {
    next.add(id);
  }
  openIds.value = next;
}

onContentUpdated(refreshOutline);

onMounted(() => {
  refreshOutline();
  window.addEventListener('scroll', syncActive, { passive: true });
});

onUnmounted(() => {
  window.removeEventListener('scroll', syncActive);
});
</script>

<template>
  <nav v-if="items.length" class="CustomDocOutline" aria-label="目录">
    <div class="custom-outline-title">目录</div>
    <ul class="custom-outline-list root">
      <OutlineNode
        v-for="item in items"
        :key="item.id"
        :item="item"
        :active-id="activeId"
        :open-ids="openIds"
        @toggle="toggle"
      />
    </ul>
  </nav>
</template>

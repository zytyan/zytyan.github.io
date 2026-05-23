import { inBrowser } from 'vitepress';

const enhanced = new WeakSet<HTMLElement>();
let observer: MutationObserver | undefined;
let syncTimer: number | undefined;

function scheduleSync() {
  if (!inBrowser) return;
  window.clearTimeout(syncTimer);
  syncTimer = window.setTimeout(syncOutline, 50);
}

function setupItems(outline: HTMLElement) {
  const items = outline.querySelectorAll<HTMLLIElement>('.VPDocOutlineItem li');

  items.forEach((item) => {
    const nested = item.querySelector<HTMLElement>(':scope > .VPDocOutlineItem.nested');
    const link = item.querySelector<HTMLAnchorElement>(':scope > .outline-link');

    if (!nested || !link) return;

    item.classList.add('has-children');

    if (enhanced.has(item)) return;

    const button = document.createElement('button');
    button.className = 'outline-toggle';
    button.type = 'button';
    button.setAttribute('aria-label', '展开或折叠目录');

    button.addEventListener('click', (event) => {
      event.preventDefault();
      event.stopPropagation();
      item.classList.toggle('is-collapsed');
      button.setAttribute('aria-expanded', String(!item.classList.contains('is-collapsed')));
    });

    item.insertBefore(button, link);
    enhanced.add(item);
  });
}

function collapseInactiveBranches(outline: HTMLElement) {
  outline.querySelectorAll<HTMLLIElement>('.VPDocOutlineItem li.has-children').forEach((item) => {
    item.classList.add('is-collapsed');
    item.querySelector<HTMLButtonElement>(':scope > .outline-toggle')?.setAttribute('aria-expanded', 'false');
  });
}

function expandActiveBranch(outline: HTMLElement) {
  const active = outline.querySelector<HTMLAnchorElement>('.outline-link.active');

  if (!active) {
    const firstBranch = outline.querySelector<HTMLLIElement>('.VPDocOutlineItem.root > li.has-children');
    firstBranch?.classList.remove('is-collapsed');
    firstBranch?.querySelector<HTMLButtonElement>(':scope > .outline-toggle')?.setAttribute('aria-expanded', 'true');
    return;
  }

  let node: HTMLElement | null = active;
  while (node && node !== outline) {
    if (node.matches('li.has-children')) {
      node.classList.remove('is-collapsed');
      node.querySelector<HTMLButtonElement>(':scope > .outline-toggle')?.setAttribute('aria-expanded', 'true');
    }
    node = node.parentElement;
  }
}

function syncOutline() {
  const outline = document.querySelector<HTMLElement>('.VPDocAsideOutline');
  if (!outline) return;

  setupItems(outline);
  const signature = Array.from(outline.querySelectorAll<HTMLAnchorElement>('.outline-link'))
    .map((link) => link.getAttribute('href') || '')
    .join('|');

  if (outline.dataset.collapsibleSignature !== signature) {
    collapseInactiveBranches(outline);
    outline.dataset.collapsibleSignature = signature;
  }
  expandActiveBranch(outline);
}

export function setupCollapsibleOutline() {
  if (!inBrowser || observer) return;

  observer = new MutationObserver(scheduleSync);
  observer.observe(document.body, {
    attributes: true,
    childList: true,
    subtree: true,
    attributeFilter: ['class'],
  });

  window.addEventListener('scroll', scheduleSync, { passive: true });
  window.addEventListener('hashchange', scheduleSync);
  window.addEventListener('load', scheduleSync);
  scheduleSync();
}

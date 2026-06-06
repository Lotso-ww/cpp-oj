/* ==========================================================================
   Problem list — load, render, filter, search, logout.
   Pure DOM, no frameworks. Loaded by problem_list.html.
   ========================================================================== */

(function (global) {
  'use strict';

  /* ---------- DOM utilities ---------- */

  const $  = (sel, root = document) => root.querySelector(sel);
  const $$ = (sel, root = document) => Array.from(root.querySelectorAll(sel));

  function escapeHtml(s) {
    return String(s)
      .replace(/&/g, '&amp;')
      .replace(/</g, '&lt;')
      .replace(/>/g, '&gt;')
      .replace(/"/g, '&quot;')
      .replace(/'/g, '&#39;');
  }

  /* ---------- State ---------- */

  const state = {
    all:        [],      // full list from API
    difficulty: 'all',
    search:     '',
    loading:    false,
    error:      null,
    role:       null      // 'admin' | 'user' | null — set by /api/me
  };

  /* ---------- Lucide-style SVG fragments (inline) ---------- */

  const SVG = {
    arrowRight: '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" width="14" height="14" stroke-width="2"><path d="M5 12h14"/><path d="m12 5 7 7-7 7"/></svg>'
  };

  /* ---------- Difficulty bar (3-bar, level 1-3) ---------- */

  function difficultyBars(level) {
    let bars = '';
    for (let i = 0; i < 3; i++) {
      const active = i < level;
      const x = 1 + i * 7;          // 1px gutter, 3px bar, 3px gap
      bars += `<rect class="difficulty__bar${active ? ' is-active' : ''}" x="${x}" y="1" width="3" height="9" rx="0.5"/>`;
    }
    return `<svg class="difficulty__bars" width="22" height="11" viewBox="0 0 22 11" aria-hidden="true">${bars}</svg>`;
  }

  const DIFFICULTY_LABEL = { Easy: '简单', Medium: '中等', Hard: '困难' };
  const DIFFICULTY_LEVEL = { Easy: 1, Medium: 2, Hard: 3 };

  function difficultyTag(difficulty) {
    const lvl = DIFFICULTY_LEVEL[difficulty] || 1;
    const label = DIFFICULTY_LABEL[difficulty] || difficulty;
    return `<span class="difficulty" data-difficulty="${escapeHtml(difficulty)}">${difficultyBars(lvl)}<span class="difficulty__label">${escapeHtml(label)}</span></span>`;
  }

  /* ---------- Row rendering ---------- */

  function rowHtml(problem) {
    const href = `/problem.html?id=${encodeURIComponent(problem.id)}`;
    return `
      <tr class="problem-table__row" data-href="${href}">
        <td class="problem-table__cell problem-table__cell--id">${escapeHtml(problem.id)}</td>
        <td class="problem-table__cell problem-table__cell--title">
          <a href="${href}">${escapeHtml(problem.title)}</a>
        </td>
        <td class="problem-table__cell problem-table__cell--difficulty">
          ${difficultyTag(problem.difficulty)}
        </td>
        <td class="problem-table__cell problem-table__cell--action">
          <span class="problem-table__arrow">${SVG.arrowRight}</span>
        </td>
      </tr>`;
  }

  function rowsHtml(problems) {
    return problems.map(rowHtml).join('');
  }

  /* ---------- Skeleton (loading) ---------- */

  function skeletonRowsHtml(n) {
    let rows = '';
    for (let i = 0; i < n; i++) {
      rows += `
        <tr class="problem-table__row problem-table__row--skeleton" aria-hidden="true">
          <td class="problem-table__cell problem-table__cell--id"><span class="skeleton skeleton--id"></span></td>
          <td class="problem-table__cell problem-table__cell--title"><span class="skeleton skeleton--title"></span></td>
          <td class="problem-table__cell problem-table__cell--difficulty"><span class="skeleton skeleton--difficulty"></span></td>
          <td class="problem-table__cell problem-table__cell--action"></td>
        </tr>`;
    }
    return rows;
  }

  function skeletonHtml() {
    return `
      <table class="problem-table">
        <thead>
          <tr>
            <th>题号</th>
            <th>标题</th>
            <th>难度</th>
            <th></th>
          </tr>
        </thead>
        <tbody>${skeletonRowsHtml(6)}</tbody>
      </table>`;
  }

  /* ---------- Table rendering ---------- */

  function tableHtml(problems) {
    return `
      <table class="problem-table">
        <thead>
          <tr>
            <th>题号</th>
            <th>标题</th>
            <th>难度</th>
            <th></th>
          </tr>
        </thead>
        <tbody>${rowsHtml(problems)}</tbody>
      </table>`;
  }

  /* ---------- Empty state ---------- */

  function emptyStateHtml(title, description, actionLabel, actionId) {
    return `
      <div class="empty-state" role="status">
        <h2 class="empty-state__title">${escapeHtml(title)}</h2>
        <p class="empty-state__description">${escapeHtml(description)}</p>
        ${actionLabel ? `<button class="empty-state__action" type="button" data-action="${actionId}">${escapeHtml(actionLabel)}</button>` : ''}
      </div>`;
  }

  /* ---------- State → render ---------- */

  function render() {
    const wrapper = $('#tableWrapper');
    if (!wrapper) return;

    if (state.loading) {
      wrapper.innerHTML = skeletonHtml();
      wrapper.setAttribute('aria-busy', 'true');
      return;
    }

    if (state.error) {
      wrapper.innerHTML = emptyStateHtml(
        '无法加载题目',
        '请检查网络后重试。',
        '重试',
        'retry'
      );
      wrapper.setAttribute('aria-busy', 'false');
      return;
    }

    const filtered = applyFilter(state.all, state.difficulty, state.search);

    if (state.all.length === 0) {
      wrapper.innerHTML = emptyStateHtml(
        '暂无题目',
        '题库正在建设中，敬请期待。',
        null,
        null
      );
    } else if (filtered.length === 0) {
      wrapper.innerHTML = emptyStateHtml(
        '没有匹配的题目',
        '尝试调整过滤或搜索条件。',
        '查看全部',
        'reset-filter'
      );
    } else {
      wrapper.innerHTML = tableHtml(filtered);
      bindRowClicks();
    }

    wrapper.setAttribute('aria-busy', 'false');
  }

  /* ---------- Filter / search ---------- */

  function applyFilter(list, difficulty, search) {
    const q = (search || '').trim().toLowerCase();
    return list.filter((p) => {
      if (difficulty !== 'all' && p.difficulty !== difficulty) return false;
      if (!q) return true;
      if (String(p.id).startsWith(q)) return true;
      if (String(p.title || '').toLowerCase().indexOf(q) !== -1) return true;
      return false;
    });
  }

  /* ---------- Counts ---------- */

  function updateCounts() {
    const counts = { all: state.all.length, Easy: 0, Medium: 0, Hard: 0 };
    state.all.forEach((p) => {
      if (counts[p.difficulty] !== undefined) counts[p.difficulty]++;
    });
    $$('.filter__chip-count').forEach((el) => {
      const key = el.getAttribute('data-count');
      el.textContent = counts[key] != null ? counts[key] : '—';
    });
  }

  /* ---------- Row click → navigate (with link as real <a> for a11y) ---------- */

  function bindRowClicks() {
    $$('.problem-table__row', $('#tableWrapper')).forEach((row) => {
      // Only bind once per row, guard via flag
      if (row.dataset.bound === '1') return;
      row.dataset.bound = '1';
      row.addEventListener('click', (e) => {
        if (e.target.closest('a')) return; // user clicked the link directly
        const href = row.getAttribute('data-href');
        if (href) global.location.href = href;
      });
    });
  }

  /* ---------- API: load ---------- */

  function loadProblems() {
    state.loading = true;
    state.error = null;
    render();

    Api.listProblems().then((res) => {
      state.loading = false;
      if (!res.ok) {
        state.error = (res.data && res.data.error) || '加载失败';
        render();
        return;
      }
      const list = (res.data && res.data.problems) || [];
      // Sort by id ascending for stable display
      list.sort((a, b) => a.id - b.id);
      state.all = list;
      updateCounts();
      render();
    });
  }

  /* ---------- Filter chip binding ---------- */

  function bindFilter() {
    $$('.filter__chip').forEach((chip) => {
      chip.addEventListener('click', () => {
        const difficulty = chip.getAttribute('data-difficulty');
        if (difficulty === state.difficulty) return;
        state.difficulty = difficulty;
        $$('.filter__chip').forEach((c) => {
          const isActive = c === chip;
          c.classList.toggle('is-active', isActive);
          c.setAttribute('aria-selected', isActive ? 'true' : 'false');
        });
        render();
      });
    });
  }

  /* ---------- Search input binding ---------- */

  function bindSearch() {
    const input = $('#searchInput');
    if (!input) return;
    let timer = null;
    input.addEventListener('input', () => {
      clearTimeout(timer);
      timer = setTimeout(() => {
        state.search = input.value;
        render();
      }, 80);
    });
  }

  /* ---------- Empty-state action binding ---------- */

  function bindTableActions() {
    const wrapper = $('#tableWrapper');
    if (!wrapper) return;
    wrapper.addEventListener('click', (e) => {
      const btn = e.target.closest('[data-action]');
      if (!btn) return;
      const action = btn.getAttribute('data-action');
      if (action === 'retry') {
        loadProblems();
      } else if (action === 'reset-filter') {
        state.difficulty = 'all';
        state.search = '';
        const input = $('#searchInput');
        if (input) input.value = '';
        $$('.filter__chip').forEach((c) => {
          const isAll = c.getAttribute('data-difficulty') === 'all';
          c.classList.toggle('is-active', isAll);
          c.setAttribute('aria-selected', isAll ? 'true' : 'false');
        });
        render();
      }
    });
  }

  /* ---------- User menu ---------- */

  function getStoredUsername() {
    try { return sessionStorage.getItem('oj_username') || ''; }
    catch (_) { return ''; }
  }

  function setStoredUsername(name) {
    try { sessionStorage.setItem('oj_username', name || ''); }
    catch (_) {}
  }

  function clearStoredUsername() {
    try { sessionStorage.removeItem('oj_username'); }
    catch (_) {}
  }

  function getStoredRole() {
    try { return sessionStorage.getItem('oj_role') || null; }
    catch (_) { return null; }
  }

  function setStoredRole(role) {
    try { if (role) sessionStorage.setItem('oj_role', role); }
    catch (_) {}
  }

  function clearStoredRole() {
    try { sessionStorage.removeItem('oj_role'); }
    catch (_) {}
  }

  function renderUserMenu() {
    const menu = $('#userMenu');
    if (!menu) return;
    const name = getStoredUsername();

    if (name) {
      const isAdmin = state.role === 'admin';
      // Small shield icon prefix to the username — marks admin accounts.
      const mark = isAdmin
        ? '<svg class="admin-mark" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" title="管理员"><path d="M20 13c0 5-3.5 7.5-7.66 8.95a1 1 0 0 1-.67-.01C7.5 20.5 4 18 4 13V6a1 1 0 0 1 1-1c2 0 4.5-1.2 6.24-2.72a1.17 1.17 0 0 1 1.52 0C14.51 3.81 17 5 19 5a1 1 0 0 1 1 1z"/></svg>'
        : '';
      // Add a quick "进入管理后台" link in the user menu for admins —
      // a secondary entry point in addition to the nav link.
      const adminLink = isAdmin
        ? '<a href="/admin.html" class="user-menu__link">管理后台</a>'
        : '';
      menu.innerHTML = `
        <span class="user-menu__greeting">${mark}你好，<span class="user-menu__name">${escapeHtml(name)}</span></span>
        ${adminLink}
        <button class="user-menu__logout" type="button" data-action="logout">退出</button>`;
      menu.querySelector('[data-action="logout"]').addEventListener('click', handleLogout);
    } else {
      const ret = encodeURIComponent(global.location.pathname + global.location.search);
      menu.innerHTML = `
        <a href="/login.html?return=${ret}" class="user-menu__link">登录</a>
        <a href="/register.html" class="user-menu__link user-menu__link--primary">注册</a>`;
    }
  }

  /* Verify auth state with the server (handles new-tab case where
     sessionStorage is empty but the session cookie is still valid). */
  function verifyAuth() {
    if (typeof Api === 'undefined' || !Api.me) return;
    Api.me().then((res) => {
      if (res.ok && res.data && res.data.username) {
        const serverName = String(res.data.username);
        const serverRole = String(res.data.role || 'user');
        const current = getStoredUsername();
        if (current !== serverName) {
          setStoredUsername(serverName);
          setStoredRole(serverRole);
          state.role = serverRole;
          renderUserMenu();
        } else if (getStoredRole() !== serverRole) {
          setStoredRole(serverRole);
          state.role = serverRole;
          renderUserMenu();
        }
      } else if (getStoredUsername()) {
        clearStoredUsername();
        clearStoredRole();
        state.role = null;
        renderUserMenu();
      }
    });
  }

  /* ---------- Logout ---------- */

  function showToast(message, duration) {
    const toast = $('#toast');
    if (!toast) return;
    toast.textContent = message;
    toast.classList.add('is-visible');
    clearTimeout(showToast._t);
    showToast._t = setTimeout(() => {
      toast.classList.remove('is-visible');
    }, duration || 1800);
  }

  function handleLogout() {
    Api.logout().finally(() => {
      clearStoredUsername();
      clearStoredRole();
      state.role = null;
      showToast('已退出登录');
      // Re-render menu in logged-out state, then redirect
      renderUserMenu();
      setTimeout(() => { global.location.href = '/login.html'; }, 700);
    });
  }

  /* ---------- Init ---------- */

  function init() {
    state.role = getStoredRole();
    renderUserMenu();
    bindFilter();
    bindSearch();
    bindTableActions();
    loadProblems();
    verifyAuth();
  }

  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', init);
  } else {
    init();
  }

  /* Expose for debugging / future use */
  global.ProblemList = { state, render, loadProblems };
})(window);

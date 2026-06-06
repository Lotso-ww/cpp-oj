/* ==========================================================================
   Admin — load, render, add, delete.
   Requires an admin session. Non-admins get bounced to /login.html.
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
    username:  null,
    role:      null,        // 'admin' | 'user' | null
    authKnown: false,
    problems:  [],
    loading:   false,
    error:     null,
    deleting:  null         // { id, title } when confirm modal is open
  };

  /* ---------- SVG fragments ---------- */

  const SVG = {
    trash: '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" width="16" height="16" stroke-width="2"><polyline points="3 6 5 6 21 6"/><path d="M19 6 18 20a2 2 0 0 1-2 2H8a2 2 0 0 1-2-2L5 6"/><path d="M10 11v6"/><path d="M14 11v6"/><path d="M9 6V4a2 2 0 0 1 2-2h2a2 2 0 0 1 2 2v2"/></svg>',
    arrowRight: '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" width="14" height="14" stroke-width="2"><path d="M5 12h14"/><path d="m12 5 7 7-7 7"/></svg>'
  };

  /* ---------- Difficulty bar (3-bar, level 1-3) ---------- */

  function difficultyBars(level) {
    let bars = '';
    for (let i = 0; i < 3; i++) {
      const active = i < level;
      const x = 1 + i * 7;
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

  /* ---------- Toast ---------- */

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

  /* ---------- User menu (top right) ---------- */

  function renderUserMenu() {
    const menu = $('#userMenu');
    if (!menu) return;
    const isAdmin = state.role === 'admin';
    // No "进入管理后台" link here — the user is already ON the admin page,
    // and the nav already shows "管理" as the active item. Adding another
    // link to the user menu would be redundant and confusing.
    const mark = isAdmin
      ? '<svg class="admin-mark" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" title="管理员"><path d="M20 13c0 5-3.5 7.5-7.66 8.95a1 1 0 0 1-.67-.01C7.5 20.5 4 18 4 13V6a1 1 0 0 1 1-1c2 0 4.5-1.2 6.24-2.72a1.17 1.17 0 0 1 1.52 0C14.51 3.81 17 5 19 5a1 1 0 0 1 1 1z"/></svg>'
      : '';
    if (state.username) {
      menu.innerHTML = `
        <span class="user-menu__greeting">${mark}你好，<span class="user-menu__name">${escapeHtml(state.username)}</span></span>
        <button class="user-menu__logout" type="button" data-action="logout">退出</button>`;
      menu.querySelector('[data-action="logout"]').addEventListener('click', handleLogout);
    } else {
      const ret = encodeURIComponent(global.location.pathname + global.location.search);
      menu.innerHTML = `
        <a href="/login.html?return=${ret}" class="user-menu__link">登录</a>
        <a href="/register.html" class="user-menu__link user-menu__link--primary">注册</a>`;
    }
  }

  function handleLogout() {
    Api.logout().finally(() => {
      try { sessionStorage.removeItem('oj_username'); sessionStorage.removeItem('oj_role'); } catch (_) {}
      state.username = null;
      state.role = null;
      showToast('已退出登录');
      renderUserMenu();
      setTimeout(() => { global.location.href = '/login.html'; }, 700);
    });
  }

  /* ---------- Auth gate ---------- */

  function verifyAuth() {
    if (typeof Api === 'undefined' || !Api.me) return Promise.resolve();
    return Api.me().then((res) => {
      if (res.ok && res.data && res.data.username) {
        state.username  = String(res.data.username);
        state.role      = String(res.data.role || 'user');
        state.authKnown = true;
        try {
          sessionStorage.setItem('oj_username', state.username);
          sessionStorage.setItem('oj_role',     state.role);
        } catch (_) {}
        // Bounce non-admins with a friendly toast so they understand why
        // they were redirected (not just silently dropped on problem list)
        if (state.role !== 'admin') {
          showToast('需要管理员权限', 2200);
          setTimeout(() => { global.location.replace('/problem_list.html'); }, 800);
          return;
        }
        renderUserMenu();
      } else {
        // Not logged in → go to login
        const ret = encodeURIComponent(global.location.pathname + global.location.search);
        global.location.replace('/login.html?return=' + ret);
      }
    });
  }

  /* ---------- Skeleton / Empty / Error ---------- */

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
            <th>题号</th><th>标题</th><th>难度</th><th></th>
          </tr>
        </thead>
        <tbody>${skeletonRowsHtml(6)}</tbody>
      </table>`;
  }

  function emptyStateHtml(title, description, actionLabel, actionId) {
    return `
      <div class="empty-state" role="status">
        <h2 class="empty-state__title">${escapeHtml(title)}</h2>
        <p class="empty-state__description">${escapeHtml(description)}</p>
        ${actionLabel ? `<button class="empty-state__action" type="button" data-action="${actionId}">${escapeHtml(actionLabel)}</button>` : ''}
      </div>`;
  }

  /* ---------- Row rendering (with delete button) ---------- */

  function rowHtml(problem) {
    const href = `/problem.html?id=${encodeURIComponent(problem.id)}`;
    return `
      <tr class="problem-table__row" data-id="${escapeHtml(problem.id)}">
        <td class="problem-table__cell problem-table__cell--id">${escapeHtml(problem.id)}</td>
        <td class="problem-table__cell problem-table__cell--title">
          <a href="${href}">${escapeHtml(problem.title)}</a>
        </td>
        <td class="problem-table__cell problem-table__cell--difficulty">
          ${difficultyTag(problem.difficulty)}
        </td>
        <td class="problem-table__cell problem-table__cell--action problem-table__cell--admin-action">
          <a class="problem-table__link-icon" href="${href}" aria-label="查看题目 ${escapeHtml(problem.title)}" title="查看">
            ${SVG.arrowRight}
          </a>
          <button class="problem-table__delete" type="button" data-action="delete" data-id="${escapeHtml(problem.id)}" data-title="${escapeHtml(problem.title)}" aria-label="删除题目 ${escapeHtml(problem.title)}" title="删除">
            ${SVG.trash}
          </button>
        </td>
      </tr>`;
  }

  function tableHtml(problems) {
    return `
      <table class="problem-table">
        <thead>
          <tr>
            <th>题号</th><th>标题</th><th>难度</th><th class="problem-table__th--action">操作</th>
          </tr>
        </thead>
        <tbody>${problems.map(rowHtml).join('')}</tbody>
      </table>`;
  }

  /* ---------- State → render ---------- */

  function render() {
    const wrapper = $('#tableWrapper');
    const counter = $('#problemCount');
    if (!wrapper) return;

    if (state.loading) {
      wrapper.innerHTML = skeletonHtml();
      wrapper.setAttribute('aria-busy', 'true');
      if (counter) counter.textContent = '加载中…';
      return;
    }
    if (state.error) {
      wrapper.innerHTML = emptyStateHtml('无法加载题目', '请检查网络后重试。', '重试', 'retry');
      wrapper.setAttribute('aria-busy', 'false');
      if (counter) counter.textContent = '— 题';
      return;
    }
    if (state.problems.length === 0) {
      wrapper.innerHTML = emptyStateHtml('暂无题目', '还没有任何题目。在上方表单中添加一道吧。', null, null);
    } else {
      wrapper.innerHTML = tableHtml(state.problems);
    }
    wrapper.setAttribute('aria-busy', 'false');
    if (counter) counter.textContent = `${state.problems.length} 题`;
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
      list.sort((a, b) => a.id - b.id);
      state.problems = list;
      render();
    });
  }

  /* ---------- Delete: confirm modal ---------- */

  function openDeleteModal(id, title) {
    state.deleting = { id, title };
    const modal = $('#deleteModal');
    const body  = $('#deleteModalBody');
    if (!modal || !body) return;
    body.textContent = `将永久删除题目 #${id}「${title}」及其所有测试用例。此操作不可恢复。`;
    modal.hidden = false;
    // Focus the cancel button by default (safer default for destructive action)
    const cancel = $('#cancelDeleteBtn');
    if (cancel) cancel.focus();
  }

  function closeDeleteModal() {
    state.deleting = null;
    const modal = $('#deleteModal');
    if (modal) modal.hidden = true;
  }

  function bindModal() {
    const modal = $('#deleteModal');
    if (!modal) return;
    modal.addEventListener('click', (e) => {
      if (e.target.closest('[data-close="1"]')) closeDeleteModal();
    });
    document.addEventListener('keydown', (e) => {
      if (e.key === 'Escape' && !modal.hidden) closeDeleteModal();
    });
    const confirmBtn = $('#confirmDeleteBtn');
    if (confirmBtn) {
      confirmBtn.addEventListener('click', () => {
        if (!state.deleting) return;
        const { id, title } = state.deleting;
        closeDeleteModal();
        performDelete(id, title);
      });
    }
  }

  function performDelete(id, title) {
    showToast('正在删除…', 1000);
    Api.deleteProblem(id).then((res) => {
      if (!res.ok) {
        const err = (res.data && res.data.error) || `删除失败 (${res.status})`;
        showToast(err, 2400);
        return;
      }
      // Optimistic local removal so the list re-renders instantly
      state.problems = state.problems.filter(p => String(p.id) !== String(id));
      render();
      showToast(`已删除「${title}」`, 1800);
    });
  }

  /* ---------- Table click delegation (delete button) ---------- */

  function bindTableActions() {
    const wrapper = $('#tableWrapper');
    if (!wrapper) return;
    wrapper.addEventListener('click', (e) => {
      // Don't open modal when clicking the row's view link
      if (e.target.closest('a.problem-table__link-icon')) return;

      // Use a more specific selector and explicit prevent/stop so the
      // click always reaches the delete handler, even if the click target
      // is the SVG icon inside the button.
      const btn = e.target.closest('button[data-action="delete"]');
      if (btn) {
        e.preventDefault();
        e.stopPropagation();
        const id    = btn.dataset.id;
        const title = btn.dataset.title;
        if (id) openDeleteModal(id, title);
        return;
      }
      const retry = e.target.closest('[data-action="retry"]');
      if (retry) loadProblems();
    });
  }

  /* ---------- New-problem form ---------- */

  // Index counter for dynamic test-case rows
  let nextCaseIndex = 2;

  function renderCase(index) {
    return `
      <div class="form__test-case" data-index="${index}">
        <div class="form__test-case-head">
          <span>用例 #${index + 1}</span>
          <button class="form__test-case-remove" type="button" data-remove="${index}" aria-label="删除此用例">删除</button>
        </div>
        <div class="form__test-case-fields">
          <textarea class="form__textarea form__textarea--mini" name="input-${index}" rows="2" placeholder="stdin 输入"></textarea>
          <textarea class="form__textarea form__textarea--mini" name="expected-${index}" rows="2" placeholder="stdout 预期"></textarea>
        </div>
      </div>`;
  }

  function bindNewProblemForm() {
    const addBtn = $('#addCaseBtn');
    if (addBtn) {
      addBtn.addEventListener('click', () => {
        const container = $('#newProblemCases');
        if (!container) return;
        container.insertAdjacentHTML('beforeend', renderCase(nextCaseIndex));
        nextCaseIndex++;
      });
    }

    const casesContainer = $('#newProblemCases');
    if (casesContainer) {
      casesContainer.addEventListener('click', (e) => {
        const btn = e.target.closest('[data-remove]');
        if (!btn) return;
        const row = btn.closest('.form__test-case');
        if (!row) return;
        // Always keep at least one row
        if (casesContainer.querySelectorAll('.form__test-case').length <= 1) return;
        row.remove();
        // Renumber
        casesContainer.querySelectorAll('.form__test-case').forEach((r, i) => {
          r.querySelector('.form__test-case-head span').textContent = `用例 #${i + 1}`;
        });
      });
    }

    const form = $('#newProblemForm');
    if (!form) return;
    form.addEventListener('submit', (e) => {
      e.preventDefault();
      submitNewProblem(form);
    });
  }

  function collectTestCases() {
    const rows = $$('.form__test-case');
    const cases = [];
    rows.forEach((row) => {
      const idx = row.dataset.index;
      const input    = (row.querySelector(`textarea[name="input-${idx}"]`)    || {}).value || '';
      const expected = (row.querySelector(`textarea[name="expected-${idx}"]`) || {}).value || '';
      // Skip rows where both fields are empty (admin likely left an extra row)
      if (input.trim() === '' && expected.trim() === '') return;
      cases.push({ input, expected });
    });
    return cases;
  }

  function submitNewProblem(form) {
    const fd = new FormData(form);
    const title       = String(fd.get('title') || '').trim();
    const difficulty  = String(fd.get('difficulty') || '').trim();
    const content     = String(fd.get('content')  || '').trim();
    const template    = String(fd.get('template') || '');
    const testCases   = collectTestCases();

    if (!title)    { showToast('请填写标题', 1800); return; }
    if (!content)  { showToast('请填写题目描述', 1800); return; }
    if (!['Easy', 'Medium', 'Hard'].includes(difficulty)) { showToast('请选择难度', 1800); return; }

    const submitBtn = $('#submitNewBtn');
    if (submitBtn) {
      submitBtn.disabled = true;
      submitBtn.classList.add('is-loading');
    }

    Api.createProblem({ title, difficulty, content, template, testCases }).then((res) => {
      if (submitBtn) {
        submitBtn.disabled = false;
        submitBtn.classList.remove('is-loading');
      }

      if (res.status === 401) { global.location.replace('/login.html'); return; }
      if (res.status === 403) {
        showToast('需要管理员权限', 2200);
        return;
      }
      if (!res.ok) {
        const err = (res.data && res.data.error) || `错误 ${res.status}`;
        showToast(err, 2400);
        return;
      }

      // Success
      form.reset();
      // Rebuild the default 2 test-case rows (form.reset() doesn't reset dynamically-added rows)
      rebuildDefaultCases();
      showToast('题目已添加', 1800);
      loadProblems();
    });
  }

  function rebuildDefaultCases() {
    const container = $('#newProblemCases');
    if (!container) return;
    container.innerHTML = renderCase(0) + renderCase(1);
    nextCaseIndex = 2;
  }

  /* ---------- Init ---------- */

  function init() {
    // Empty initial render so the topbar shows immediately
    renderUserMenu();

    bindNewProblemForm();
    bindTableActions();
    bindModal();

    verifyAuth().then(() => {
      if (state.role === 'admin') {
        loadProblems();
      }
      // non-admins are already redirected; nothing to do
    });
  }

  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', init);
  } else {
    init();
  }

  global.AdminPage = { state, render, loadProblems };
})(window);

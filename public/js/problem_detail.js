/* ==========================================================================
   Problem detail — load problem, render markdown, init ace editor,
                   wire submit, render result card, auth gating.
   Pure DOM, no frameworks. Loaded by problem.html.
   ========================================================================== */

(function (global) {
  'use strict';

  /* ---------- DOM utilities ---------- */

  const $ = (sel, root = document) => root.querySelector(sel);
  const $$ = (sel, root = document) => Array.from(root.querySelectorAll(sel));

  function escapeHtml(s) {
    return String(s == null ? '' : s)
      .replace(/&/g, '&amp;')
      .replace(/</g, '&lt;')
      .replace(/>/g, '&gt;')
      .replace(/"/g, '&quot;')
      .replace(/'/g, '&#39;');
  }

  /* ---------- State ---------- */

  const state = {
    problemId: null,
    problem:   null,
    username:  null,
    isAuthed:  false,
    authKnown: false,   // becomes true after /api/me (or logout) responds
    role:      null,    // 'admin' | 'user' | null — drives the badge + admin link
    editor:    null,
    initialCode: '',
    submitting: false,
    // Read-only test cases loaded from the problem's DB. These are the
    // authoritative cases — "提交" judges against all of them. The
    // "运行测试" feature compares the user's code against them too, so
    // the user can see exactly which official cases are passing.
    problemTestCases: [],
    // User-added test cases (LeetCode-style). These have input only —
    // there's no "expected output" to write, because the user just wants
    // to inspect the actual output for some input they care about. They
    // are appended to the problem's test cases when running.
    customTestCases: []
  };

  /* ---------- Defaults ---------- */

  const DEFAULT_CPP =
    '#include <iostream>\n' +
    'using namespace std;\n' +
    '\n' +
    'int main() {\n' +
    '    // 在这里编写你的 C++ 代码\n' +
    '    \n' +
    '    return 0;\n' +
    '}\n';

  const DIFFICULTY_LABEL = { Easy: '简单', Medium: '中等', Hard: '困难' };
  const DIFFICULTY_LEVEL = { Easy: 1, Medium: 2, Hard: 3 };

  const STATUS_META = {
    'AC':           { label: '通过',       sub: '全部用例通过' },
    'WA':           { label: '答案错误',   sub: '输出与预期不符' },
    'TLE':          { label: '运行超时',   sub: '超出时间限制' },
    'RE':           { label: '运行错误',   sub: '程序异常退出' },
    'MLE':          { label: '内存超限',   sub: '超出内存限制' },
    'CE':           { label: '编译错误',   sub: '代码无法编译' },
    'SYSTEM_ERROR': { label: '系统错误',   sub: '服务内部异常' }
  };

  /* ---------- Difficulty tag (shared with list) ---------- */

  function difficultyBars(level) {
    let bars = '';
    for (let i = 0; i < 3; i++) {
      const active = i < level;
      const x = 1 + i * 7;
      bars += `<rect class="difficulty__bar${active ? ' is-active' : ''}" x="${x}" y="1" width="3" height="9" rx="0.5"/>`;
    }
    return `<svg class="difficulty__bars" width="22" height="11" viewBox="0 0 22 11" aria-hidden="true">${bars}</svg>`;
  }

  function difficultyTag(difficulty) {
    const lvl = DIFFICULTY_LEVEL[difficulty] || 1;
    const label = DIFFICULTY_LABEL[difficulty] || difficulty;
    return `<span class="difficulty" data-difficulty="${escapeHtml(difficulty)}">${difficultyBars(lvl)}<span class="difficulty__label">${escapeHtml(label)}</span></span>`;
  }

  /* ---------- Ace theme: monochrome, ink-scale syntax ---------- */

  function defineAceTheme() {
    if (typeof ace === 'undefined') return;
    ace.define('ace/theme/cpp-oj', ['require', 'exports', 'module'], function (require, exports, module) {
      exports.isDark = false;
      exports.cssClass = 'ace-cpp-oj';
      exports.cssText = [
        '.ace-cpp-oj .ace_gutter {',
        '  background: #FAFAF7;',
        '  color: #9A9A9A;',
        '}',
        '.ace-cpp-oj .ace_print-margin {',
        '  background: #E8E6E1;',
        '  width: 1px;',
        '}',
        '.ace-cpp-oj {',
        '  background-color: #FFFFFF;',
        '  color: #0A0A0A;',
        '  /* JetBrains Mono for Latin (1em/char, clean monospace).',
        '     For CJK we fall back to the OS system font (PingFang SC on macOS,',
        '     Microsoft YaHei on Windows, Noto Sans CJK SC on Linux). These are',
        '     all designed for body text with CJK at exactly 1em full-width,',
        '     which matches JetBrains Mono Latin 1em — so the cell width is',
        '     consistent and the cursor lands at the right boundary. */',
        '  font-family: "JetBrains Mono", -apple-system, BlinkMacSystemFont, "Segoe UI", "PingFang SC", "Hiragino Sans GB", "Microsoft YaHei", "Noto Sans CJK SC", "WenQuanYi Micro Hei", sans-serif;',
        '  line-height: 1.7;',
        '}',
        '.ace-cpp-oj .ace_scroller {',
        '  font-family: "JetBrains Mono", -apple-system, BlinkMacSystemFont, "Segoe UI", "PingFang SC", "Hiragino Sans GB", "Microsoft YaHei", "Noto Sans CJK SC", "WenQuanYi Micro Hei", sans-serif;',
        '  font-size: 14px;',
        '}',
        '.ace-cpp-oj .ace_marker-layer .ace_selection {',
        '  background: rgba(10, 10, 10, 0.08);',
        '}',
        '.ace-cpp-oj.ace_multiselect .ace_selection.ace_start {',
        '  box-shadow: 0 0 3px 0 rgba(10, 10, 10, 0.12);',
        '}',
        '.ace-cpp-oj .ace_marker-layer .ace_bracket {',
        '  margin: -1px 0 0 -1px;',
        '  border: 1px solid rgba(10, 10, 10, 0.15);',
        '}',
        '.ace-cpp-oj .ace_keyword,',
        '.ace-cpp-oj .ace_storage,',
        '.ace-cpp-oj .ace_storage.ace_type {',
        '  color: #0A0A0A;',
        '  font-weight: 600;',
        '}',
        '.ace-cpp-oj .ace_type {',
        '  color: #0A0A0A;',
        '  font-weight: 500;',
        '}',
        '.ace-cpp-oj .ace_function,',
        '.ace-cpp-oj .ace_variable.ace_function {',
        '  color: #0A0A0A;',
        '  font-style: italic;',
        '}',
        '.ace-cpp-oj .ace_string,',
        '.ace-cpp-oj .ace_string.ace_regexp,',
        '.ace-cpp-oj .ace_constant.ace_character {',
        '  /* warm desaturated brown — clearly distinct from ink scale, but muted */',
        '  color: #7A5C3A;',
        '}',
        '.ace-cpp-oj .ace_constant.ace_numeric,',
        '.ace-cpp-oj .ace_constant.ace_other {',
        '  /* cool desaturated slate — counterpoint to the warm string color */',
        '  color: #4F6478;',
        '}',
        '.ace-cpp-oj .ace_comment,',
        '.ace-cpp-oj .ace_comment.ace_doc,',
        '.ace-cpp-oj .ace_comment.ace_doc-comment {',
        '  color: #9A9A9A;',
        '}',
        '.ace-cpp-oj .ace_variable,',
        '.ace-cpp-oj .ace_variable.ace_language,',
        '.ace-cpp-oj .ace_variable.ace_parameter {',
        '  color: #0A0A0A;',
        '}',
        '.ace-cpp-oj .ace_meta,',
        '.ace-cpp-oj .ace_meta.ace_tag,',
        '.ace-cpp-oj .ace_meta.ace_import,',
        '.ace-cpp-oj .ace_meta.ace_preprocessor {',
        '  color: #1F1F1F;',
        '  font-weight: 500;',
        '}',
        '.ace-cpp-oj .ace_indent-guide {',
        '  background: transparent;',
        '  border-right: 1px solid #E8E6E1;',
        '  right: 0;',
        '}',
        '.ace-cpp-oj .ace_fold-widget {',
        '  color: #9A9A9A;',
        '  background-color: #FAFAF7;',
        '}',
        '.ace-cpp-oj .ace_active-line {',
        '  background: rgba(10, 10, 10, 0.025);',
        '}',
        '.ace-cpp-oj .ace_gutter-active-line {',
        '  background: transparent;',
        '  color: #0A0A0A;',
        '}'
      ].join('\n');
    });
  }

  /* ---------- Editor ---------- */

  // Wait for all document fonts to be ready before initializing ace.
  // Without this, ace measures char widths with whatever font is currently
  // loaded (often a system fallback), and when the real font arrives later,
  // the cursor positions don't match the rendered character → cursor drift.
  async function waitForFonts() {
    if (!document.fonts || !document.fonts.ready) return;
    try {
      // Explicitly request the fonts we care about. This both ensures they're
      // loaded and (in modern browsers) the CSS @font-face is processed.
      await Promise.all([
        document.fonts.load("14px 'JetBrains Mono'"),
        document.fonts.load("14px 'IBM Plex Sans SC'"),
        document.fonts.ready
      ]);
    } catch (_) {
      // If anything fails, just proceed — the editor will use whatever loaded
    }
  }

  // Recompute ace's cached character-size and re-render. Call this after
  // the editor is created and after the document value is set, so the
  // cursor sits at the right boundary of the cell (after the new char)
  // rather than the left boundary of the previous cell.
  function remeasureEditor(editor) {
    if (!editor) return;
    try {
      if (editor.renderer.$loop && editor.renderer.$loop._updateCharacterSize) {
        editor.renderer.$loop._updateCharacterSize();
      }
      editor.resize();
      if (editor.renderer.updateFull) {
        editor.renderer.updateFull();
      }
    } catch (_) {
      // best-effort; ignore failures
    }
  }

  // Find the "// 在这里编写你的 C++ 代码" line in the default template and
  // select it (so typing replaces it). Skips leading spaces so indentation
  // is preserved.
  function placeCursorOnComment(editorInstance, code) {
    const lines = code.split('\n');
    let commentRow = -1;
    for (let i = 0; i < lines.length; i++) {
      if (lines[i].indexOf('//') !== -1) {
        commentRow = i;
        break;
      }
    }
    if (commentRow === -1) {
      if (typeof editorInstance.navigateFileEnd === 'function') {
        editorInstance.navigateFileEnd();
      } else if (typeof editorInstance.gotoLine === 'function') {
        const last = editorInstance.session.getLength();
        editorInstance.gotoLine(last, last, false);
      }
      return;
    }
    const line = lines[commentRow];
    // Skip leading spaces
    let startCol = 0;
    while (startCol < line.length && line[startCol] === ' ') startCol++;
    const range = new ace.Range(commentRow, startCol, commentRow, line.length);
    editorInstance.selection.setRange(range, false);
    // Make sure the cursor (and selection) is visible
    editorInstance.renderer.scrollCursorIntoView();
  }

  async function initEditor() {
    const editorEl = $('#editor');
    if (!editorEl) return;

    if (typeof ace === 'undefined') {
      // Fallback: plain textarea (ace failed to load)
      editorEl.innerHTML = '<textarea class="editor__fallback" placeholder="在这里编写你的 C++ 代码…" style="width:100%;height:100%;border:0;outline:0;padding:14px 16px;font:13px/1.6 JetBrains Mono,monospace;resize:none;background:transparent;color:#0A0A0A;"></textarea>';
      const ta = editorEl.querySelector('textarea');
      state.editor = {
        getValue:   () => ta.value,
        setValue:   (v) => { ta.value = v; },
        setReadOnly: (b) => { ta.readOnly = !!b; },
        focus:      () => ta.focus(),
        clearSelection: () => { ta.setSelectionRange(0, 0); }
      };
      state.initialCode = DEFAULT_CPP;
      ta.value = state.initialCode;
      return;
    }

    // CRITICAL: wait for fonts BEFORE defining the theme + creating the editor.
    // If we don't, ace measures char widths with whatever font is loaded at
    // that moment (usually a system fallback), and when the real font arrives
    // later, the cursor positions don't match the rendered character → drift.
    await waitForFonts();

    defineAceTheme();

    const editor = ace.edit(editorEl, {
      mode:             'ace/mode/c_cpp',
      theme:            'ace/theme/cpp-oj',
      tabSize:          4,
      useSoftTabs:      true,
      showPrintMargin:  false,
      showLineNumbers:  true,
      showGutter:       true,
      highlightActiveLine: true,
      wrap:             false,
      cursorStyle:      'round',
      readOnly:         false
    });

    editor.renderer.setShowGutter(true);

    // Set the value BEFORE setting font/size options. When fontFamily or
    // fontSize changes, Ace re-measures character widths. By setting the
    // value first, the subsequent re-measurement aligns the cursor to
    // the actual rendered character cells (this is the order the working
    // round-cursor setup uses).
    const codeKey = 'oj_editor_code_' + (state.problemId || 'unknown');
    const saved = (() => { try { return sessionStorage.getItem(codeKey); } catch (_) { return null; } })();
    const isNewCode = !saved;
    editor.session.setValue(saved || DEFAULT_CPP);

    // Now apply font + size via setOptions. Ace reads fontFamily from
    // getOption() (not from theme CSS) when measuring char widths, so
    // setting it here is what makes the cursor land on the correct
    // cell boundary instead of one cell to the left.
    editor.setOptions({
      fontSize:       '14px',
      fontFamily:     '"JetBrains Mono", monospace',
      enableBasicAutocompletion:   false,
      enableLiveAutocompletion:    false,
      enableSnippets:              false,
      animatedScroll:              true,
      scrollPastEnd:               0.1,
      roundedselection:            true
    });

    // Track focus for the border highlight
    editor.on('focus', () => editorEl.classList.add('is-focused'));
    editor.on('blur',  () => editorEl.classList.remove('is-focused'));

    state.editor = editor;
    state.initialCode = DEFAULT_CPP;

    // Re-measure after font/size is applied so the cursor sits on the
    // correct cell boundary (after the new character), not one cell to
    // the left.
    remeasureEditor(editor);

    if (isNewCode) {
      placeCursorOnComment(editor, state.initialCode);
    } else {
      if (typeof editor.navigateFileEnd === 'function') {
        editor.navigateFileEnd();
      } else if (typeof editor.gotoLine === 'function') {
        const last = editor.session.getLength();
        editor.gotoLine(last, last, false);
      }
    }

    // Auto-save on every change
    state.saveTimer = null;
    state.cancelAutoSave = () => {
      if (state.saveTimer) {
        clearTimeout(state.saveTimer);
        state.saveTimer = null;
      }
    };
    editor.session.on('change', () => {
      state.cancelAutoSave();
      state.saveTimer = setTimeout(() => {
        try { sessionStorage.setItem(codeKey, editor.getValue()); } catch (_) {}
        state.saveTimer = null;
      }, 200);
    });

    // Resize handling — ace needs explicit resize when container changes
    let resizeTimer = null;
    global.addEventListener('resize', () => {
      clearTimeout(resizeTimer);
      resizeTimer = setTimeout(() => editor.resize(), 100);
    });
  }


  /* ---------- Auth state & UI ---------- */

  function getStoredUsername() {
    try { return sessionStorage.getItem('oj_username') || ''; }
    catch (_) { return ''; }
  }

  function getStoredRole() {
    try { return sessionStorage.getItem('oj_role') || null; }
    catch (_) { return null; }
  }

  function renderUserMenu() {
    const menu = $('#userMenu');
    if (!menu) return;

    if (state.isAuthed) {
      const isAdmin = state.role === 'admin';
      const mark = isAdmin
        ? '<svg class="admin-mark" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" title="管理员"><path d="M20 13c0 5-3.5 7.5-7.66 8.95a1 1 0 0 1-.67-.01C7.5 20.5 4 18 4 13V6a1 1 0 0 1 1-1c2 0 4.5-1.2 6.24-2.72a1.17 1.17 0 0 1 1.52 0C14.51 3.81 17 5 19 5a1 1 0 0 1 1 1z"/></svg>'
        : '';
      const adminLink = isAdmin
        ? '<a href="/admin.html" class="user-menu__link">管理后台</a>'
        : '';
      menu.innerHTML = `
        <span class="user-menu__greeting">${mark}你好，<span class="user-menu__name">${escapeHtml(state.username)}</span></span>
        ${adminLink}
        <button class="user-menu__logout" type="button" data-action="logout">退出</button>`;
      const btn = menu.querySelector('[data-action="logout"]');
      if (btn) btn.addEventListener('click', handleLogout);
    } else {
      const ret = encodeURIComponent(global.location.pathname + global.location.search);
      menu.innerHTML = `
        <a href="/login.html?return=${ret}" class="user-menu__link">登录</a>
        <a href="/register.html" class="user-menu__link user-menu__link--primary">注册</a>`;
    }
  }

  function renderAuthHint() {
    const hint = $('#authHint');
    if (!hint) return;
    // Show the hint only when we KNOW the user is not authed
    // (avoids a brief flash for users who are already logged in but
    // happen to have empty sessionStorage in this tab)
    hint.hidden = state.isAuthed || !state.authKnown;
  }

  /* ---------- Test case editor (LeetCode-style "Run Code" inputs) ----------
   *
   * Layout:
   *   - "题目用例" (problemTestCases) — read-only display of input/expected.
   *     Loaded from the problem's DB. The user doesn't write these, the
   *     "运行测试" feature judges the code against them automatically.
   *   - "自定义用例" (customTestCases) — input only, the user can add
   *     as many as they want. "运行测试" runs them and shows the actual
   *     output for each (no comparison — there's no expected).
   *
   * The two groups have different row markup (.test-case-row vs.
   * .test-case-row--readonly) so the editor's event delegation can
   * distinguish them, and so the visual style signals which is editable.
   */

  // Render the test case editor. Two visually distinct sections: a
  // read-only display of the problem's official test cases, and an
  // editable list of the user's custom test cases.
  function renderTestCases() {
    const list = $('#testCaseList');
    if (!list) return;

    const dbCases    = state.problemTestCases;
    const customCases = state.customTestCases;

    // Empty state — no DB cases AND no custom cases
    if (dbCases.length === 0 && customCases.length === 0) {
      list.innerHTML = '<p class="test-case-editor__empty">还没有测试用例。点击「+ 添加用例」创建自定义用例来运行代码。</p>';
      return;
    }

    let html = '';

    // Section A: problem's official test cases (read-only)
    if (dbCases.length > 0) {
      html += '<div class="test-case-section" data-section="db">';
      html += '<h4 class="test-case-section__title">题目用例 <span class="test-case-section__count">' + dbCases.length + '</span></h4>';
      dbCases.forEach((c, i) => {
        html += '<div class="test-case-row test-case-row--readonly" data-source="db" data-index="' + i + '">';
        html += '<div class="test-case-row__head">';
        html += '<span class="test-case-row__num">用例 #' + (i + 1) + '</span>';
        html += '<span class="test-case-row__badge">官方</span>';
        html += '</div>';
        html += '<div class="test-case-row__fields">';
        html += '<div class="test-case-row__field">';
        html += '<span class="test-case-row__label">输入</span>';
        html += '<pre class="test-case-row__readonly">' + escapeHtml(c.input || '') + '</pre>';
        html += '</div>';
        html += '<div class="test-case-row__field">';
        html += '<span class="test-case-row__label">预期输出</span>';
        html += '<pre class="test-case-row__readonly">' + escapeHtml(c.expected || '') + '</pre>';
        html += '</div>';
        html += '</div>';
        html += '</div>';
      });
      html += '</div>';
    }

    // Section B: user-added custom test cases (editable, input only)
    if (customCases.length > 0) {
      html += '<div class="test-case-section" data-section="custom">';
      html += '<h4 class="test-case-section__title">自定义用例 <span class="test-case-section__count">' + customCases.length + '</span></h4>';
      customCases.forEach((c, i) => {
        html += '<div class="test-case-row" data-source="custom" data-index="' + i + '">';
        html += '<div class="test-case-row__head">';
        html += '<span class="test-case-row__num">用例 #' + (dbCases.length + i + 1) + '</span>';
        html += '<button class="test-case-row__remove" type="button" data-action="remove" aria-label="删除此用例">删除</button>';
        html += '</div>';
        html += '<div class="test-case-row__fields">';
        html += '<label class="test-case-row__field">';
        html += '<span class="test-case-row__label">输入</span>';
        html += '<textarea class="test-case-row__textarea" data-field="input" rows="2" placeholder="stdin 输入">' + escapeHtml(c.input || '') + '</textarea>';
        html += '</label>';
        html += '</div>';
        html += '</div>';
      });
      html += '</div>';
    }

    list.innerHTML = html;
  }

  // Wire up add / remove / edit (event delegation on the list). Only the
  // custom section is editable; the problem cases are read-only and the
  // remove button doesn't appear on those rows.
  function bindTestCaseEditor() {
    const addBtn = $('#addTestCaseBtn');
    if (addBtn) {
      addBtn.addEventListener('click', () => {
        state.customTestCases.push({ input: '' });
        renderTestCases();
        // Focus the new case's input field for fast entry
        const list = $('#testCaseList');
        if (list) {
          const customSection = list.querySelector('.test-case-section[data-section="custom"]');
          if (customSection) {
            const rows = customSection.querySelectorAll('.test-case-row');
            const last = rows[rows.length - 1];
            if (last) {
              const ta = last.querySelector('textarea[data-field="input"]');
              if (ta) ta.focus();
            }
          }
        }
      });
    }

    const list = $('#testCaseList');
    if (!list) return;

    // Edit (input event on textareas in the custom section only)
    list.addEventListener('input', (e) => {
      const ta = e.target.closest('textarea[data-field]');
      if (!ta) return;
      const row = ta.closest('.test-case-row');
      if (!row) return;
      // Defensive: the read-only rows don't contain textareas, but check
      // anyway in case the markup drifts.
      if (row.dataset.source !== 'custom') return;
      const index = parseInt(row.dataset.index, 10);
      const field = ta.dataset.field;
      if (state.customTestCases[index]) {
        state.customTestCases[index][field] = ta.value;
      }
    });

    // Remove (click event delegation). Only custom rows have a remove
    // button; problem cases are read-only.
    list.addEventListener('click', (e) => {
      const btn = e.target.closest('[data-action="remove"]');
      if (!btn) return;
      const row = btn.closest('.test-case-row');
      if (!row) return;
      if (row.dataset.source !== 'custom') return;
      const index = parseInt(row.dataset.index, 10);
      state.customTestCases.splice(index, 1);
      renderTestCases();
    });
  }

  function handleLogout() {
    Api.logout().finally(() => {
      try { sessionStorage.removeItem('oj_username'); } catch (_) {}
      try { sessionStorage.removeItem('oj_role'); } catch (_) {}
      state.username = null;
      state.isAuthed = false;
      state.role = null;
      state.authKnown = true;  // server confirmed via the logout 200
      applyAuthState();
      clearResult();
      showToast('已退出登录');
    });
  }

  /* Apply current isAuthed/username to all UI surfaces that depend on it.
     Called whenever the auth state changes. */
  function applyAuthState() {
    renderUserMenu();
    renderAuthHint();
  }

  /* Verify auth state with the server. This is the source of truth —
     sessionStorage is only a fast-path cache. If /api/me says the user
     is logged in (valid session cookie), we trust that even when
     sessionStorage is empty (e.g. opened the page in a new tab). */
  function verifyAuthWithServer() {
    Api.me().then((res) => {
      state.authKnown = true;
      if (res.ok && res.data && res.data.username) {
        const serverName = String(res.data.username);
        const serverRole = String(res.data.role || 'user');
        if (!state.isAuthed || state.username !== serverName) {
          state.username = serverName;
          state.isAuthed = true;
          try { sessionStorage.setItem('oj_username', serverName); } catch (_) {}
        }
        if (state.role !== serverRole) {
          state.role = serverRole;
          try { sessionStorage.setItem('oj_role', serverRole); } catch (_) {}
        }
      } else {
        // Server says not authed — clear any stale local state
        if (state.isAuthed || getStoredUsername()) {
          state.username = null;
          state.isAuthed = false;
          state.role = null;
          try { sessionStorage.removeItem('oj_username'); } catch (_) {}
          try { sessionStorage.removeItem('oj_role'); } catch (_) {}
        }
      }
      applyAuthState();
    });
  }

  /* ---------- Toast ---------- */

  function showToast(message, duration) {
    const toast = $('#toast');
    if (!toast) return;
    toast.textContent = message;
    toast.classList.add('is-visible');
    clearTimeout(showToast._t);
    showToast._t = setTimeout(() => toast.classList.remove('is-visible'), duration || 1800);
  }

  /* ---------- Page header & content ---------- */

  function renderPageHeader() {
    if (!state.problem) return;
    $('#problemEyebrow').textContent = `题库 · 题目 #${state.problem.id}`;
    $('#problemTitle').textContent = state.problem.title || `题目 #${state.problem.id}`;
    const metaEl = $('#problemMeta');
    if (metaEl && state.problem.difficulty) {
      metaEl.innerHTML = difficultyTag(state.problem.difficulty);
    }
    document.title = `题目 #${state.problem.id} · ${state.problem.title} · CPP·OJ`;
  }

  function renderContent() {
    const container = $('#problemContent');
    if (!container) return;
    const raw = (state.problem && state.problem.content) || '';

    // Defensive: any failure in markdown rendering must NOT leave the user
    // staring at the skeleton. The skeleton is removed in `finally` no matter
    // what happens above.
    try {
      if (!raw.trim()) {
        container.innerHTML = '<p style="color: var(--ink-muted);">这道题暂无描述。</p>';
      } else if (typeof marked !== 'undefined') {
        try {
          marked.setOptions({ gfm: true, breaks: true });
          container.innerHTML = marked.parse(raw);
        } catch (e) {
          container.textContent = raw;
        }
      } else {
        container.innerHTML = '<pre style="white-space: pre-wrap; font-family: inherit;">' + escapeHtml(raw) + '</pre>';
      }
    } catch (err) {
      // Last-resort fallback: plain text
      try { container.textContent = raw || '加载失败'; } catch (_) {}
    } finally {
      // ALWAYS clear the loading state — the skeleton is gone either way
      try { container.setAttribute('aria-busy', 'false'); } catch (_) {}
    }
  }

  /* ---------- Result card ---------- */

  function clearResult() {
    const area = $('#resultArea');
    if (area) area.innerHTML = '';
  }

  function showResult(result) {
    const area = $('#resultArea');
    if (!area) return;
    area.innerHTML = resultCardHtml(result);
    // Scroll into view (after DOM paints)
    setTimeout(() => {
      const card = area.querySelector('.result-card');
      if (card) card.scrollIntoView({ behavior: 'smooth', block: 'center' });
    }, 80);
  }

  function resultCardHtml(result) {
    if (!result) return '';
    if (result.kind === 'pending') return pendingHtml();
    if (result.kind === 'network') return networkHtml(result);
    if (result.kind === 'test') return testCasesHtml(result.data);
    return statusHtml(result);
  }

  function pendingHtml() {
    return `
      <div class="result-card result-card--pending" role="status">
        <div class="result-card__status">
          <div class="result-card__spinner" aria-hidden="true">
            <span></span><span></span><span></span>
          </div>
          <div>
            <div class="result-card__label">编译运行中</div>
            <div class="result-card__label-sub">正在执行测试用例…</div>
          </div>
        </div>
      </div>`;
  }

  function networkHtml(result) {
    const message = result.message || '请检查网络后重试';
    return `
      <div class="result-card result-card--network" role="alert">
        <div class="result-card__status">
          <span class="result-card__badge">ERR</span>
          <div>
            <div class="result-card__label">网络异常</div>
            <div class="result-card__label-sub">${escapeHtml(message)}</div>
          </div>
        </div>
      </div>`;
  }

  // Per-case test results (LeetCode-style "Run Code" output).
  // Renders each test case with: index, status badge, input, expected (DB
  // cases only), actual, stderr. Custom cases have no `expected` field —
  // we hide that row entirely and label the status differently.
  function testCasesHtml(data) {
    const cases = (data && data.cases) || [];
    if (cases.length === 0) {
      return networkHtml({ message: '该题暂无测试用例' });
    }

    // `passed` and `total` from the server only count DB cases (custom
    // cases can't pass/fail — there's no expected to compare against).
    // For display, we still need to know how many cases are custom so we
    // can build a meaningful subtitle.
    const dbCases    = cases.filter(c => c.source !== 'custom');
    const customCount = cases.length - dbCases.length;
    const passed = (typeof data.passed === 'number') ? data.passed
                 : dbCases.filter(c => c.status === 'AC').length;
    const total  = (typeof data.total  === 'number') ? data.total  : dbCases.length;
    const allPass = !!data.allPassed;
    const compileOk = data.compileSuccess !== false;

    // Top status bar. Three states:
    //   - compile error → "CE" badge
    //   - all DB cases pass → "AC" badge
    //   - some DB cases fail → "WA" badge with pass/fail count
    // Custom cases don't affect the badge (no expected to assert).
    let badgeClass, badgeText, title, subtitle;
    if (!compileOk) {
      badgeClass = 'ce';
      badgeText  = 'CE';
      title      = '编译错误';
      subtitle   = '代码无法编译';
    } else if (total === 0 && customCount === 0) {
      // No cases at all (shouldn't happen — guarded above — but be safe)
      badgeClass = 'ac';
      badgeText  = 'OK';
      title      = '运行完成';
      subtitle   = '';
    } else if (total === 0) {
      // No official DB cases, only custom — no pass/fail to report, just
      // the count of custom cases that ran.
      badgeClass = 'ac';
      badgeText  = `${customCount}`;
      title      = '运行完成';
      subtitle   = `已运行 ${customCount} 个自定义用例`;
    } else if (allPass) {
      badgeClass = 'ac';
      badgeText  = `${passed}/${total}`;
      title      = '运行完成';
      subtitle   = customCount > 0
        ? `全部 ${total} 个官方用例通过，另运行 ${customCount} 个自定义用例`
        : '全部用例通过';
    } else {
      badgeClass = 'wa';
      badgeText  = `${passed}/${total}`;
      title      = '运行完成';
      subtitle   = `${passed} 个通过，${total - passed} 个失败`;
    }

    let html = `<div class="result-card result-card--test result-card--${badgeClass}" role="status">`;
    html += '<div class="result-card__status">';
    html += `<span class="result-card__badge">${escapeHtml(badgeText)}</span>`;
    html += '<div>';
    html += `<div class="result-card__label">${escapeHtml(title)}</div>`;
    html += `<div class="result-card__label-sub">${escapeHtml(subtitle)}</div>`;
    html += '</div></div>';

    // Compile error block (only if compile failed)
    if (!compileOk && data.compileOutput) {
      html += '<div class="result-card__output-wrap">';
      html += '<p class="result-card__output-label">编译信息</p>';
      html += `<pre class="result-card__output">${escapeHtml(data.compileOutput)}</pre>`;
      html += '</div>';
    }

    // Per-case list
    html += '<div class="test-cases">';
    cases.forEach((c) => {
      const cStatus = (c.status || 'AC').toLowerCase();
      const isCustom = c.source === 'custom';
      // Custom cases with no expected get a friendlier "完成" label
      // instead of the English "OK" we get from the server.
      const statusText = (isCustom && c.status === 'OK') ? '完成' : (c.status || '?');
      html += `<div class="test-case test-case--${escapeHtml(cStatus)}">`;
      html += '<div class="test-case__header">';
      html += `<span class="test-case__num">#${(c.position || 0) + 1}</span>`;
      // Source badge so the user can tell official vs custom at a glance.
      html += `<span class="test-case__source">${isCustom ? '自定义' : '官方'}</span>`;
      html += `<span class="test-case__status">${escapeHtml(statusText)}</span>`;
      if (typeof c.executionTimeMs === 'number') {
        html += `<span class="test-case__time">${c.executionTimeMs} ms</span>`;
      }
      html += '</div>';
      html += '<div class="test-case__body">';
      html += `<div class="test-case__row"><span class="test-case__label">输入</span><pre class="test-case__value">${escapeHtml(c.input || '')}</pre></div>`;
      // Custom cases have no expected output — skip the row entirely.
      if (!isCustom && 'expected' in c) {
        html += `<div class="test-case__row"><span class="test-case__label">预期</span><pre class="test-case__value">${escapeHtml(c.expected || '')}</pre></div>`;
      }
      if (c.actual) {
        html += `<div class="test-case__row"><span class="test-case__label">实际</span><pre class="test-case__value">${escapeHtml(c.actual)}</pre></div>`;
      }
      if (c.stderr) {
        html += `<div class="test-case__row"><span class="test-case__label">错误</span><pre class="test-case__value">${escapeHtml(c.stderr)}</pre></div>`;
      }
      html += '</div></div>';
    });
    html += '</div>';  // .test-cases
    html += '</div>';  // .result-card
    return html;
  }

  function statusHtml(r) {
    const status = (r.status || 'SYSTEM_ERROR').toLowerCase();
    const meta = STATUS_META[r.status] || { label: r.status, sub: '' };

    // Decide which output to show and its label
    let outputLabel = '标准输出';
    let output = r.stdout || '';
    if (r.status === 'CE') {
      outputLabel = '编译信息';
      output = r.compileOutput || r.error || '';
    } else if (r.status === 'RE') {
      outputLabel = '运行时输出';
      output = r.stderr || r.error || '';
    } else if (r.status === 'TLE') {
      outputLabel = '错误信息';
      output = r.error || '程序运行超时';
    } else if (r.status === 'MLE') {
      outputLabel = '错误信息';
      output = r.error || '程序超出内存限制';
    } else if (r.status === 'SYSTEM_ERROR') {
      outputLabel = '系统信息';
      output = r.error || '服务内部异常';
    }

    const hasTime = typeof r.executionTimeMs === 'number' && r.executionTimeMs >= 0;
    const statsLine = hasTime
      ? `<span>${escapeHtml(meta.sub || '')}</span>${meta.sub ? '<span class="result-card__sep">·</span>' : ''}<span>${r.executionTimeMs} ms</span>`
      : `<span>${escapeHtml(meta.sub || '')}</span>`;

    const outputBlock = output && output.trim()
      ? `<div class="result-card__output-wrap">
           <p class="result-card__output-label">${escapeHtml(outputLabel)}</p>
           <pre class="result-card__output">${escapeHtml(output)}</pre>
         </div>`
      : '';

    return `
      <div class="result-card result-card--${status}" role="status">
        <div class="result-card__status">
          <span class="result-card__badge">${escapeHtml(r.status)}</span>
          <div>
            <div class="result-card__label">${escapeHtml(meta.label)}</div>
            <div class="result-card__label-sub">${escapeHtml(meta.sub || '')}</div>
          </div>
        </div>
        <div class="result-card__stats">${statsLine}</div>
        ${outputBlock}
      </div>`;
  }

  /* ---------- Error state (problem not found, etc.) ---------- */

  function showError(title, description) {
    const container = $('.container');
    if (!container) return;
    container.innerHTML = `
      <div class="empty-state" role="alert">
        <h2 class="empty-state__title">${escapeHtml(title)}</h2>
        <p class="empty-state__description">${escapeHtml(description)}</p>
        <button class="empty-state__action" type="button" data-action="back-to-list">返回列表</button>
      </div>`;
    document.title = '题目 · CPP·OJ';
  }

  function bindGlobalActions() {
    document.addEventListener('click', (e) => {
      const btn = e.target.closest('[data-action]');
      if (!btn) return;
      const action = btn.getAttribute('data-action');
      if (action === 'back-to-list') {
        global.location.href = '/problem_list.html';
      }
    });
  }

  /* ---------- API: load ---------- */

  function loadProblem() {
    const container = $('#problemContent');
    if (container) container.setAttribute('aria-busy', 'true');

    // Manual safety net: even if api.js's 8s timeout fails for any reason
    // (e.g., the browser doesn't honor AbortController), the user will see
    // an error state after 10s instead of a permanent skeleton.
    const timeoutId = setTimeout(() => {
      showError('加载超时', '请检查网络后重试。');
    }, 10000);

    Api.getProblem(state.problemId).then((res) => {
      clearTimeout(timeoutId);
      if (res.status === 404) {
        showError('题目不存在', '这道题可能已被删除，或链接有误。');
        return;
      }
      if (!res.ok) {
        const msg = (res.data && res.data.error) || '加载失败';
        showError('无法加载题目', msg);
        return;
      }
      try {
        state.problem = res.data;
        renderPageHeader();
        renderContent();

        // If problem has a non-empty template, use it. BUT only when the
        // user has not already typed anything (i.e. no sessionStorage draft
        // was hydrated by initEditor). Overwriting a user-edited draft on
        // reload would silently discard their work.
        const tpl = (state.problem.template || '').trim();
        const codeKey = 'oj_editor_code_' + (state.problemId || 'unknown');
        const hasSavedDraft = (() => { try { return !!sessionStorage.getItem(codeKey); } catch (_) { return false; } })();
        if (tpl && state.editor && !hasSavedDraft) {
          state.initialCode = tpl;
          state.editor.setValue(tpl, -1);
          // Realign the cursor with the rendered characters after the
          // template swap (same reason as in initEditor).
          remeasureEditor(state.editor);
        } else if (tpl && state.editor) {
          // Keep the user's draft as the initial code so reset returns to it.
          state.initialCode = state.editor.getValue();
        }

        // Populate the test-case editor. The problem's DB test cases go
        // into the read-only `problemTestCases` (they are the authoritative
        // set — the server already knows about them, so we don't need to
        // re-send them on "运行测试"). The user starts with no custom
        // cases; they can add some via "+ 添加用例".
        const dbCases = (state.problem.testCases || []);
        state.problemTestCases = dbCases.map(tc => ({
          input:    tc.input    || '',
          expected: tc.expected || ''
        }));
        if (state.customTestCases.length === 0 && dbCases.length === 0) {
          // No DB cases AND no custom cases — start with one empty custom
          // row so the user has something to type into.
          state.customTestCases = [{ input: '' }];
        }
        renderTestCases();
      } catch (err) {
        // .then callback threw — show the error rather than leaving the skeleton
        showError('渲染失败', '请刷新页面重试。');
      }
    }).catch((err) => {
      // The promise chain itself rejected (network error not caught by api.js,
      // or unhandled exception bubbled up)
      clearTimeout(timeoutId);
      showError('无法加载题目', '请检查网络后重试。');
    });
  }

  /* ---------- Actions: reset, submit ---------- */

  function bindActions() {
    const resetBtn = $('#resetBtn');
    const submitBtn = $('#submitBtn');

    if (resetBtn) {
      resetBtn.addEventListener('click', () => {
        if (!state.editor) return;
        // Cancel any pending auto-save first so the change triggered by
        // setValue below does not re-write the draft 200ms later.
        if (typeof state.cancelAutoSave === 'function') {
          state.cancelAutoSave();
        }
        // The user expects reset to revert to the *original* problem
        // template (or the DEFAULT_CPP fallback). Recover that here because
        // state.initialCode may have been overwritten with the user's draft
        // when the editor was hydrated from sessionStorage.
        const originalCode = (state.problem && (state.problem.template || '').trim())
          ? state.problem.template
          : DEFAULT_CPP;
        state.editor.setValue(originalCode, -1);
        state.initialCode = originalCode;
        // Also clear the saved code so the next page load starts fresh
        const codeKey = 'oj_editor_code_' + (state.problemId || 'unknown');
        try { sessionStorage.removeItem(codeKey); } catch (_) {}
        // Place cursor on the comment line so user can immediately type
        placeCursorOnComment(state.editor, originalCode);
        clearResult();
        if (state.editor && state.editor.focus) state.editor.focus();
      });
    }

    const runTestBtn = $('#runTestBtn');
    if (runTestBtn) {
      runTestBtn.addEventListener('click', runTestCode);
    }

    if (submitBtn) {
      submitBtn.addEventListener('click', submitCode);
    }
  }

  function setSubmitting(isLoading) {
    state.submitting = isLoading;
    const submitBtn = $('#submitBtn');
    if (!submitBtn) return;
    submitBtn.classList.toggle('is-loading', isLoading);
    submitBtn.disabled = isLoading;
  }

  function submitCode() {
    if (state.submitting) return;

    // Auth gate: not logged in → bounce to login (with return= to come back here)
    if (!state.isAuthed) {
      const ret = encodeURIComponent(global.location.pathname + global.location.search);
      global.location.href = `/login.html?return=${ret}`;
      return;
    }

    if (!state.problem) {
      showResult({ kind: 'network', message: '题目尚未加载完成' });
      return;
    }

    const code = (state.editor && state.editor.getValue() || '').trim();
    if (!code) {
      showResult({ kind: 'network', message: '代码不能为空' });
      return;
    }

    setSubmitting(true);
    showResult({ kind: 'pending' });

    Api.submitCode(state.problemId, code).then((res) => {
      setSubmitting(false);

      if (res.status === 0) {
        showResult({ kind: 'network', message: (res.data && res.data.error) || '请求失败' });
        return;
      }
      if (res.status === 401 || res.status === 403) {
        // Server says not authed — bounce to login
        const ret = encodeURIComponent(global.location.pathname + global.location.search);
        global.location.href = `/login.html?return=${ret}`;
        return;
      }
      if (!res.ok) {
        const err = (res.data && res.data.error) || `错误 ${res.status}`;
        showResult({ kind: 'network', message: err });
        return;
      }

      showResult({ kind: 'result', ...res.data });
    });
  }

  // Run tests (LeetCode "Run Code" equivalent) — returns per-case AC/WA/TLE/RE
  // for each test case, with input/expected/actual so the user can see exactly
  // which case failed and how. Unlike 提交, this is informational — no submission
  // is saved. We allow this even when not authed, so the user can iterate on their
  // code before logging in.
  function runTestCode() {
    if (state.submitting) return;

    if (!state.problem) {
      showResult({ kind: 'network', message: '题目尚未加载完成' });
      return;
    }

    const code = (state.editor && state.editor.getValue() || '').trim();
    if (!code) {
      showResult({ kind: 'network', message: '代码不能为空' });
      return;
    }

    const runTestBtn = $('#runTestBtn');
    if (runTestBtn) {
      runTestBtn.classList.add('is-loading');
      runTestBtn.disabled = true;
    }
    showResult({ kind: 'pending' });

    // Collect the user-added custom cases (input only). The server already
    // knows about the problem's official test cases — it'll merge them
    // together and tag each result with `source: "db" | "custom"`.
    // We drop empty rows so the user can leave the "add case" placeholder
    // unfilled without it counting as a test case.
    const customCases = state.customTestCases
      .map(c => ({ input: c.input || '' }))
      .filter(c => c.input.length > 0);

    // We always run, even if the user has no custom cases — the official
    // DB cases will still be executed and compared.

    Api.runCode(state.problemId, code, customCases).then((res) => {
      if (runTestBtn) {
        runTestBtn.classList.remove('is-loading');
        runTestBtn.disabled = false;
      }

      if (res.status === 0) {
        showResult({ kind: 'network', message: (res.data && res.data.error) || '请求失败' });
        return;
      }
      if (res.status === 401 || res.status === 403) {
        showResult({ kind: 'network', message: '请先登录后再运行测试' });
        return;
      }
      if (!res.ok) {
        const err = (res.data && res.data.error) || `错误 ${res.status}`;
        showResult({ kind: 'network', message: err });
        return;
      }

      showResult({ kind: 'test', data: res.data });
    });
  }

  /* ---------- Init ---------- */

  // Run a step defensively — failures must not block later steps.
  // The previous flow had init()'s steps as plain statements; if initEditor
  // threw (e.g., ace failed to load from the CDN), loadProblem() was never
  // reached and the skeleton stayed forever. safeCall breaks that chain.
  function safeCall(fn, name) {
    try { fn(); }
    catch (e) {
      // Log for debugging but don't break the user experience
      try { console.error('[init]', name || 'step', 'failed:', e); } catch (_) {}
    }
  }

  function getProblemIdFromUrl() {
    const params = new URLSearchParams(global.location.search);
    const id = params.get('id');
    if (!id) return null;
    const num = parseInt(id, 10);
    if (isNaN(num) || num <= 0) return null;
    return num;
  }

  function init() {
    state.problemId = getProblemIdFromUrl();
    // Optimistic fast path: trust sessionStorage so the page doesn't flicker
    // for users who are already logged in within this tab.
    state.username  = getStoredUsername();
    state.isAuthed  = Boolean(state.username);

    if (!state.problemId) {
      showError('题目不存在', '链接缺少题号。');
      return;
    }

    // Order matters here: loadProblem is BEFORE initEditor. The problem
    // load doesn't depend on ace — so if ace fails (CDN, network, whatever),
    // the problem still loads and the user can read the description.
    // Each step is wrapped in safeCall so any single failure can't block
    // the rest of the page from initializing.
    safeCall(renderUserMenu,        'renderUserMenu');
    safeCall(renderAuthHint,        'renderAuthHint');
    safeCall(loadProblem,           'loadProblem');
    safeCall(initEditor,            'initEditor');
    safeCall(bindActions,           'bindActions');
    safeCall(bindGlobalActions,     'bindGlobalActions');
    safeCall(bindTestCaseEditor,     'bindTestCaseEditor');
    safeCall(verifyAuthWithServer,  'verifyAuthWithServer');
  }

  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', init);
  } else {
    init();
  }

  global.ProblemDetail = { state };
})(window);

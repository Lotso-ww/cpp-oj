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
    editor:    null,
    initialCode: '',
    submitting: false
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
        '  /* JetBrains Mono first for clean Latin (1em/char, cursor hugs the',
        '     character). CJK falls back to LXGW WenKai — typing Chinese will',
        '     have slight cursor drift since the two fonts have different metrics,',
        '     but Latin code looks like proper code. */',
        '  font-family: "JetBrains Mono", "LXGW WenKai", ui-monospace, SFMono-Regular, Menlo, monospace;',
        '  line-height: 1.65;',
        '}',
        '.ace-cpp-oj .ace_scroller {',
        '  font-family: "JetBrains Mono", "LXGW WenKai", ui-monospace, SFMono-Regular, Menlo, monospace;',
        '  font-size: 13px;',
        '}',
        '.ace-cpp-oj .ace_cursor {',
        '  /* 1px hairline — thin and unobtrusive, hugs the character boundary */',
        '  border-left: 1px solid #0A0A0A;',
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
        '  font-style: italic;',
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
      editorInstance.navigateToEnd();
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

  function initEditor() {
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

    defineAceTheme();

    const editor = ace.edit(editorEl, {
      mode:             'ace/mode/c_cpp',
      theme:            'ace/theme/cpp-oj',
      fontSize:         '13px',
      tabSize:          4,
      useSoftTabs:      true,
      showPrintMargin:  false,
      showLineNumbers:  true,
      showGutter:       true,
      highlightActiveLine: true,
      wrap:             false,
      cursorWidth:      1,     // 1px hairline, no thick block
      readOnly:         false
    });

    editor.renderer.setShowGutter(true);
    editor.setOptions({
      enableBasicAutocompletion:   false,
      enableLiveAutocompletion:    false,
      enableSnippets:              false,
      animatedScroll:              true,
      scrollPastEnd:               0.1
    });

    // Track focus for the border highlight
    editor.on('focus', () => editorEl.classList.add('is-focused'));
    editor.on('blur',  () => editorEl.classList.remove('is-focused'));

    state.editor = editor;
    state.initialCode = DEFAULT_CPP;

    // Restore previously-saved code from sessionStorage (survives the
    // login redirect roundtrip — when user writes code, clicks submit,
    // gets bounced to /login.html, then comes back, their code is still here).
    // Scoped per-problem so navigating between problems doesn't leak.
    const codeKey = 'oj_editor_code_' + (state.problemId || 'unknown');
    const saved = (() => { try { return sessionStorage.getItem(codeKey); } catch (_) { return null; } })();
    const isNewCode = !saved;
    editor.setValue(saved || state.initialCode, -1);

    if (isNewCode) {
      // For new code, select the comment line in main() so the user can
      // immediately type to replace it. This puts the cursor inside the
      // function — never at the start of the file (so typing into
      // #include <> is impossible by accident).
      placeCursorOnComment(editor, state.initialCode);
    } else {
      // For restored code, cursor at the end (where the user was writing)
      editor.navigateToEnd();
    }

    // Auto-save on every change
    let saveTimer = null;
    editor.session.on('change', () => {
      clearTimeout(saveTimer);
      saveTimer = setTimeout(() => {
        try { sessionStorage.setItem(codeKey, editor.getValue()); } catch (_) {}
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

  function renderUserMenu() {
    const menu = $('#userMenu');
    if (!menu) return;
    if (state.isAuthed) {
      menu.innerHTML = `
        <span class="user-menu__greeting">你好，<span class="user-menu__name">${escapeHtml(state.username)}</span></span>
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

  function handleLogout() {
    Api.logout().finally(() => {
      try { sessionStorage.removeItem('oj_username'); } catch (_) {}
      state.username = null;
      state.isAuthed = false;
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
        if (!state.isAuthed || state.username !== serverName) {
          state.username = serverName;
          state.isAuthed = true;
          try { sessionStorage.setItem('oj_username', serverName); } catch (_) {}
        }
      } else {
        // Server says not authed — clear any stale local state
        if (state.isAuthed || getStoredUsername()) {
          state.username = null;
          state.isAuthed = false;
          try { sessionStorage.removeItem('oj_username'); } catch (_) {}
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

        // If problem has a non-empty template, use it
        const tpl = (state.problem.template || '').trim();
        if (tpl && state.editor) {
          state.initialCode = tpl;
          state.editor.setValue(tpl, -1);
        }
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
        state.editor.setValue(state.initialCode, -1);
        // Also clear the saved code so the next page load starts fresh
        const codeKey = 'oj_editor_code_' + (state.problemId || 'unknown');
        try { sessionStorage.removeItem(codeKey); } catch (_) {}
        // Place cursor on the comment line so user can immediately type
        placeCursorOnComment(state.editor, state.initialCode);
        clearResult();
        if (state.editor && state.editor.focus) state.editor.focus();
      });
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
    safeCall(verifyAuthWithServer,  'verifyAuthWithServer');
  }

  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', init);
  } else {
    init();
  }

  global.ProblemDetail = { state };
})(window);

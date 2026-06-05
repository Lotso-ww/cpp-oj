/* ==========================================================================
   Auth form helpers — login & register behavior.
   Pure DOM, no frameworks. Loaded by login.html and register.html.
   ========================================================================== */

(function (global) {
  'use strict';

  /* ---------- DOM utilities ---------- */

  const $ = (sel, root = document) => root.querySelector(sel);

  function setFieldFilled(input) {
    const field = input.closest('.field');
    if (!field) return;
    field.classList.toggle('is-filled', input.value.length > 0);
  }

  function setFieldError(input, message) {
    const field = input.closest('.field');
    if (!field) return;
    field.classList.toggle('is-error', Boolean(message));
    const errEl = $('.field__error', field);
    if (errEl) errEl.textContent = message || '';
  }

  function clearFieldError(input) {
    setFieldError(input, '');
  }

  function showFormAlert(form, message) {
    const alert = $('.alert', form);
    if (!alert) return;
    $('.alert__message', alert).textContent = message;
    alert.classList.add('is-visible');
  }

  function hideFormAlert(form) {
    const alert = $('.alert', form);
    if (alert) alert.classList.remove('is-visible');
  }

  function setLoading(button, isLoading) {
    button.classList.toggle('is-loading', isLoading);
    button.disabled = isLoading;
  }

  /* ---------- Validation ---------- */

  function validateUsername(value) {
    const v = (value || '').trim();
    if (!v) return '请输入用户名';
    if (v.length < 3 || v.length > 64) return '长度需在 3 到 64 个字符之间';
    return '';
  }

  function validatePassword(value, { minLength = 1 } = {}) {
    const v = value || '';
    if (!v) return '请输入密码';
    if (v.length < minLength) return `至少需要 ${minLength} 个字符`;
    return '';
  }

  function validateConfirm(value, password) {
    if (!value) return '请再次输入密码';
    if (value !== password) return '两次密码不一致';
    return '';
  }

  /* ---------- Server error → Chinese mapping ---------- */

  // Map English error strings returned by the C++ backend to user-facing Chinese.
  // Unknown messages fall through to the caller-supplied default.
  const SERVER_ERROR_MAP = {
    'Invalid username or password':                 '用户名或密码错误',
    'Username already exists':                     '这个用户名已被占用',
    'Username must be between 3 and 64 characters':'用户名长度需在 3 到 64 个字符之间',
    'Password must be at least 6 characters':       '密码至少需要 6 个字符',
    'Username and password cannot be empty':        '用户名和密码不能为空',
    'Missing username or password':                 '请填写用户名和密码',
    'Failed to create user':                        '创建账号失败，请稍后重试',
    'Invalid JSON':                                 '请求格式错误',
    'Problem not found':                            '题目不存在',
    'Code cannot be empty':                         '代码不能为空',
    'No test cases configured for this problem':    '该题目尚未配置测试用例',
    'Missing required fields: code, problemId':     '提交缺少必要字段',
    'Missing required fields: title, difficulty, content': '题目信息不完整',
    'Invalid difficulty. Must be Easy, Medium, or Hard': '难度值无效',
    'Unauthorized':                                 '请先登录',
    'Forbidden':                                    '权限不足',
    'Not found':                                    '资源不存在'
  };

  const FALLBACK_ERROR = '出错了，请稍后重试';

  function translateError(message) {
    if (!message) return FALLBACK_ERROR;
    if (SERVER_ERROR_MAP[message]) return SERVER_ERROR_MAP[message];
    // Substring fallback so future backend errors still get a useful Chinese phrase
    const lower = String(message).toLowerCase();
    if (lower.indexOf('username') !== -1 && lower.indexOf('exist') !== -1) return '这个用户名已被占用';
    if (lower.indexOf('username') !== -1) return '用户名不符合要求';
    if (lower.indexOf('password') !== -1) return '密码不符合要求';
    if (lower.indexOf('unauthorized') !== -1) return '请先登录';
    if (lower.indexOf('forbidden') !== -1) return '权限不足';
    return message;
  }

  /* ---------- Password strength (register only) ---------- */

  function scorePassword(value) {
    if (!value) return 0;
    let score = 0;
    if (value.length >= 6) score += 1;
    if (value.length >= 10) score += 1;
    if (/[A-Za-z]/.test(value) && /\d/.test(value)) score += 1;
    if (/[^A-Za-z0-9]/.test(value) || (/[A-Z]/.test(value) && /[a-z]/.test(value))) score += 1;
    return Math.min(score, 4);
  }

  const STRENGTH_LABEL = ['', '太弱', '一般', '良好', '很强'];

  function updateStrength(passwordInput) {
    const meter = $('.strength', passwordInput.closest('.field').parentElement);
    if (!meter) return;
    const score = scorePassword(passwordInput.value);
    meter.dataset.score = String(score);
    const label = $('.strength__label', meter);
    if (label) label.textContent = STRENGTH_LABEL[score] || '';
  }

  /* ---------- Password reveal toggle ---------- */

  function bindPasswordToggle(button) {
    if (!button) return;
    button.addEventListener('click', () => {
      const input = button.previousElementSibling; // input is the previous sibling
      // Fallback: look for the field's input
      const field = button.closest('.field');
      const target = (field && field.querySelector('.field__input')) || input;
      if (!target) return;

      const isPassword = target.type === 'password';
      target.type = isPassword ? 'text' : 'password';

      // Swap icon
      const eyeOpen = button.querySelector('[data-icon="eye"]');
      const eyeOff = button.querySelector('[data-icon="eye-off"]');
      if (eyeOpen && eyeOff) {
        eyeOpen.style.display = isPassword ? 'none' : 'block';
        eyeOff.style.display  = isPassword ? 'block' : 'none';
      }

      button.setAttribute('aria-label', isPassword ? '隐藏密码' : '显示密码');
      button.setAttribute('aria-pressed', isPassword ? 'true' : 'false');
      target.focus();
    });
  }

  /* ---------- Form wiring ---------- */

  function wireForm(form, options) {
    const inputs = Array.from(form.querySelectorAll('.field__input'));

    // Mark already-filled fields on load (e.g., browser autofill)
    inputs.forEach(setFieldFilled);

    // Live update fill state and clear errors on input
    inputs.forEach((input) => {
      input.addEventListener('input', () => {
        setFieldFilled(input);
        if (input.closest('.field').classList.contains('is-error')) {
          clearFieldError(input);
        }
        if (options.onInput) options.onInput(input);
        hideFormAlert(form);
      });
      input.addEventListener('blur', () => {
        if (options.onBlur) options.onBlur(input);
      });
    });

    // Password toggles
    form.querySelectorAll('[data-toggle="password"]').forEach(bindPasswordToggle);

    // Submit
    form.addEventListener('submit', async (event) => {
      event.preventDefault();
      hideFormAlert(form);

      const values = {};
      let firstInvalid = null;

      inputs.forEach((input) => {
        const name = input.name;
        values[name] = input.value;
        const message = options.validate(input, values);
        setFieldError(input, message);
        if (message && !firstInvalid) firstInvalid = input;
      });

      if (firstInvalid) {
        firstInvalid.focus();
        return;
      }

      const submitBtn = $('button[type="submit"]', form);
      setLoading(submitBtn, true);

      try {
        await options.onSubmit(values, {
          setLoading: (loading) => setLoading(submitBtn, loading),
          showAlert: (message) => showFormAlert(form, message)
        });
      } finally {
        setLoading(submitBtn, false);
      }
    });
  }

  /* ---------- Public API ---------- */

  global.AuthForms = {
    wireForm,
    validateUsername,
    validatePassword,
    validateConfirm,
    scorePassword,
    updateStrength,
    translateError,
    showFormAlert,
    hideFormAlert,
    setFieldError,
    clearFieldError,
    setLoading
  };
})(window);

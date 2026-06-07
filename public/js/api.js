/* ==========================================================================
   API client — thin fetch wrapper for the CPP OJ backend.
   Returns { ok, status, data } so callers can branch on errors without try/catch.
   ========================================================================== */

(function (global) {
  'use strict';

  const DEFAULT_TIMEOUT = 8000;

  async function request(method, path, body) {
    const controller = new AbortController();
    const timeoutId = setTimeout(() => controller.abort(), DEFAULT_TIMEOUT);

    const init = {
      method,
      credentials: 'same-origin',
      signal: controller.signal,
      headers: { 'Accept': 'application/json' }
    };

    if (body !== undefined) {
      init.headers['Content-Type'] = 'application/json';
      init.body = JSON.stringify(body);
    }

    let res;
    try {
      res = await fetch(path, init);
    } catch (err) {
      clearTimeout(timeoutId);
      return {
        ok: false,
        status: 0,
        data: { error: err.name === 'AbortError' ? '请求超时，请稍后重试' : '网络异常，请检查连接' }
      };
    }

    clearTimeout(timeoutId);

    let data = null;
    const text = await res.text();
    if (text) {
      try { data = JSON.parse(text); }
      catch (_) { data = { raw: text }; }
    }

    return { ok: res.ok, status: res.status, data: data || {} };
  }

  const Api = {
    login(username, password) {
      return request('POST', '/api/login', { username, password });
    },
    register(username, password) {
      return request('POST', '/api/register', { username, password });
    },
    logout() {
      return request('POST', '/api/logout');
    },
    me() {
      return request('GET', '/api/me');
    },
    listProblems() {
      return request('GET', '/api/problems');
    },
    getProblem(id) {
      return request('GET', '/api/problems/' + encodeURIComponent(id));
    },
    submitCode(problemId, code) {
      return request('POST', '/api/submit', { problemId, code });
    },
    runCode(problemId, code, customCases) {
      // `customCases` are LeetCode-style user-added test cases (input only).
      // The server appends them to the problem's official DB cases — we
      // don't need to send the DB cases ourselves.
      return request('POST', '/api/run', { problemId, code, customCases: customCases || [] });
    },
    // ----- Admin endpoints (require admin role; 403 otherwise) -----
    createProblem(payload) {
      return request('POST', '/api/admin/problems', payload);
    },
    deleteProblem(id) {
      return request('DELETE', '/api/admin/problems/' + encodeURIComponent(id));
    }
  };

  global.Api = Api;
})(window);

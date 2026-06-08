/* ==========================================================================
   CPP·OJ — Landing Page Logic
   - Year stamp
   - Topbar scroll effect
   - Active nav highlight on scroll
   - Reveal-on-scroll (IntersectionObserver)
   - Smooth scroll for in-page anchors
   - Auth state → user menu (mirrors problem.js pattern)
   - Live problem count from /api/problems
   - Toast helper
   ========================================================================== */

(function () {
  'use strict';

  /* ---------- DOM utilities ---------- */
  const $  = (sel, root = document) => root.querySelector(sel);
  const $$ = (sel, root = document) => Array.from(root.querySelectorAll(sel));

  /* ---------- Year stamp ---------- */
  function stampYear() {
    const el = $('#year');
    if (el) el.textContent = String(new Date().getFullYear());
  }

  /* ---------- Topbar scroll effect ---------- */
  function wireTopbarScroll() {
    const topbar = $('#topbar');
    if (!topbar) return;
    const onScroll = () => {
      topbar.classList.toggle('is-scrolled', window.scrollY > 8);
    };
    onScroll();
    window.addEventListener('scroll', onScroll, { passive: true });
  }

  /* ---------- Smooth scroll for in-page anchors ---------- */
  function wireSmoothScroll() {
    $$('a[href^="#"]').forEach((a) => {
      a.addEventListener('click', (e) => {
        const href = a.getAttribute('href');
        if (!href || href === '#' || href.length < 2) return;
        const target = document.getElementById(href.slice(1));
        if (!target) return;
        e.preventDefault();
        const top = target.getBoundingClientRect().top + window.scrollY - 64;
        window.scrollTo({ top, behavior: 'smooth' });
      });
    });
  }

  /* ---------- Active nav highlight (scroll spy) ---------- */
  function wireNavSpy() {
    const links = $$('[data-nav]');
    if (!links.length) return;
    const map = new Map();
    links.forEach((a) => {
      const id = a.getAttribute('href');
      if (id && id.startsWith('#') && id.length > 1) {
        const sec = document.getElementById(id.slice(1));
        if (sec) map.set(sec, a);
      }
    });
    if (!map.size) return;

    const setActive = (active) => {
      links.forEach((a) => a.classList.toggle('is-active', a === active));
    };

    const obs = new IntersectionObserver(
      (entries) => {
        // Pick the section whose top is closest to ~25% of viewport from top
        const visible = entries
          .filter((e) => e.isIntersecting)
          .sort((a, b) => b.intersectionRatio - a.intersectionRatio);
        if (visible.length) {
          const link = map.get(visible[0].target);
          if (link) setActive(link);
        }
      },
      { rootMargin: '-30% 0px -55% 0px', threshold: [0, 0.25, 0.5, 0.75, 1] }
    );

    map.forEach((_, sec) => obs.observe(sec));
  }

  /* ---------- Reveal-on-scroll ---------- */
  function wireReveal() {
    const els = $$('[data-reveal]');
    if (!els.length) return;

    // Stagger reveals inside the same parent
    els.forEach((el, i) => {
      el.style.transitionDelay = `${Math.min(i, 8) * 40}ms`;
    });

    if (!('IntersectionObserver' in window)) {
      els.forEach((el) => el.classList.add('is-revealed'));
      return;
    }

    const obs = new IntersectionObserver(
      (entries) => {
        entries.forEach((entry) => {
          if (entry.isIntersecting) {
            entry.target.classList.add('is-revealed');
            obs.unobserve(entry.target);
          }
        });
      },
      { rootMargin: '0px 0px -8% 0px', threshold: 0.05 }
    );

    els.forEach((el) => obs.observe(el));
  }

  /* ---------- Auth state → user menu ---------- */
  function getStoredUsername() {
    try { return sessionStorage.getItem('oj_username') || ''; }
    catch (_) { return ''; }
  }
  function setStoredUsername(name) {
    try { sessionStorage.setItem('oj_username', name || ''); } catch (_) {}
  }
  function clearStoredUsername() {
    try { sessionStorage.removeItem('oj_username'); } catch (_) {}
  }
  function getStoredRole() {
    try { return sessionStorage.getItem('oj_role') || null; }
    catch (_) { return null; }
  }
  function setStoredRole(role) {
    try { if (role) sessionStorage.setItem('oj_role', role); } catch (_) {}
  }
  function clearStoredRole() {
    try { sessionStorage.removeItem('oj_role'); } catch (_) {}
  }

  function escapeHtml(s) {
    return String(s).replace(/[&<>"']/g, (c) => ({
      '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;'
    }[c]));
  }

  function renderUserMenu() {
    const menu = $('#userMenu');
    if (!menu) return;
    const name = getStoredUsername();
    const isAdmin = getStoredRole() === 'admin';

    if (name) {
      const mark = isAdmin
        ? '<svg class="admin-mark" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" title="管理员"><path d="M20 13c0 5-3.5 7.5-7.66 8.95a1 1 0 0 1-.67-.01C7.5 20.5 4 18 4 13V6a1 1 0 0 1 1-1c2 0 4.5-1.2 6.24-2.72a1.17 1.17 0 0 1 1.52 0C14.51 3.81 17 5 19 5a1 1 0 0 1 1 1z"/></svg>'
        : '';
      const adminLink = isAdmin
        ? '<a href="/admin.html" class="user-menu__link">管理后台</a>'
        : '';
      menu.innerHTML = `
        <span class="user-menu__greeting">${mark}你好，<span class="user-menu__name">${escapeHtml(name)}</span></span>
        ${adminLink}
        <a href="/problem_list.html" class="user-menu__link user-menu__link--primary">刷题</a>
        <button class="user-menu__logout" type="button" data-action="logout">退出</button>`;
      const btn = menu.querySelector('[data-action="logout"]');
      if (btn) btn.addEventListener('click', handleLogout);
    } else {
      const ret = encodeURIComponent(window.location.pathname + window.location.search);
      menu.innerHTML = `
        <a href="/login.html?return=${ret}" class="user-menu__link">登录</a>
        <a href="/register.html" class="user-menu__link user-menu__link--primary">注册</a>`;
    }
  }

  function handleLogout() {
    if (typeof Api === 'undefined' || !Api.logout) {
      clearStoredUsername();
      clearStoredRole();
      renderUserMenu();
      return;
    }
    Api.logout().finally(() => {
      clearStoredUsername();
      clearStoredRole();
      renderUserMenu();
      showToast('已退出登录');
    });
  }

  function verifyAuth() {
    if (typeof Api === 'undefined' || !Api.me) return;
    Api.me().then((res) => {
      if (res.ok && res.data && res.data.username) {
        const serverName = String(res.data.username);
        const serverRole = String(res.data.role || 'user');
        if (getStoredUsername() !== serverName) {
          setStoredUsername(serverName);
          setStoredRole(serverRole);
          renderUserMenu();
        } else if (getStoredRole() !== serverRole) {
          setStoredRole(serverRole);
          renderUserMenu();
        }
      } else if (getStoredUsername()) {
        clearStoredUsername();
        clearStoredRole();
        renderUserMenu();
      }
    });
  }

  /* ---------- Live problem count ---------- */
  function fetchProblemCount() {
    const el = $('[data-count="problems"]');
    if (!el || typeof Api === 'undefined' || !Api.listProblems) return;
    Api.listProblems().then((res) => {
      if (!res.ok || !res.data) return;
      const list = Array.isArray(res.data)
        ? res.data
        : (Array.isArray(res.data.problems) ? res.data.problems : null);
      if (list) {
        animateCount(el, list.length);
      }
    }).catch(() => { /* keep placeholder */ });
  }

  function animateCount(el, target) {
    const duration = 900;
    const start = performance.now();
    const startVal = 0;
    const ease = (t) => 1 - Math.pow(1 - t, 3);

    function step(now) {
      const t = Math.min(1, (now - start) / duration);
      const v = Math.round(startVal + (target - startVal) * ease(t));
      el.textContent = String(v);
      if (t < 1) requestAnimationFrame(step);
      else el.textContent = String(target);
    }
    requestAnimationFrame(step);
  }

  /* ---------- Toast helper ---------- */
  let toastTimer = null;
  function showToast(msg, ms = 1800) {
    const t = $('#toast');
    if (!t) return;
    t.textContent = msg;
    t.classList.add('is-visible');
    if (toastTimer) clearTimeout(toastTimer);
    toastTimer = setTimeout(() => t.classList.remove('is-visible'), ms);
  }

  /* ---------- Boot ---------- */
  document.addEventListener('DOMContentLoaded', function () {
    stampYear();
    wireTopbarScroll();
    wireSmoothScroll();
    wireNavSpy();
    wireReveal();
    renderUserMenu();
    verifyAuth();
    fetchProblemCount();
  });
})();

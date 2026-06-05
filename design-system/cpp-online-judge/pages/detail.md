# Problem Detail — Page Override

> Page-specific deviations from `MASTER.md` for the **题目详情页** (`/problem.html?id=X`).
> Read MASTER first; this file only describes what's *different* on this page.

---

## 1. Layout Pattern

Two-column editorial on desktop (problem left, editor right), stacked on mobile. The right column is `position: sticky` so the editor stays visible while the user scrolls the problem.

```
[topbar — same as list page]

  ← 返回列表
  · 题库 · 题目 #958
  
  多测试点题目_1780645803_1780645803_4001
  ▌▌▌ 中等

┌────────────────────────────┬────────────────────────────┐
│  ## 描述                    │ [登录后即可提交代码] 登录   │ ← auth banner (logged out only)
│  ...                        │                            │
│  ## 输入                    │ C++ 代码                   │
│  ...                        │ ┌────────────────────────┐ │
│  ## 输出                    │ │                        │ │
│  ...                        │ │   ace editor           │ │ ← sticky on desktop
│                             │ │   (C++ mode)           │ │
│  ## 示例                    │ │   theme: cpp-oj        │ │
│  Input:                     │ │                        │ │
│  1 2                        │ │                        │ │
│  Output:                    │ └────────────────────────┘ │
│  3                          │                            │
│                             │ [重置]            [提交 →] │
│                             │                            │
│                             │ ┌── AC ──────────────────┐│
│                             │ │ 通过 · 全部用例通过    ││
│                             │ │ · 1 ms                 ││
│                             │ └────────────────────────┘│
└────────────────────────────┴────────────────────────────┘
```

- **Desktop (>900px)**: `grid-template-columns: 1fr 1.15fr`, gap 48px. The right column is `position: sticky; top: 80px;` (just below the topbar).
- **Mobile (≤900px)**: stacked, editor is no longer sticky.
- **No bottom footer** — topbar carries the brand.

---

## 2. Components Added (vs. MASTER)

### 2.1 Page Header (extensions)

- **Back link** (`.page-header__back`): mono 11px uppercase, with a left-arrow icon. Sits above the eyebrow, links to `/problem_list.html`. On hover, icon slides 2px left.
- **Meta row** (`.page-header__meta`): 4px below the H1, holds the difficulty 3-bar tag.

### 2.2 Problem Layout

- CSS Grid, two columns, 48px gap on desktop, 32px on mobile.
- `align-items: start` so the right column (sticky) doesn't stretch to match the problem's height.
- The right column (`problem-layout__editor`) is a flex column with `gap: 16px`: auth banner → editor label → editor → actions → result area.

### 2.3 Problem Content (rendered markdown)

`.problem-content` is a styled container for `marked.parse(content)` output. Typography rules:

| Element | Style |
|---------|-------|
| Headings (h1-h4) | Fraunces / LXGW WenKai, weight 400-500, sizes 15-24px |
| Body text       | Inter 15px, line-height 1.7, --ink |
| Inline code     | mono 0.88em, --hairline bg, --ink-2 text, 3px radius |
| Code blocks     | mono 13px, --surface bg, hairline border, 6px radius, overflow-x auto |
| Blockquote      | left border --ink 2px, --surface bg, --ink-muted text |
| Tables          | mono 13px, hairline borders, header row --surface |
| Lists           | 24px left padding, soft markers (--ink-soft) |
| Links           | underline with --hairline-2, darkens to --ink on hover |

- `text-wrap: pretty` + `word-break: keep-all` for natural CJK line breaks.
- Uses `marked@12.0.0` with `{gfm: true, breaks: true}` (GitHub-Flavored Markdown, soft line breaks).
- **Sanitization**: marked v8+ does not render raw HTML in markdown by default; user-submitted content is safe from script injection. Trust model: problem content is set by the admin, not by arbitrary users.
- **Fallback**: if `marked` fails to load from CDN, content is rendered as a `<pre>` with `white-space: pre-wrap`.

### 2.4 Editor (ace.js)

- Loaded from `cdn.jsdelivr.net/npm/ace-builds@1.32.7/src-noconflict/` (core + C++ mode).
- **Custom theme `ace/theme/cpp-oj`**: monochrome, ink-scale syntax highlighting. No blue keywords, no green strings, no orange numbers. Visual hierarchy comes from font weight (500 on keywords/storage) and color (--ink, --ink-2, --ink-muted, --ink-soft).
  - Gutter: --paper bg, --ink-soft text
  - Active line: rgba(10,10,10,0.025)
  - Selection: rgba(10,10,10,0.08)
  - Comments: italic, --ink-soft
  - Strings/numbers/constants: --ink-muted (no color)
- **Container** (`.editor`): 480px height, hairline border, --surface bg, 6px radius. Border darkens to --ink-soft on hover, --ink when focused. Disabled state: 50% opacity, no pointer events.
- **Fallback**: if ace.js fails to load, a plain `<textarea>` is rendered with monospace styling. `state.editor` is a shim with the same `getValue/setValue/setReadOnly/clearSelection/focus` interface.
- **Options**:
  - `tabSize: 4`, `useSoftTabs: true` (C++ convention)
  - `showPrintMargin: false` (no 80-col rule noise)
  - `wrap: false` (code never wraps)
  - `readOnly: !state.isAuthed`
  - All autocompletion/snippets disabled — the editor is a clean text surface
- **Default code**: a complete C++ skeleton (include, using, main) — used when the problem has no `template` field.
- **Template**: if the problem's `template` is non-empty, it replaces the default code. This is the only "data" from the backend that initializes the editor.

### 2.5 Editor Actions

- **Reset** (`.btn--ghost`): transparent bg, hairline border, --ink-muted text. On hover → --ink text, --ink-soft border. Restores the initial code, clears selection, clears any result card, focuses the editor.
- **Submit** (`.btn--primary`): filled --ink. Loading state shows 3 dots; disabled while submitting.

### 2.6 Auth Banner

- Renders above the editor when the user is **not logged in** (`sessionStorage.oj_username` is empty).
- Three parts: lock icon in a circle · title + description · "登录" pill button.
- The login link uses `?return=` param (URL-encoded current path+search) so the user comes back here after login.
- The editor below the banner is **read-only** (visible but can't be edited). Submit button is hidden.
- **On login.html** the `safeReturn()` helper validates the return param (must start with `/`, not `//`) to prevent open redirects.

### 2.7 Result Card (7 status variants + 1 network + 1 pending)

All share the same structure: **status row** (badge + label + sub-label) · **stats row** (mono caption with separator) · **output block** (pre, monospace, scrollable).

| Status         | CSS modifier              | Badge color  | Label       | Sub label         |
|----------------|---------------------------|--------------|-------------|-------------------|
| AC             | `result-card--ac`         | success tint | 通过        | 全部用例通过      |
| WA             | `result-card--wa`         | error tint   | 答案错误    | 输出与预期不符    |
| TLE            | `result-card--tle`        | error tint   | 运行超时    | 超出时间限制      |
| RE             | `result-card--re`         | error tint   | 运行错误    | 程序异常退出      |
| MLE            | `result-card--mle`        | error tint   | 内存超限    | 超出内存限制      |
| CE             | `result-card--ce`         | error tint   | 编译错误    | 代码无法编译      |
| SYSTEM_ERROR   | `result-card--system_error` | error tint | 系统错误    | 服务内部异常      |
| Network fail   | `result-card--network`    | error tint   | 网络异常    | (api error msg)   |
| Pending        | `result-card--pending`    | paper bg     | 编译运行中  | 正在执行测试用例  |

- **Badge**: mono 13px, min-width 56px, 4px radius. 6% tinted background (rgba), 18% tinted border.
- **Output block**: only shown if the relevant output field (`stdout`, `stderr`, `compileOutput`, `error`) is non-empty. Output is HTML-escaped.
- **Stats**: mono 12px, --ink-muted. Shows "sub · {time} ms" when executionTimeMs is a number ≥ 0.
- **Pending**: 3-dot pulse spinner in place of the badge.

### 2.8 Toast (for logout feedback)

Bottom-center, same as on the list page. Shows "已退出登录" when the user logs out from this page.

---

## 3. Microcopy

| Context                  | Copy                                                  |
|--------------------------|-------------------------------------------------------|
| Page title (loading)     | "题目 · CPP·OJ"                                       |
| Page title (loaded)      | "题目 #{id} · {title} · CPP·OJ"                       |
| Meta description         | "在 CPP·OJ 上阅读题目、编写 C++ 代码并提交判题。"     |
| Back link                | "返回列表"                                            |
| Eyebrow (loading)        | "题库 · 题目"                                         |
| Eyebrow (loaded)         | "题库 · 题目 #{id}"                                   |
| H1 (loading)             | "加载中…"                                             |
| Editor label             | "C++ 代码"                                            |
| Default code (in editor) | `#include <iostream>` ... `int main() { ... }`       |
| Auth banner title        | "登录后即可提交代码"                                  |
| Auth banner description  | "注册账号，编译并提交你的 C++ 解法。"                 |
| Auth banner action       | "登录"                                                |
| Reset button             | "重置"                                                |
| Submit button            | "提交"                                                |
| Result: AC               | "通过" / "全部用例通过"                               |
| Result: WA               | "答案错误" / "输出与预期不符"                         |
| Result: TLE              | "运行超时" / "超出时间限制"                           |
| Result: RE               | "运行错误" / "程序异常退出"                           |
| Result: MLE              | "内存超限" / "超出内存限制"                           |
| Result: CE               | "编译错误" / "代码无法编译"                           |
| Result: SYSTEM_ERROR     | "系统错误" / "服务内部异常"                           |
| Result: Network          | "网络异常" / (api error message)                      |
| Result: Pending          | "编译运行中" / "正在执行测试用例…"                    |
| Output labels            | 标准输出 / 编译信息 / 运行时输出 / 错误信息 / 系统信息 |
| Logout toast             | "已退出登录"                                          |
| 404 / missing id         | "题目不存在" / "这道题可能已被删除，或链接有误。"     |

---

## 4. Behavior

### Lifecycle
1. Parse `?id=X` from URL. If missing or invalid → show error state.
2. Read `sessionStorage.oj_username` → set `state.isAuthed`.
3. Render topbar user-menu (with `?return=` bouncing back to this page).
4. Render auth banner (visible only if not authed).
5. Initialize ace editor (with `readOnly = !isAuthed`).
6. Load problem from `/api/problems/:id`.
   - 404 → show "题目不存在" empty state.
   - 200 → render header + content (via marked.js). If `template` non-empty, replace editor's default code.

### Submit flow
1. Click 提交 (or 登录 if not authed, redirects to login with return=).
2. If submitting in progress, ignore.
3. Trim code; if empty → show "代码不能为空" inline error, abort.
4. Set submit button to loading state.
5. Render pending result card (scrolls into view).
6. `POST /api/submit` with `{code, problemId}`.
7. On response:
   - 200 → render result card with status + output.
   - 0 (network) / 401 / 403 / other non-2xx → render network card with error message.

### Reset flow
1. Click 重置.
2. `editor.setValue(initialCode, -1)` (cursor at start).
3. Clear selection.
4. Clear result area.
5. Focus editor.

### Logout flow (from topbar)
1. `Api.logout()`.
2. Clear `sessionStorage.oj_username`.
3. Set `state.isAuthed = false`.
4. `editor.setReadOnly(true)`, add `is-disabled` class.
5. Hide submit button.
6. Re-render topbar (now logged-out state) and show auth banner.
7. Clear result.
8. Show "已退出登录" toast.

### Keyboard
- `Tab` order: back link → wordmark → nav → user-menu → auth-banner action (if shown) → editor (via ace) → reset → submit.
- The editor (ace) handles its own keyboard input (Tab inserts spaces, etc.).

---

## 5. State → render matrix

| isAuthed | problem | submitting | result          | rendered (right column)                              |
|----------|---------|------------|-----------------|------------------------------------------------------|
| false    | —       | false      | none            | auth banner · read-only editor · no submit · no result |
| true     | null    | false      | none            | editor · reset+submit · no result                    |
| true     | loaded  | false      | none            | editor · reset+submit · no result                    |
| true     | loaded  | true       | (pending card)  | editor (loading) · reset+submit (loading) · pending card |
| true     | loaded  | false      | network / result| editor · reset+submit · result card                  |
| true     | 404     | —          | —               | entire page → empty state "题目不存在" + 返回列表    |
| true     | null    | —          | —               | entire page → empty state "无法加载题目"            |

---

## 6. Anti-Patterns (do NOT violate)

- ❌ No colored syntax highlighting in the editor — keep the ink scale; let font weight and italic convey hierarchy.
- ❌ No bright color tints on result cards — 6% / 18% opacity on rgba is the upper bound.
- ❌ No visible 80-col print margin in the editor.
- ❌ No autocomplete popup — the editor is a clean text surface, not an IDE.
- ❌ No emoji status icons in the result card — the badge is a mono code ("AC", "WA", "ERR"), not a green check.
- ❌ No `alert()` or `confirm()` dialogs for errors — use inline result cards.
- ❌ No per-test-case result UI (the backend doesn't return that data) — show the overall status and the output.
- ❌ No eval of user-submitted content (problem template, code) anywhere on the page.
- ❌ No storing the code to localStorage across page loads — ephemeral by design (the user can re-load via "重置" anytime).

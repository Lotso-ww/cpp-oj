# Problem List — Page Override

> Page-specific deviations from `MASTER.md` for the **题目列表页** (`/problem_list.html`).
> Read MASTER first; this file only describes what's *different* on this page.

---

## 1. Layout Pattern

Editorial list with a **floating topbar** (frosted glass on supporting browsers) and a centered content column.

```
┌──────────────────────────────────────────────────────────┐
│  ╭──────────────────────────────────────────────────╮    │  ← Topbar (floating, sticky, 12px from top)
│  │ [CPP·OJ]  题目·排行·讨论           你好, lotso · 退出 │     Hairline border, 12px radius, max-width 1200px
│  ╰──────────────────────────────────────────────────╯    │
│                                                          │
│        · 题库 · 列表                                     │  ← Eyebrow (mono, 11px, with dot)
│        题目                                              │  ← H1 (LXGW WenKai + Fraunces, 44px)
│        按难度递进，从易到难。                            │  ← Subhead (15px, muted)
│                                                          │
│        [全部 66] [简单 59] [中等 7] [困难 0]   [🔍 搜索] │  ← Filter bar
│                                                          │
│        题号   标题              难度                    │  ← Table header (mono 11px)
│        ─────────────────────────────────────────        │
│        64     TC DB Test        ▌▌▌ 简单        →       │  ← Row
│        92     TC DB Test        ▌▌▌ 简单        →       │
│        …                                                  │
│                                                          │
└──────────────────────────────────────────────────────────┘
```

- **Topbar** max-width `1200px`, sticky at `top: 12px`, `backdrop-filter: blur(20px) saturate(180%)` over a translucent paper background. Falls back to opaque `var(--paper)` on browsers without `backdrop-filter`.
- **Container** max-width `960px`, centered. Padding `24px` (desktop) / `16px` (mobile).
- **No bottom footer** — the topbar carries the brand; the page is content-focused.

---

## 2. Components Added (vs. MASTER)

These are the new components introduced for this page. All reuse the same design tokens from MASTER.

### 2.1 Topbar

- Floating, sticky, hairline border, 12px corner radius.
- Background: `rgba(250, 250, 247, 0.72)` + `backdrop-filter: blur(20px) saturate(180%)` when supported; opaque `var(--paper)` otherwise.
- Height 56px (52px on mobile).
- Contains: wordmark · nav · user-menu (`margin-left: auto` pushes user-menu to the right).

### 2.2 Nav

- Inline-flex, gap 24px.
- Item: 13px Inter Medium, `--ink-muted` idle, `--ink` on hover, `--ink` when active.
- Active indicator: 1px `var(--ink)` underline on the bottom edge.
- Disabled items: `[aria-disabled="true"]` → 50% opacity, cursor not-allowed, hover does nothing.
- **Mobile (<640px)**: nav is hidden. (Future: hamburger menu. Current list is still reachable via direct URL or browser history.)

### 2.3 User Menu

- Right-aligned (pushed with `margin-left: auto`).
- **Logged in** (sessionStorage has `oj_username`):
  - Greeting: "你好，{username}" — username rendered in mono 12px for an "engineering" feel.
  - Action: "退出" text button.
- **Logged out**:
  - "登录" text link → `/login.html`
  - "注册" filled-black pill → `/register.html`
- The "register" pill is the only place a non-link element mimics a button; this is acceptable since the brand intent is to **subtly nudge** new users without overdoing CTAs.

### 2.4 Filter Bar

- Flex with `justify-content: space-between`, wraps on mobile.
- Left: **Filter chips** (4: 全部 / 简单 / 中等 / 困难) with count next to each.
- Right: **Search input** (240px wide, hairline underline, magnifier icon on the left).

### 2.5 Filter Chips

- 13px Inter Medium, 4px radius (intentionally **not** a full pill — more architectural, less bubbly).
- Idle: transparent, `--ink-muted` text.
- Hover: transparent + hairline border, `--ink` text.
- Active: `--ink` background, `--paper` text, `--ink` border.
- Count: 11px JetBrains Mono, 4px gap, `--ink-soft` idle / 50% paper white when active.

### 2.6 Search Input

- Type `search`, no border except a bottom hairline.
- Magnifier icon (Lucide `search`, 14px) absolutely positioned on the left.
- 240px wide, hairline underline, 22px left padding for the icon.
- Native `::-webkit-search-cancel-button` restyled to a small X using pure CSS gradients (no extra SVG).

### 2.7 Problem Table

- 4 columns: 题号 (64px mono) / 标题 (flex) / 难度 (110px) / action (32px).
- Header: 11px JetBrains Mono, uppercase, `--ink-soft`, bottom border `var(--hairline-2)`.
- Rows: 14px Inter, 16px vertical padding, 1px hairline between rows.
- Hover: row background → `var(--surface)` (white, 6% lift off the paper).
- Action arrow (`→`): hidden by default (`opacity: 0`, `translateX(-4px)`), fades in on row hover/focus-within.
- **Row click**: navigates to `/problem.html?id={id}`. The title is a real `<a>` for keyboard accessibility; the row click handler skips if the click target is already a link.
- **Mobile (<640px)**: difficulty label (`简单`/`中等`/`困难`) is hidden — the 3-bar is sufficient. Wrapper has `overflow-x: auto` if content overflows.

### 2.8 Difficulty Indicator (3-bar)

- Inline SVG with 3 rounded rectangles, each 3×9px.
- Inactive bars: `var(--hairline)`.
- Active bars: `var(--ink)`.
- Count of active bars = level (1/2/3 for Easy/Medium/Hard).
- No colored accent — all bars use the ink scale, keeping the design monochromatic.
- The visible Chinese label (`简单` / `中等` / `困难`) is omitted on mobile to save space.

### 2.9 Skeleton

- 6 placeholder rows on initial load.
- 14px tall, 4px radius, `var(--hairline)` background.
- Pulses (`opacity: 0.55 → 1`) over 1.6s, `var(--ease-out)`.
- Disabled by `prefers-reduced-motion`.

### 2.10 Empty State

- Centered column, 96px vertical padding, `page-enter` fade-in.
- Title: LXGW WenKai 24px, weight 300, `--ink`.
- Description: 14px, `--ink-muted`, max-width 360px, with the same anti-orphan rules as the subhead.
- Action button: 1px hairline border, transparent background; on hover → filled black with paper text.

Three empty-state variants:
- "暂无题目" — when API returns 0 problems.
- "没有匹配的题目" + "查看全部" button — when filter/search has no match.
- "无法加载题目" + "重试" button — when API call fails.

### 2.11 Toast

- Bottom-center, 24px from bottom, fixed position.
- `--ink` background, `--paper` text, 8px radius.
- Fade + slight slide-up on enter.
- Used for transient messages ("已退出登录").

---

## 3. Microcopy

| Context             | Copy                                                            |
|---------------------|-----------------------------------------------------------------|
| Page title          | "题目 · CPP·OJ"                                                 |
| Meta description    | "浏览所有题目，按难度递进练习。CPP·OJ — …"                     |
| Eyebrow             | "题库 · 列表"                                                   |
| H1                  | "题目"                                                          |
| Subhead             | "按难度递进，从易到难。"                                        |
| Filter: all         | "全部" + count                                                  |
| Filter: easy        | "简单" + count                                                  |
| Filter: medium      | "中等" + count                                                  |
| Filter: hard        | "困难" + count                                                  |
| Search placeholder  | "按题号或标题搜索"                                              |
| Table header        | 题号 / 标题 / 难度                                              |
| Difficulty labels   | 简单 / 中等 / 困难                                              |
| Empty (no data)     | "暂无题目" / "题库正在建设中，敬请期待。"                       |
| Empty (no match)    | "没有匹配的题目" / "尝试调整过滤或搜索条件。" / "查看全部"      |
| Error (load failed) | "无法加载题目" / "请检查网络后重试。" / "重试"                  |
| User greeting       | "你好，{username}"                                              |
| Logout button       | "退出"                                                          |
| Logout toast        | "已退出登录"                                                    |
| Logged-out links    | "登录" / "注册"                                                 |

---

## 4. Behavior

### Data flow
1. `init()` → `renderUserMenu()` reads `sessionStorage.oj_username` (set by `login.html` on success).
2. `loadProblems()` calls `Api.listProblems()` → stores result in `state.all`, sorts by id ascending.
3. `render()` applies `state.difficulty` and `state.search` filters, then renders skeleton / table / empty / error.

### Filter & search
- Filter: client-side, instant. The chip count is computed from the **unfiltered** list (always shows the total per difficulty).
- Search: 80ms debounce on input. Matches by id prefix (`String(id).startsWith(q)`) or title substring (case-insensitive). Empty query → show all.

### Auth UX
- `/api/problems` is public, so the list works without login. The user menu shows logged-in/out state from sessionStorage only.
- On logout: `Api.logout()` → `sessionStorage.removeItem('oj_username')` → toast → 700ms delay → redirect to `/login.html`.

### Keyboard
- Tab order: wordmark → nav items → search → filter chips → table rows (via the title `<a>`).
- Enter on a focused title link navigates to the problem detail.

---

## 5. State → render matrix

| state.loading | state.error | state.all | filtered | rendered                          |
|---------------|-------------|-----------|----------|-----------------------------------|
| true          | —           | —         | —        | 6-row skeleton table              |
| false         | truthy      | —         | —        | "无法加载题目" + 重试             |
| false         | null        | 0         | 0        | "暂无题目"                        |
| false         | null        | >0        | 0        | "没有匹配的题目" + 查看全部       |
| false         | null        | >0        | >0       | Table with rows                   |

---

## 6. Anti-Patterns (do NOT violate)

- ❌ No colored difficulty badges (the entire palette is ink + paper). Bars are a stroke pattern, not colored chips.
- ❌ No card chrome around rows — let hairline dividers do the work.
- ❌ No scale/translate hover that shifts layout — use background-color and arrow fade only.
- ❌ No round "pill" buttons for chips — 4px radius is the upper bound.
- ❌ No pagination UI (66 items fit comfortably; add when total exceeds 200).
- ❌ No skeletons that pulse at 100% opacity — keep it at 55% for restraint.

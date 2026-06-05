# Auth Pages (login & register) — Page Override

Both `/login.html` and `/register.html` share an identical shell and visual language. This file governs both.

---

## Page Architecture

```
┌──────────────────────────────────────────────────────┐
│  (background: warm paper + subtle radial gradient)   │
│                                                       │
│            [logo · CPP · OJ]            ← top        │
│                                                       │
│              Welcome back                            │
│        Sign in to continue your work.                │
│                                                       │
│        [ Username field with icon ]                  │
│        [ Password field with icon + toggle ]         │
│                                                       │
│        [ Sign in → ]   ← primary CTA                 │
│                                                       │
│      New here? Create an account →                   │
│                                                       │
│                                                       │
│                                                       │
│  BUILT WITH CARE · v1.0.0 · 2026      ← footer       │
└──────────────────────────────────────────────────────┘
```

- **No card. No panel. No box.** The form sits directly on the paper.
- **No side image, no marketing illustration.** Restraint.
- **No nav bar.** Auth is a single focused task.

---

## Login page specifics

| Field    | Constraints                | Error message                       |
|----------|----------------------------|-------------------------------------|
| Username | required, 3–64 chars       | "Please enter your username."       |
| Password | required, ≥ 1 char client  | "Please enter your password."       |

Server errors map to the form level (banner above CTA), not individual fields.

| Server response            | UI treatment                                        |
|----------------------------|-----------------------------------------------------|
| 401 invalid credentials    | Inline form-level error: "Username or password is incorrect." |
| 400 malformed              | Form-level error: "Something went wrong. Try again." |
| Network failure            | Form-level error: "Connection failed. Check your network." |
| 200 success                | Brief success state, then redirect to `/problem_list.html` |

## Register page specifics

| Field     | Constraints                | Client error message                        |
|-----------|----------------------------|---------------------------------------------|
| Username  | required, 3–64 chars       | "Use 3 to 64 letters or numbers."           |
| Password  | required, ≥ 6 chars        | "At least 6 characters."                    |
| Confirm   | must match password        | "Passwords don't match."                    |

| Server response            | UI treatment                                        |
|----------------------------|-----------------------------------------------------|
| 400 username exists        | Inline on username: "This username is taken."      |
| 400 validation             | Inline on the relevant field                        |
| 201 success                | Brief success state, then redirect to `/login.html` |

---

## Password strength indicator (register only)

A 1px tall, full-width hairline directly below the password field that fills from left to right as the password strengthens. Four states:

| Score | Width | Color           | Meaning       |
|-------|-------|-----------------|---------------|
| 0     | 0%    | transparent     | empty         |
| 1     | 25%   | `--ink-soft`    | too weak      |
| 2     | 50%   | `--ink-muted`   | okay          |
| 3     | 75%   | `--ink`         | good          |
| 4     | 100%  | `--ink`         | strong        |

Strength logic (client-side, simple heuristic):
- length ≥ 6 → +1
- length ≥ 10 → +1
- has letter and digit → +1
- has symbol or mixed case → +1

---

## Microcopy

| Context                | Copy                                                  |
|------------------------|-------------------------------------------------------|
| Login heading          | "欢迎回来"                                            |
| Login subhead          | "登录后继续你的练习，从上次离开的地方开始。"          |
| Login CTA              | "登录"                                                |
| Login alt link         | "还没有账号？ 创建一个 →"                             |
| Register heading       | "创建你的账号"                                        |
| Register subhead        | "几个信息即可开始，无需邮箱。"                        |
| Register CTA           | "创建账号"                                            |
| Register alt link      | "已有账号？ 去登录 →"                                 |
| Loading (login)        | "登录中" + animated dots                              |
| Loading (register)     | "创建中" + animated dots                              |
| Success (login)        | "欢迎回来，正在跳转…"                                 |
| Success (register)     | "账号已创建，正在跳转…"                               |
| Password toggle (a11y) | `aria-label="显示密码"` / `"隐藏密码"`                |
| Footer                 | "精心打造 · v1.0.0 · 2026"                            |
| Brand (always)         | `CPP · OJ` (English, kept across languages)           |

### Server error → Chinese mapping

The C++ backend returns English error messages. The frontend maps them via `AuthForms.translateError(msg)` in `public/js/auth.js`. Keep the map in sync with the backend's `error` strings; unknown messages fall through to a substring match or the caller-supplied default.

| Backend `error` (English)                            | UI (Chinese)                              |
|-------------------------------------------------------|-------------------------------------------|
| `Invalid username or password`                       | 用户名或密码错误                          |
| `Username already exists`                             | 这个用户名已被占用                        |
| `Username must be between 3 and 64 characters`        | 用户名长度需在 3 到 64 个字符之间        |
| `Password must be at least 6 characters`              | 密码至少需要 6 个字符                     |
| `Missing username or password`                       | 请填写用户名和密码                        |
| `Invalid JSON`                                       | 请求格式错误                              |
| `Problem not found`                                  | 题目不存在                                |
| `Unauthorized`                                       | 请先登录                                  |
| `Forbidden`                                          | 权限不足                                  |

---

## Transitions between pages

There is no page transition animation. The form just appears, content changes instantly. The page-enter fade is the only motion (400ms ease-out on `opacity` of the shell).

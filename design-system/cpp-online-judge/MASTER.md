# CPP Online Judge — Design System (MASTER)

> Curated to match the brief: *smooth, premium, serene, like a top-tier Swiss spa.* Pixel-perfect, restrained, expensive.

---

## 1. Design Philosophy

- **Restraint is the statement.** No color where structure will do.
- **Editorial typography over UI typography.** Generous whitespace, tight letterspacing, light weights for display text.
- **Engineered serenity.** The product is a C++ Online Judge — sharp, technical. The *feel* should be the opposite: warm, calm, considered.
- **One job per screen.** Login signs you in. Register creates an account. No marketing, no upsell.

### Anti-Patterns (do NOT use)
- Emojis as UI icons
- Bright accent colors (green, blue, purple)
- Drop shadows, glows, gradients as decoration
- Scale-based hover that shifts layout
- Rounded "pill" buttons or 16px+ border-radius
- Stock illustrations or hero photos
- Marketing copy on auth screens

---

## 2. Pattern: Editorial Single-Column

Inspired by Stripe, Linear, Vercel auth screens. A single, perfectly centered column on a warm paper canvas. No card chrome. The form sits directly on the page with deliberate spacing.

```
[   8rem top breathing room   ]
[                              ]
[       wordmark (small)       ]
[                              ]
[        Display Heading       ]   ← Fraunces 300, 48px
[       One-line subhead       ]   ← Inter 400, 15px, muted
[                              ]
[       Form fields            ]
[       (floating labels)      ]
[                              ]
[       Primary action         ]
[                              ]
[       Secondary link         ]
[                              ]
[   4rem breathing room        ]
[   micro footer (mono)        ]
```

### Spacing Rhythm (8px base)
- Section vertical gap: `64px` (8 units)
- Field-to-field gap: `24px` (3 units)
- Label-to-input: `8px`
- Heading-to-subhead: `12px`
- Subhead-to-form: `48px`
- Form-to-CTA: `32px`
- Page top padding: `min(12vh, 160px)`

---

## 3. Color Palette — "Engineered Paper"

| Token              | Hex        | Use                                                    |
|--------------------|------------|--------------------------------------------------------|
| `--ink`            | `#0A0A0A`  | Primary text, primary button background, focus rings   |
| `--ink-2`          | `#1F1F1F`  | Hover state for primary button                         |
| `--ink-muted`      | `#6B6B6B`  | Subheads, helper text, monospace eyebrows              |
| `--ink-soft`       | `#9A9A9A`  | Floating labels (idle), placeholders                    |
| `--paper`          | `#FAFAF7`  | Page background — warm off-white (NOT pure white)      |
| `--surface`        | `#FFFFFF`  | Input background (slight lift off the paper)           |
| `--hairline`       | `#E8E6E1`  | Default borders, hairline rules                        |
| `--hairline-2`     | `#D6D3CC`  | Stronger hairline for separators                        |
| `--accent`         | `#0A0A0A`  | Single accent = ink black (restraint over color)       |
| `--error`          | `#B91C1C`  | Used only for error states; never decoration           |
| `--success`        | `#15803D`  | Used only for success states; never decoration         |
| `--focus-ring`     | `rgba(10,10,10,0.08)` | 2px ring offset 2px around focusable elements |

**Why this palette:** Black on warm paper (not pure white) reads as *expensive*. No colored accent — the only "color story" is the temperature of grey. Errors are the only time the eye sees red.

### Gradient (used ONCE — for page background depth)
```css
background:
  radial-gradient(ellipse 80% 60% at 50% 0%, rgba(10,10,10,0.025), transparent 60%),
  var(--paper);
```

---

## 4. Typography

| Role             | Family               | Weight | Size (desktop / mobile)     | Tracking | Line-height |
|------------------|----------------------|--------|-----------------------------|----------|-------------|
| Wordmark         | Inter                | 500    | 13px / 13px                 | +0.08em  | 1.0         |
| Eyebrow (mono)   | JetBrains Mono       | 400    | 11px / 11px                 | +0.04em  | 1.0         |
| Display heading  | Fraunces             | 300    | 44px / 32px                 | -0.025em | 1.1         |
| Subhead          | Inter                | 400    | 15px / 15px                 | -0.005em | 1.5         |
| Field label      | Inter                | 500    | 12px / 12px                 | +0.02em  | 1.0         |
| Field input      | Inter                | 400    | 15px / 15px                 | -0.005em | 1.4         |
| Button           | Inter                | 500    | 14px / 14px                 | +0.005em | 1.0         |
| Helper / link    | Inter                | 400    | 13px / 13px                 | -0.003em | 1.5         |
| Footer mono      | JetBrains Mono       | 400    | 11px / 11px                 | +0.02em  | 1.4         |

**Font loading:**
```html
<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link href="https://fonts.googleapis.com/css2?family=Inter:wght@300;400;500;600&family=Fraunces:opsz,wght@9..144,300;9..144,400&family=JetBrains+Mono:wght@400;500&display=swap" rel="stylesheet">
```

**Why this pairing:** Fraunces gives the editorial, spa-grade warmth for the hero heading. Inter handles UI with neutral precision. JetBrains Mono for engineering micro-details (eyebrow, footer).

---

## 5. Components

### 5.1 Floating-Label Input (the workhorse)

A single hairline underline. Label sits inside, floats up on focus or when filled.

```css
.field {
  position: relative;
  padding-top: 24px;
}
.field input {
  width: 100%;
  padding: 8px 36px 8px 0;        /* right padding leaves room for icon */
  background: transparent;
  border: 0;
  border-bottom: 1px solid var(--hairline);
  font: 400 15px/1.4 'Inter', sans-serif;
  color: var(--ink);
  outline: none;
  transition: border-color 200ms cubic-bezier(0.2, 0, 0, 1);
}
.field input:hover { border-bottom-color: var(--ink-soft); }
.field input:focus { border-bottom-color: var(--ink); }
.field label {
  position: absolute;
  top: 32px;                      /* baseline-aligned with input text */
  left: 0;
  font: 500 12px/1 'Inter', sans-serif;
  color: var(--ink-soft);
  letter-spacing: 0.02em;
  pointer-events: none;
  transform-origin: left top;
  transition: transform 200ms cubic-bezier(0.2, 0, 0, 1),
              color 200ms cubic-bezier(0.2, 0, 0, 1);
}
.field.is-filled label,
.field input:focus + label {
  transform: translateY(-24px) scale(0.85);
  color: var(--ink);
}
```

- Icon: 16px Lucide stroke (1.5px stroke-width), positioned absolute right, color `--ink-soft`, transitions to `--ink` on focus.
- Error state: border-bottom becomes `--error`, label becomes `--error`, helper text appears in `--error` below.

### 5.2 Primary Button

- Full-width, height 44px.
- Background `--ink`, text `#FAFAF7` (the paper color, for harmony).
- Border-radius: 6px (not pill, not square).
- Hover: background `--ink-2`. No transform.
- Active (press): background `#000`. No transform.
- Disabled: 50% opacity, cursor not-allowed.
- Loading: button text replaced with three animated dots; button remains 44px tall, never collapses.

```css
.btn-primary {
  width: 100%;
  height: 44px;
  border: 0;
  border-radius: 6px;
  background: var(--ink);
  color: var(--paper);
  font: 500 14px/1 'Inter', sans-serif;
  letter-spacing: 0.005em;
  cursor: pointer;
  transition: background-color 180ms cubic-bezier(0.2, 0, 0, 1);
}
.btn-primary:hover { background: var(--ink-2); }
.btn-primary:active { background: #000; }
.btn-primary:disabled { opacity: 0.5; cursor: not-allowed; }
```

### 5.3 Inline Link ("Don't have an account? Create one →")

Centered below the button. Color `--ink-muted`. Hover: color `--ink`. Arrow (→) is a separate span that translates 2px right on hover.

### 5.4 Wordmark

A small 13px monospace mark reading `CPP · OJ` (or `OJ.CPP`) with a tiny inline SVG `terminal` icon (16px) to its left. Color `--ink`. Acts as a quiet "logo."

### 5.5 Footer

A single line of JetBrains Mono 11px, color `--ink-soft`, at the bottom of the page. Reads something like:
```
BUILT WITH CARE · v1.0.0 · {YEAR}
```

---

## 6. Motion

| Interaction          | Duration | Easing                          |
|----------------------|----------|---------------------------------|
| Label float          | 200ms    | `cubic-bezier(0.2, 0, 0, 1)`   |
| Input underline      | 200ms    | `cubic-bezier(0.2, 0, 0, 1)`   |
| Button bg            | 180ms    | `cubic-bezier(0.2, 0, 0, 1)`   |
| Link color           | 180ms    | `cubic-bezier(0.2, 0, 0, 1)`   |
| Link arrow translate | 220ms    | `cubic-bezier(0.2, 0, 0, 1)`   |
| Error fade-in        | 220ms    | `ease-out`                      |
| Page enter (fade)    | 400ms    | `cubic-bezier(0.16, 1, 0.3, 1)`|

All motion respects `@media (prefers-reduced-motion: reduce)` — transitions become 0ms, opacity stays at 1.

---

## 7. Iconography

- **Set:** Lucide (https://lucide.dev). 16px size in inputs, 14px in micro-elements, 24px max elsewhere.
- **Stroke:** 1.5px, `currentColor`, round caps & joins.
- **Never** use emojis.
- **Never** use icon fonts.

Icons used in auth pages:
- `terminal` — wordmark
- `user-round` — username field
- `lock` — password field
- `eye` / `eye-off` — password reveal toggle
- `arrow-right` — link accent, button loading state
- `alert-circle` — inline error indicator
- `check` — success state

---

## 8. Layout & Responsiveness

### Container
```css
.auth-shell {
  width: 100%;
  max-width: 380px;
  margin: 0 auto;
  padding: 0 24px;
}
```

### Vertical positioning
The form column is centered vertically on desktop with a slight upward bias (logo, then form, then footer pinned to bottom). On mobile, the form sits at the top with the footer at the bottom (CSS grid with `min-height: 100dvh`).

### Breakpoints
- `< 480px` (mobile): heading 32px, subhead 15px, button height 48px (touch target)
- `≥ 480px` (desktop): heading 44px, button height 44px

### Touch targets
All interactive elements are at least 44×44px on touch devices.

---

## 9. Accessibility

- Every input has a real `<label>` (visually styled as floating, semantically present).
- All buttons are `<button type="submit">` (or `type="button"` where appropriate).
- Focus visible: 2px `--ink` ring with 2px offset, never `outline: none` without replacement.
- Color contrast: `--ink` on `--paper` = 18.4:1 (AAA). `--ink-muted` on `--paper` = 5.7:1 (AA).
- Error messages associated with inputs via `aria-describedby` and `aria-invalid="true"`.
- Page has a unique, descriptive `<title>` and a single `<h1>`.
- `prefers-reduced-motion` respected globally.

---

## 10. Pre-Delivery Checklist

- [ ] No emojis as UI icons (Lucide SVG only)
- [ ] No `outline: none` without a visible focus replacement
- [ ] `cursor-pointer` on every interactive element
- [ ] Hover states use color/opacity, never scale/shift
- [ ] Page background is warm paper, not pure white
- [ ] All inputs have associated labels (not placeholder-only)
- [ ] All buttons have visible focus states
- [ ] Mobile: 375px width renders without horizontal scroll
- [ ] Reduced-motion: page renders without animation
- [ ] All transitions are 150–300ms
- [ ] No more than ONE accent color used; here, the "accent" is the ink itself

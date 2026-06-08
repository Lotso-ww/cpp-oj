# CPP·OJ 自动化测试操作手册（playwright-cli）

> 本文档配套 `web自动化测试文档.md` 使用，聚焦**操作步骤**与**可复用的执行模式**，目的是让任何执行者能按部就班、高效率地完成回归测试。
> **测试目标地址**：`http://124.222.15.175:8080`
> **管理员账户**：`admin` / `admin123`

---

## 一、环境与启动

### 1.1 工具检查

```powershell
# 验证 playwright-cli 可用（优先本地 npx，全局命令也可）
npx --no-install playwright-cli --version
```

预期输出形如 `0.1.13`。若失败：

```powershell
npm install -g @playwright/cli@latest
```

### 1.2 启动浏览器（有头模式）

```powershell
# 首次启动并直接跳到目标页
npx --no-install playwright-cli open --headed http://124.222.15.175:8080/register.html
```

> **关键点**：
> - 必须带 `--headed`，否则窗口不可见，肉眼无法观察。
> - 浏览器实例默认名为 `default`；后续命令省略 `-s` 即操作该实例。
> - 浏览器打开后会自动 `goto` 到指定 URL，可直接进入测试。

### 1.3 调整窗口大小（建议 1920×1080，与文档约定一致）

```powershell
npx --no-install playwright-cli resize 1920 1080
```

### 1.4 全局操作节奏

```powershell
# 每一步之间等待 1 秒，便于肉眼观察；推荐作为每个 playwright-cli 命令的前置
Start-Sleep -Seconds 1; npx --no-install playwright-cli <command>
```

> **PowerShell 5.1 链式命令**：使用 `;`（不是 `&&`）。需要"上一步成功才执行"用 `; if ($?) { ... }`。

### 1.5 测试结束

```powershell
npx --no-install playwright-cli close
```

---

## 二、核心命令速查（本次高频使用）

| 命令 | 用途 | 示例 |
|------|------|------|
| `goto <url>` | 导航到指定 URL | `goto http://124.222.15.175:8080/login.html` |
| `snapshot` | 导出整页 a11y 树（含 `ref=eN`） | `snapshot` |
| `snapshot <ref>` | 仅导出指定元素子树 | `snapshot e18` |
| `fill <ref> "<text>"` | 清空并填充输入框 | `fill e20 "admin"` |
| `click <ref>` | 点击元素 | `click e32` |
| `press <key>` | 按键 | `press Enter` / `press Escape` |
| `--raw eval "<js>"` | 执行 JS 并只返回值 | `--raw eval "document.title"` |
| `requests` | 列出所有网络请求 | `requests \| Select-String register` |
| `console` | 查看控制台消息 | `console` |
| `cookie-clear` / `sessionstorage-clear` | 清状态 | （见第三章） |
| `close` | 关闭浏览器 | `close` |

### 2.1 ref 的生命周期

- `ref=eN` **仅在最近一次 `snapshot` 之后有效**。
- 下列情况**必须重新 snapshot**：
  1. 页面跳转、刷新。
  2. 元素消失/出现（如点击后弹窗、aria-label 改变）。
  3. 异步更新 DOM 后（如 setTimeout 触发跳转）。
- 示例：眼睛按钮点击后 `aria-label` 从「显示密码」变为「隐藏密码」，ref 会重新编号。

### 2.2 获取元素属性的标准模式

```powershell
# 文本内容
npx --no-install playwright-cli --raw eval "el => el.textContent" e45

# 自定义属性 / data-* / aria-*
npx --no-install playwright-cli --raw eval "el => el.getAttribute('data-score')" e22
npx --no-install playwright-cli --raw eval "el => el.getAttribute('aria-pressed')" e23

# 直接通过选择器查（无需 ref）
npx --no-install playwright-cli --raw eval "document.querySelector('#strength').dataset.score"
```

> **推荐**：判断类断言（如"得分是否为 3"）优先用 `--raw eval`，比读 snapshot 文本更可靠、更快。

### 2.3 网络请求核对

```powershell
# 列出所有请求
npx --no-install playwright-cli requests

# 仅看某个接口
npx --no-install playwright-cli requests | Select-String -Pattern "register" -Context 0,2
```

输出形如 `> 22. [POST] http://124.222.15.175:8080/api/register => [400] Bad Request`。

---

## 三、测试间状态清理（beforeEach 替代）

每个用例执行前**必须**清掉 Cookie 与 sessionStorage，避免前序用例残留。

```powershell
npx --no-install playwright-cli cookie-clear
npx --no-install playwright-cli sessionstorage-clear
```

> **批量模板**（PowerShell）：
> ```powershell
> npx --no-install playwright-cli cookie-clear; if ($?) { npx --no-install playwright-cli sessionstorage-clear }
> ```

不需要刷新的"纯前端校验"用例（TC-003 ~ TC-005、TC-006、TC-007）可在同一页面连续操作；需要"全新态"的用例（TC-002、TC-008 等）显式 `goto` 到目标页即可。

---

## 四、模块化操作模板

### 4.1 认证页（注册/登录）

**模板 A：表单填写 + 提交 + 断言**

```powershell
# 1) 打开页面（如果还在用同一上下文，可省）
npx --no-install playwright-cli goto http://124.222.15.175:8080/register.html
Start-Sleep -Milliseconds 800  # 等待页面稳定

# 2) snapshot 拿 ref
npx --no-install playwright-cli snapshot

# 3) 填写三件套
npx --no-install playwright-cli fill e20 "<username>"
npx --no-install playwright-cli fill e22 "<password>"
npx --no-install playwright-cli fill e30 "<confirm>"

# 4) 提交
npx --no-install playwright-cli click e32

# 5) 断言：URL 变化 / Alert 文本 / API 状态码
npx --no-install playwright-cli --raw eval "location.pathname"   # 应为 /login.html
npx --no-install playwright-cli requests | Select-String "register"
```

**模板 B：密码强度 / 显示隐藏（不提交）**

```powershell
# 强度计分（用 eval 拿 data-score + label）
npx --no-install playwright-cli fill e22 "Abc12345"
npx --no-install playwright-cli --raw eval "JSON.stringify({score: document.querySelector('#strength').dataset.score, label: document.querySelector('.strength__label').textContent})"

# 显示/隐藏切换
npx --no-install playwright-cli fill e22 "Test1234"
npx --no-install playwright-cli click e23
npx --no-install playwright-cli --raw eval "JSON.stringify({type: document.querySelector('input[name=password]').type, pressed: document.querySelector('[data-toggle=password]').getAttribute('aria-pressed')})"
```

### 4.2 题目模块

```powershell
# 列表加载
npx --no-install playwright-cli goto http://124.222.15.175:8080/problem_list.html
Start-Sleep -Seconds 1
npx --no-install playwright-cli --raw eval "document.querySelectorAll('.problem-table__row').length"

# 难度筛选（按 data-difficulty 属性）
npx --no-install playwright-cli click ".filter__chip[data-difficulty='Easy']"
Start-Sleep -Milliseconds 300
npx --no-install playwright-cli --raw eval "document.querySelector('.filter__chip.is-active')?.dataset.difficulty"

# 关键字搜索
npx --no-install playwright-cli fill "#searchInput" "<keyword>"
Start-Sleep -Milliseconds 300  # 防抖 80ms + 余量
npx --no-install playwright-cli --raw eval "[...document.querySelectorAll('.problem-table__row .problem-table__title')].map(t => t.textContent)"
```

### 4.3 题目详情页（编辑器、运行、提交）

```powershell
# 进入详情页
npx --no-install playwright-cli goto "http://124.222.15.175:8080/problem.html?id=<id>"

# 等待 Ace 编辑器挂载
Start-Sleep -Seconds 1

# 设置代码（务必用 setValue，避免逐字键入干扰）
npx --no-install playwright-cli --raw eval "editor.setValue(`"int main(){}"`, -1)"

# 提交
npx --no-install playwright-cli click "#submitBtn"
Start-Sleep -Seconds 2  # 视用例而定；TLE 需 6~8s

# 断言结果
npx --no-install playwright-cli --raw eval "JSON.stringify({className: document.querySelector('.result-card')?.className, badge: document.querySelector('.result-card__badge')?.textContent})"
```

### 4.4 管理后台

```powershell
npx --no-install playwright-cli goto http://124.222.15.175:8080/admin.html
# 表单操作同 4.1 模板 A；Toast 用 eval 抓
npx --no-install playwright-cli --raw eval "document.querySelector('#toast')?.textContent"
```

### 4.5 网络/异常模拟

```powershell
# 拦截 /api/problems 模拟 5xx
npx --no-install playwright-cli route "**/api/problems" --status=503
# ... 执行被测操作 ...
npx --no-install playwright-cli unroute
```

---

## 五、TC-001 ~ TC-007 推荐执行顺序与一键脚本

> **执行节奏建议**：每条命令前 `Start-Sleep -Seconds 1`。下列脚本是"逐步式"以便观察；如需压时间可合并多步。

### TC-001 用户注册成功

```powershell
npx --no-install playwright-cli cookie-clear
npx --no-install playwright-cli sessionstorage-clear
npx --no-install playwright-cli goto http://124.222.15.175:8080/register.html
Start-Sleep -Seconds 1
$TS = [int][double]::Parse((Get-Date -UFormat %s))
$U = "testuser_$TS"
npx --no-install playwright-cli fill e20 $U
npx --no-install playwright-cli fill e22 "Test1234"
npx --no-install playwright-cli fill e30 "Test1234"
npx --no-install playwright-cli click e32
Start-Sleep -Seconds 1
# 断言
npx --no-install playwright-cli --raw eval "location.pathname"   # /login.html
npx --no-install playwright-cli requests | Select-String "/api/register" | Select-String "201"
```

### TC-002 注册失败-用户名已存在

```powershell
npx --no-install playwright-cli goto http://124.222.15.175:8080/register.html
Start-Sleep -Seconds 1
npx --no-install playwright-cli snapshot      # 拿新 ref
npx --no-install playwright-cli fill e20 "admin"
npx --no-install playwright-cli fill e22 "Test1234"
npx --no-install playwright-cli fill e30 "Test1234"
npx --no-install playwright-cli click e32
Start-Sleep -Seconds 1
npx --no-install playwright-cli --raw eval "document.querySelector('.field.is-error .field__error')?.textContent"   # "这个用户名已被占用"
npx --no-install playwright-cli requests | Select-String "/api/register" | Select-String "400"
```

### TC-003 ~ TC-005 表单校验

```powershell
# 通用前置：仍处于 /register.html
# TC-003: 用户名过短
npx --no-install playwright-cli fill e20 "ab"
npx --no-install playwright-cli fill e22 "Test1234"
npx --no-install playwright-cli fill e30 "Test1234"
npx --no-install playwright-cli click e32
npx --no-install playwright-cli --raw eval "[...document.querySelectorAll('.field__error')].map(e => e.textContent).filter(Boolean)"

# TC-004: 密码过短 → 改 e22 = "Aa1"
# TC-005: 两次密码不一致 → e30 = "Test5678"
```

> **断言技巧**：`[...document.querySelectorAll('.field__error')].map(e => e.textContent).filter(Boolean)` 一次性拿到所有错误文本。

### TC-006 密码强度（**已修正样例值**）

```powershell
npx --no-install playwright-cli goto http://124.222.15.175:8080/register.html
Start-Sleep -Seconds 1
$cases = @("a", "abc123", "Abc12345", "Abc!12345X")
foreach ($p in $cases) {
  npx --no-install playwright-cli fill e22 $p
  npx --no-install playwright-cli --raw eval "JSON.stringify({input: '$p', score: document.querySelector('#strength').dataset.score, label: document.querySelector('.strength__label').textContent})"
}
```

> 期望输出（顺序）：`{"input":"a","score":"0","label":""}`、`{"input":"abc123","score":"2","label":"一般"}`、`{"input":"Abc12345","score":"3","label":"良好"}`、`{"input":"Abc!12345X","score":"4","label":"很强"}`。

### TC-007 密码显示/隐藏

```powershell
npx --no-install playwright-cli goto http://124.222.15.175:8080/login.html
Start-Sleep -Seconds 1
npx --no-install playwright-cli fill e22 "Test1234"
npx --no-install playwright-cli click e23   # 显示密码
npx --no-install playwright-cli --raw eval "JSON.stringify({type: document.querySelector('input[name=password]').type, pressed: document.querySelector('[data-toggle=password]').getAttribute('aria-pressed')})"   # text / true
npx --no-install playwright-cli snapshot    # 重新拿 ref（按钮 label 已变）
npx --no-install playwright-cli click <new-ref>  # 隐藏密码
npx --no-install playwright-cli --raw eval "JSON.stringify({type: document.querySelector('input[name=password]').type, pressed: document.querySelector('[data-toggle=password]').getAttribute('aria-pressed')})"   # password / false
```

---

## 六、效率提升技巧

### 6.1 减少 snapshot 次数

- **同页操作**且 ref 不会失效时，复用首次 snapshot 的 ref。
- **动态变化**才重 snapshot（如按钮 aria-label 切换、Modal 出现）。

### 6.2 eval 替代 snapshot 读数

读文本/属性时优先：

```powershell
npx --no-install playwright-cli --raw eval "<one-line JS>"
```

而非：

```powershell
npx --no-install playwright-cli snapshot | Select-String "data-score"
```

### 6.3 一次 eval 多断言

```powershell
npx --no-install playwright-cli --raw eval "JSON.stringify({path: location.pathname, username: sessionStorage.getItem('oj_username'), role: sessionStorage.getItem('oj_role')})"
```

### 6.4 批量执行 TC-001 ~ TC-007（一键脚本）

```powershell
$BASE = "http://124.222.15.175:8080"
$delay = 1

function Step($cmd) {
  Start-Sleep -Seconds $delay
  npx --no-install playwright-cli $cmd
}

# TC-001
npx --no-install playwright-cli cookie-clear
npx --no-install playwright-cli sessionstorage-clear
Step "goto $BASE/register.html"
$TS = [int][double]::Parse((Get-Date -UFormat %s))
Step "fill e20 testuser_$TS"
Step 'fill e22 Test1234'
Step 'fill e30 Test1234'
Step 'click e32'
# 后续断言...
```

> **提示**：函数/脚本可保存为 `run-tc.ps1`，每次改 `$cases` 即可复用。

### 6.5 避免常见坑

| 坑 | 现象 | 解决 |
|----|------|------|
| `ref not found` | 点击后 DOM 变化导致 ref 失效 | `snapshot` 重新拿 |
| 表单 `fill` 没触发 input 事件 | 强度计/校验不更新 | 用 `--raw eval` 显式触发，或改用 `press` 逐字键入 |
| PowerShell 命令链断 | `&&` 不识别 | 用 `;` 或 `; if ($?) { ... }` |
| snapshot 输出截断 | 长页面 ref 列表不完整 | 改用 `snapshot <ref>` 分片读 |
| 浏览器缓存 | 看不到最新代码 | URL 加 `?v=N`（前端已用 `?v=7`） |

---

## 七、测试记录模板

每次回归建议在 `web自动化测试文档.md` 第五章表格追加：

| 日期 | 测试人员 | 浏览器/分辨率 | 总用例 | 通过 | 失败 | 阻塞 | 备注 |
|------|----------|----------------|--------|------|------|------|------|
| 2026-06-08 | opencode | Chrome/1920×1080 | 7 | 7 | 0 | 0 | TC-006 文档已按 ISS-001 修正 |

---

## 八、待办与建议

1. 把 `npx --no-install playwright-cli` 抽成 `$PWC` 变量，PowerShell 脚本里直接 `$PWC fill ...` 简化命令。
2. 后续用例涉及 Ace 编辑器时，**必须**用 `editor.setValue()`（见 4.3 模板）。
3. 后台管理类用例（TC-035 ~ TC-046）依赖 `globalSetup` 动态变量，详见 `web自动化测试文档.md` 1.3 节。
4. 越权类（TC-049/054/055）需要先注册普通用户，建议单建 `setup.ps1` 完成"注册 → 取 Easy 题 id → 建临时题"等动作。

> 文档完成时间：2026-06-08

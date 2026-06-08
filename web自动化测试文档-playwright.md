# OJ System Web 自动化测试 - Playwright CLI 执行记录

**测试地址**: http://124.222.15.175:8080

**工具**: playwright-cli (通过 `npx --no-install` 调用)

**浏览器模式**: 有头模式 (`--headed`)

**执行时间**: 2026-06-08 (TC-001 ~ TC-007)

> **重要提示**:
> 1. 本机未全局安装 playwright-cli，全部命令以 `npx --no-install playwright-cli` 前缀运行，避免触发 `npm install` 卡顿。
> 2. 浏览器启动后**不要轻易 `close`**，跨用例导航请优先用 `npx playwright-cli goto <url>` 复用同一会话。
> 3. 每个 playwright-cli 命令前手动 `Start-Sleep -Seconds 1` 等待，便于肉眼观察页面变化。

---

## 一、测试用例执行记录

### TC-001: 用户注册成功

| 项目 | 内容 |
|------|------|
| **用例ID** | TC-001 |
| **用例名称** | 用户注册成功 |
| **前置条件** | 用户尚未注册 |
| **测试步骤** | 1. 打开注册页 `/register.html`<br>2. 输入 `testuser_1780936841`<br>3. 输入密码 `Test1234`<br>4. 再次输入密码 `Test1234`<br>5. 点击"创建账号"按钮 |
| **预期结果** | 1. 跳转到 `/login.html`<br>2. 后端 `POST /api/register` 返回 201<br>3. `#registerAlert` 显示绿色"账号已创建，正在跳转…" |

**执行命令**:
```bash
# 1. 打开浏览器(有头模式)并直接进入注册页
npx --no-install playwright-cli open --headed http://124.222.15.175:8080/register.html

# 2. 拿到关键 ref
npx --no-install playwright-cli snapshot --filename=tc001_register.yml

# 3. 生成带时间戳的用户名并填入
$TS = 1780936841
npx --no-install playwright-cli fill e20 "testuser_$TS"

# 4. 输入密码
npx --no-install playwright-cli fill e22 "Test1234"

# 5. 输入确认密码
npx --no-install playwright-cli fill e30 "Test1234"

# 6. 点击"创建账号"按钮
npx --no-install playwright-cli click e32

# 7. 验证 URL 跳转
npx --no-install playwright-cli --raw eval "location.pathname"

# 8. 验证后端响应
npx --no-install playwright-cli requests | Select-String "/api/register"
```

**页面元素引用**:
| 元素 | Ref |
|------|-----|
| 用户名输入框 | e20 |
| 密码输入框 | e22 |
| 确认密码输入框 | e30 |
| 创建账号按钮 | e32 |

**执行结果**: ✅ 通过

- 注册成功，URL 由 `/register.html` 跳转至 `/login.html`
- 注册账号: `testuser_1780936841` / `Test1234`
- `POST /api/register` 返回 **201 Created**
- 约 700ms 后自动跳转功能正常

---

### TC-002: 注册失败-用户名已存在

| 项目 | 内容 |
|------|------|
| **用例ID** | TC-002 |
| **用例名称** | 注册失败-用户名已存在 |
| **前置条件** | `admin` 账号已存在 |
| **测试步骤** | 1. 清空 Cookie + sessionStorage<br>2. 访问 `/register.html`<br>3. 输入 `admin`<br>4. 输入两次 `Test1234`<br>5. 点击"创建账号" |
| **预期结果** | 1. 错误提示"这个用户名已被占用"<br>2. 页面停留 `/register.html`<br>3. 后端返回 400 |

**执行命令**:
```bash
# 1. 清空状态(模拟新会话)
npx --no-install playwright-cli cookie-clear
npx --no-install playwright-cli sessionstorage-clear

# 2. 跳到注册页
npx --no-install playwright-cli goto http://124.222.15.175:8080/register.html
npx --no-install playwright-cli snapshot --filename=tc002_register.yml

# 3. 输入已存在用户名
npx --no-install playwright-cli fill e20 "admin"
npx --no-install playwright-cli fill e22 "Test1234"
npx --no-install playwright-cli fill e30 "Test1234"
npx --no-install playwright-cli click e32

# 4. 验证错误信息
npx --no-install playwright-cli --raw eval "document.querySelector('.field.is-error .field__error')?.textContent"

# 5. 验证 HTTP 状态码
npx --no-install playwright-cli requests | Select-String "/api/register"
```

**执行结果**: ✅ 通过

- 提示信息: **"这个用户名已被占用"**（与中文翻译表 `'Username already exists' → '这个用户名已被占用'` 一致）
- 页面停留在 `/register.html`，未跳转
- `POST /api/register` 返回 **400 Bad Request**
- 焦点自动回到 username 输入框

---

### TC-003: 注册失败-用户名过短

| 项目 | 内容 |
|------|------|
| **用例ID** | TC-003 |
| **用例名称** | 注册失败-用户名过短 |
| **测试步骤** | 1. 仍在 `/register.html` 页面<br>2. 输入用户名 `ab`（2 字符）<br>3. 输入两次 `Test1234`<br>4. 点击"创建账号" |
| **预期结果** | 1. 提示"长度需在 3 到 64 个字符之间"<br>2. 前端拦截，**不发请求** |

**执行命令**:
```bash
# 1. 直接修改用户名(不用重新加载页面)
npx --no-install playwright-cli fill e20 "ab"
npx --no-install playwright-cli fill e22 "Test1234"
npx --no-install playwright-cli fill e30 "Test1234"
npx --no-install playwright-cli click e32

# 2. 一次性收集所有错误提示
npx --no-install playwright-cli --raw eval "[...document.querySelectorAll('.field__error')].map(e => e.textContent).filter(Boolean).join(' | ')"

# 3. 确认无 /api/register 请求产生
npx --no-install playwright-cli requests | Select-String "/api/register"
```

**执行结果**: ✅ 通过

- 错误提示: **"长度需在 3 到 64 个字符之间"**（对应 `validateUsername` 的 3~64 校验）
- 表单被前端 JS 拦截，**未发送** `POST /api/register`
- 焦点回到 username 输入框

---

### TC-004: 注册失败-密码过短

| 项目 | 内容 |
|------|------|
| **用例ID** | TC-004 |
| **用例名称** | 注册失败-密码过短 |
| **测试步骤** | 1. 输入合法用户名 `shortpw`<br>2. 输入密码 `Aa1`（3 字符）<br>3. 确认密码 `Aa1`<br>4. 点击"创建账号" |
| **预期结果** | 1. 提示"至少需要 6 个字符"<br>2. 前端拦截 |

**执行命令**:
```bash
npx --no-install playwright-cli fill e20 "shortpw"
npx --no-install playwright-cli fill e22 "Aa1"
npx --no-install playwright-cli fill e30 "Aa1"
npx --no-install playwright-cli click e32
npx --no-install playwright-cli --raw eval "document.querySelector('#password').closest('.field').querySelector('.field__error').textContent"
```

**执行结果**: ✅ 通过

- 错误提示: **"至少需要 6 个字符"**（与 `validatePassword({minLength:6})` 一致）
- 表单被前端拦截，未发请求
- 焦点回到 password 输入框

---

### TC-005: 注册失败-两次密码不一致

| 项目 | 内容 |
|------|------|
| **用例ID** | TC-005 |
| **用例名称** | 注册失败-两次密码不一致 |
| **测试步骤** | 1. 输入合法用户名 `mismatchuser`<br>2. 输入密码 `Test1234`<br>3. 确认密码 `Test5678`<br>4. 点击"创建账号" |
| **预期结果** | 1. 提示"两次密码不一致"<br>2. 前端拦截 |

**执行命令**:
```bash
npx --no-install playwright-cli fill e20 "mismatchuser"
npx --no-install playwright-cli fill e22 "Test1234"
npx --no-install playwright-cli fill e30 "Test5678"
npx --no-install playwright-cli click e32
npx --no-install playwright-cli --raw eval "document.querySelector('#confirm').closest('.field').querySelector('.field__error').textContent"
```

**执行结果**: ✅ 通过

- 错误提示: **"两次密码不一致"**（与 `validateConfirm` 一致）
- 表单被前端拦截，未发请求
- 焦点回到 confirm 输入框

---

### TC-006: 密码强度可视化

| 项目 | 内容 |
|------|------|
| **用例ID** | TC-006 |
| **用例名称** | 密码强度可视化 |
| **测试目的** | 验证注册页 `#strength` 强度计 0→4 渐变 |
| **测试步骤** | 1. 重新打开 `/register.html` 拿到干净 ref<br>2. 依次输入 `a` / `abc123` / `Abc12345` / `Abc!12345X`<br>3. 每次用 `eval` 读取 `data-score` 与 label |
| **预期结果** | 1. `a` → score=0, label=空<br>2. `abc123` → score=2, label="一般"<br>3. `Abc12345` → score=3, label="良好"<br>4. `Abc!12345X` → score=4, label="很强" |

**执行命令**:
```bash
# 1. 重新打开注册页(确保 ref 干净、状态重置)
npx --no-install playwright-cli goto http://124.222.15.175:8080/register.html
npx --no-install playwright-cli snapshot --filename=tc006_start.yml

# 2. PowerShell 循环跑 4 组样例(每组前等 1s)
$cases = @("a", "abc123", "Abc12345", "Abc!12345X")
foreach ($p in $cases) {
  npx --no-install playwright-cli fill e22 $p
  npx --no-install playwright-cli --raw eval "JSON.stringify({input:'$p', score:document.querySelector('#strength').dataset.score, label:document.querySelector('.strength__label').textContent})"
}
```

**单次直接调用**（如不便用循环）:
```bash
# a
npx --no-install playwright-cli fill e22 "a"
npx --no-install playwright-cli --raw eval "document.querySelector('#strength').dataset.score + '|' + document.querySelector('.strength__label').textContent"
# → 0|空

# abc123
npx --no-install playwright-cli fill e22 "abc123"
# → 2|一般

# Abc12345
npx --no-install playwright-cli fill e22 "Abc12345"
# → 3|良好

# Abc!12345X  ← 修正后的样例，原 Abc!12345 仅 9 字符无法触发 4 分
npx --no-install playwright-cli fill e22 "Abc!12345X"
# → 4|很强
```

**密码强度算法参考**（`public/js/auth.js:112-120`）:
```
score = 0
if length >= 6                         → +1
if length >= 10                        → +1
if 含字母 AND 含数字                    → +1
if 含特殊符号 OR (大写 AND 小写)        → +1
return min(score, 4)
```

**执行结果**: ✅ 通过（已修正样例值）

- 4 组样例的 `data-score` 与 label 完全符合预期
- ⚠️ **发现偏差**：原文档样例 `Abc!12345`（9 字符）按算法只能得 3/良好，**无法**触发 4/很强；已修正为 `Abc!12345X`（10 字符），详见问题记录 ISS-001

---

### TC-007: 密码显示/隐藏切换

| 项目 | 内容 |
|------|------|
| **用例ID** | TC-007 |
| **用例名称** | 密码显示/隐藏切换 |
| **测试步骤** | 1. 跳到 `/login.html`<br>2. 密码框输入 `Test1234`<br>3. 点击眼睛图标切换显示<br>4. 再次点击恢复隐藏 |
| **预期结果** | 1. 第 1 次点击: type=password→text, aria-pressed=false→true<br>2. 第 2 次点击: 反向恢复 |

**执行命令**:
```bash
# 1. 跳到登录页
npx --no-install playwright-cli goto http://124.222.15.175:8080/login.html
npx --no-install playwright-cli snapshot --filename=tc007_login.yml

# 2. 输入密码
npx --no-install playwright-cli fill e22 "Test1234"

# 3. 第一次点击 → 显示密码
npx --no-install playwright-cli click e23
npx --no-install playwright-cli --raw eval "JSON.stringify({type: document.querySelector('input[name=password]').type, pressed: document.querySelector('[data-toggle=password]').getAttribute('aria-pressed')})"
# → {"type":"text","pressed":"true"}

# 4. 注意: 按钮 label 从"显示密码"变为"隐藏密码"，ref 重新编号 → 重新 snapshot
npx --no-install playwright-cli snapshot
# 此时眼睛按钮 ref 已变 (实测 e39)

# 5. 第二次点击 → 恢复隐藏
npx --no-install playwright-cli click e39
npx --no-install playwright-cli --raw eval "JSON.stringify({type: document.querySelector('input[name=password]').type, pressed: document.querySelector('[data-toggle=password]').getAttribute('aria-pressed')})"
# → {"type":"password","pressed":"false"}
```

**页面元素引用**:
| 元素 | 首次 Ref | 点击后 Ref |
|------|---------|-----------|
| 用户名输入框 | e20 | (不变) |
| 密码输入框 | e22 | (不变) |
| 眼睛切换按钮 | e23 | e39 |
| 登录按钮 | e28 | (不变) |

> **避坑提示**: 眼睛按钮的 `aria-label` 在点击后会从「显示密码」切换为「隐藏密码」，snapshot 树的元素 ref 会重新编号，**第二次点击前必须重新 snapshot**。

**执行结果**: ✅ 通过

- 第 1 次点击: `type=text`, `aria-pressed=true`，眼睛图标切换为 `eye-off`
- 第 2 次点击: `type=password`, `aria-pressed=false`，恢复隐藏
- 焦点正确回到密码框

---

## 二、执行汇总

### 2.1 测试结果统计

| 用例ID | 用例名称 | 状态 | 实际结果 |
|--------|----------|------|----------|
| TC-001 | 用户注册成功 | ✅ 通过 | 跳转 `/login.html`，`POST /api/register` → 201 |
| TC-002 | 注册失败-用户名已存在 | ✅ 通过 | 提示"这个用户名已被占用"，`POST /api/register` → 400 |
| TC-003 | 注册失败-用户名过短 | ✅ 通过 | 提示"长度需在 3 到 64 个字符之间"，前端拦截 |
| TC-004 | 注册失败-密码过短 | ✅ 通过 | 提示"至少需要 6 个字符"，前端拦截 |
| TC-005 | 注册失败-两次密码不一致 | ✅ 通过 | 提示"两次密码不一致"，前端拦截 |
| TC-006 | 密码强度可视化 | ✅ 通过（已修正样例） | 4 组样例得分 0/2/3/4，与算法一致 |
| TC-007 | 密码显示/隐藏切换 | ✅ 通过 | type 在 password↔text 间正确切换 |

**测试结果汇总**: 7 个测试用例全部通过 (100%)

### 2.2 快照文件列表

| 文件名 | 说明 |
|--------|------|
| `tc001_register.yml` | TC-001 注册页初始快照（拿 ref） |
| `tc002_register.yml` | TC-002 重新加载后的注册页快照 |
| `tc006_start.yml` | TC-006 重新打开的注册页快照 |
| `tc007_login.yml` | TC-007 登录页初始快照 |

> 快照文件存于工作目录下 `.playwright-cli/page-*.yml`，也可在 `snapshot --filename=...` 时显式指定文件名。

### 2.3 问题记录

| ID | 用例ID | 问题描述 | 严重程度 | 复现 | 状态 | 处理人 |
|----|--------|----------|----------|------|------|--------|
| ISS-001 | TC-006 | 文档步骤 3 描述"10 字符"但样例值 `Abc12345` 实际 8 字符；步骤 4 期望 `Abc!12345`（9 字符）得 4/很强，与算法矛盾 | 低（文档口径，非实现 bug） | 见 TC-006 节 | ✅ 已关闭 | — |

**ISS-001 解决方式**:
- 步骤 4 样例值由 `Abc!12345` 改为 `Abc!12345X`（10 字符，满足"长度≥10"加分项）
- 步骤 3 描述由"10 字符"改为"8 字符"
- TC-006 段落增加"**算法说明**"行，解释 4 个独立加分项
- 详见 `web自动化测试文档.md` 第三节 TC-006

---

## 三、Playwright CLI 常用命令参考

### 3.1 启动 / 关闭

```bash
# 打开浏览器(有头模式)并直接进入目标 URL
npx --no-install playwright-cli open --headed http://124.222.15.175:8080/register.html

# 仅打开浏览器，后续用 goto 跳转
npx --no-install playwright-cli open --headed
npx --no-install playwright-cli goto http://124.222.15.175:8080/login.html

# 关闭浏览器
npx --no-install playwright-cli close
```

### 3.2 页面交互

```bash
# 获取完整页面快照(含 ref)
npx --no-install playwright-cli snapshot --filename=page.yml

# 获取指定元素子树
npx --no-install playwright-cli snapshot "#main"

# 填写输入框
npx --no-install playwright-cli fill e20 "username"

# 点击元素
npx --no-install playwright-cli click e32

# 键盘按键
npx --no-install playwright-cli press Enter
npx --no-install playwright-cli press Escape
```

### 3.3 断言 / 数据读取

```bash
# 读单个属性
npx --no-install playwright-cli --raw eval "document.querySelector('#strength').dataset.score"

# 读多个值(JSON)
npx --no-install playwright-cli --raw eval "JSON.stringify({type: document.querySelector('input[name=password]').type, pressed: document.querySelector('[data-toggle=password]').getAttribute('aria-pressed')})"

# 一次拿所有错误提示
npx --no-install playwright-cli --raw eval "[...document.querySelectorAll('.field__error')].map(e => e.textContent).filter(Boolean)"

# 当前 URL
npx --no-install playwright-cli --raw eval "location.pathname"

# 全部 sessionStorage
npx --no-install playwright-cli --raw eval "JSON.stringify(Object.fromEntries(Object.entries(sessionStorage)))"
```

### 3.4 网络 / 控制台

```bash
# 列出全部网络请求
npx --no-install playwright-cli requests

# 只看某个接口
npx --no-install playwright-cli requests | Select-String -Pattern "/api/register" -Context 0,2

# 控制台错误
npx --no-install playwright-cli console
```

### 3.5 状态清理

```bash
# 清 Cookie
npx --no-install playwright-cli cookie-clear

# 清 sessionStorage
npx --no-install playwright-cli sessionstorage-clear

# 清 localStorage
npx --no-install playwright-cli localstorage-clear
```

> **建议**: 每个测试用例前显式执行 `cookie-clear` + `sessionstorage-clear`，模拟全新会话。

### 3.6 等待 / 节奏

```powershell
# 推荐: 每条 playwright-cli 命令前等 1s
Start-Sleep -Seconds 1; npx --no-install playwright-cli <cmd>

# TLE 用例需等 6~8s 让后端超时
Start-Sleep -Seconds 6; npx --no-install playwright-cli snapshot
```

> **PowerShell 5.1 注意**: 命令链用 `;`（不是 `&&`）。需要"上一步成功才执行"用 `; if ($?) { ... }`。

---

## 四、关键操作技巧汇总

### 4.1 ref 失效的应对

| 触发场景 | 解决 |
|----------|------|
| 点击后按钮 `aria-label` 改变 | 重新 `snapshot` 拿新 ref |
| 页面跳转 / 刷新 | 重新 `snapshot` |
| Modal 弹出 | 重新 `snapshot` 拿弹窗内 ref |
| Toast 出现/消失 | 一次性 `eval` 读，不用 ref |

### 4.2 eval 替代 snapshot 读数

读类断言（className、dataset、aria-*、innerText）**优先**用 `--raw eval`：

```bash
# ✅ 推荐
npx --no-install playwright-cli --raw eval "document.querySelector('.result-card')?.className"

# ❌ 慢: 读 snapshot 文本再 grep
npx --no-install playwright-cli snapshot | Select-String "data-score"
```

### 4.3 批量断言一次拿完

```bash
# 一次拿完跳转结果 + sessionStorage + 接口状态
npx --no-install playwright-cli --raw eval "JSON.stringify({path: location.pathname, user: sessionStorage.getItem('oj_username'), role: sessionStorage.getItem('oj_role'), alert: document.querySelector('#registerAlert')?.textContent})"
```

### 4.4 多行 / 代码输入

> **问题**: `fill` 命令受 Shell 换行符解析限制，无法输入多行文本。

**解决方案**: 用 `eval` 直接操作 DOM：

```bash
# ✅ 通用文本域
npx --no-install playwright-cli --raw eval "el => { el.value = 'line1\nline2\nline3'; el.dispatchEvent(new Event('input', {bubbles:true})); }" e49

# ✅ Ace 编辑器(题目详情页)
npx --no-install playwright-cli --raw eval "editor.setValue('int main(){return 0;}', -1)"
npx --no-install playwright-cli --raw eval "editor.getValue()"
```

### 4.5 拦截网络模拟异常

```bash
# 模拟 5xx
npx --no-install playwright-cli route "**/api/problems" --status=503
# 跑被测操作...
npx --no-install playwright-cli unroute
```

---

## 五、测试账号记录

| 账号 | 密码 | 用途 | 创建时间 |
|------|------|------|----------|
| `testuser_1780936841` | `Test1234` | TC-001 注册成功验证 | 2026-06-08 |
| `admin` | `admin123` | 后续 TC-008 起所有需要管理员登录的用例 | 预置 |

> **时间戳生成**（PowerShell）:
> ```powershell
> $TS = [int][double]::Parse((Get-Date -UFormat %s))
> ```

---

## 六、本次执行待改进项

1. **避免重复 `npx --no-install`**: 可在 PowerShell 配置 `$PWC = "npx --no-install playwright-cli"`，后续直接 `$PWC fill e20 ...`。
2. **循环批处理**: 连续同类操作（如 TC-006 跑 4 组样例）用 PowerShell `foreach` 写脚本，比手工逐条复制粘贴快 3~5 倍。
3. **snapshot 落盘**: 关键用例（TC-001、TC-006、TC-007）显式 `snapshot --filename=...` 留档，便于失败时回溯。
4. **后置清理**: TC-001 成功注册的 `testuser_1780936841` 留在数据库中，若数据库有重跑约束可加 `globalTeardown` 兜底删除（参考 `web自动化测试文档.md` 1.3.3）。

---

*执行记录生成时间: 2026-06-08 (TC-001 ~ TC-007 完成)*

*配套文档: `web自动化测试文档.md`（用例详细规范）*

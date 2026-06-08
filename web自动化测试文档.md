# CPP·OJ Web 自动化测试文档

**测试目标地址**：`http://124.222.15.175:8080`
**管理员账户**：用户名 `admin` / 密码 `admin123`

---

## 一、测试环境与全局变量说明

### 1.1 测试环境

| 配置项 | 值 |
|--------|---|
| Base URL | `http://124.222.15.175:8080` |
| 推荐浏览器 | Chrome / Edge（Chromium 内核） |
| 推荐分辨率 | 1920 × 1080 |
| 管理员账户 | `admin` / `admin123`（角色 admin） |
| 后端语言/框架 | C++17 / cpp-httplib |
| 存储 | MySQL 8.0 (`oj_system`) |
| 评测沙盒 | `fork + exec`，超时上限 5s |

### 1.2 全局变量与选择器约定

| 名称 | 含义 / 取值 | 用途 |
|------|--------------|------|
| `BASE_URL` | `http://124.222.15.175:8080` | 所有相对路径前缀 |
| `ADMIN_USER` | `admin` | 管理员用户名 |
| `ADMIN_PASS` | `admin123` | 管理员密码 |
| `SESSION_COOKIE` | `oj_session` | 服务端下发的 Session Cookie，HttpOnly |
| `oj_username` (sessionStorage) | 当前登录用户名 | 客户端缓存的用户名 |
| `oj_role` (sessionStorage) | `admin` \| `user` | 客户端缓存的角色 |
| `oj_editor_code_{problemId}` (sessionStorage) | Ace 编辑器内容 | 每题独立的代码草稿 |
| Toast 选择器 | `#toast.is-visible` | 顶部居中 Toast 提示 |
| 结果卡片选择器 | `.result-card` | 题目详情页评测结果区 |
| 难度筛选 | `.filter__chip[data-difficulty="..."]` | `all` / `Easy` / `Medium` / `Hard` |

### 1.3 幂等性约定（确保用例任意顺序、任意次数重跑都通过）

> 本节为新增规范，目的是让**任意测试套件、任意执行顺序、任意重跑次数**下，所有用例都能稳定通过。任何在用例体内直接写入「admin」「id=1」「两数之和」等硬编码且会在数据库留下副作用的字段，都必须按本节约定改造为**动态变量**或**setup/teardown 钩子**。

#### 1.3.1 beforeEach 钩子（每个用例执行前必须执行）

```javascript
// 1. 清 Cookie：避免前序用例的 admin / 普通用户 session 残留
await context.clearCookies();

// 2. 清 sessionStorage：清空 oj_username / oj_role / oj_editor_code_{id} / 自定义测试用例等
await page.evaluate(() => sessionStorage.clear());

// 3. 注入当前秒级时间戳，供 {unix_timestamp} 占位符使用
const TS = Math.floor(Date.now() / 1000);
```

> **重要**：测试步骤中出现的 `{unix_timestamp}` **必须**由脚本运行时替换为真实的秒级时间戳，**禁止**作为字面量提交，否则第二次执行会因为用户名/标题冲突而失败。

#### 1.3.2 globalSetup 钩子（套件启动执行一次）

```javascript
// 1. 确保管理员可用（admin / admin123 已由 init.sql 创建）
await ensureAdmin(ADMIN_USER, ADMIN_PASS);

// 2. 注册并登录一个普通用户 testuser_{TS}（供 TC-049 / TC-054 / TC-055 使用）
const TEST_NORMAL_USER = `testuser_${Math.floor(Date.now() / 1000)}`;
await registerAndLogin(TEST_NORMAL_USER, 'Test1234');
process.env.TEST_NORMAL_USER = TEST_NORMAL_USER;

// 3. 动态获取首个 Easy 题目 id（供 TC-010 / TC-020~034 使用）
const problems = await fetchProblems();
let easyProblem = problems.find(p => p.difficulty === 'Easy');
if (!easyProblem) {
  // 题库中无 Easy 题时，种子化一道「两数之和」并重取
  const newId = await seedTwoSumProblem();
  easyProblem = (await fetchProblems()).find(p => p.id === newId);
}
process.env.TEST_PROBLEM_ID = String(easyProblem.id);

// 4. 取该题标题前 2 字作为搜索关键字（供 TC-018 使用）
process.env.TEST_DIFF_KEYWORD = easyProblem.title.slice(0, 2);

// 5. 创建一道临时题（供 TC-041~044 删除类用例使用）
const tempTitle = `delete_target_${Math.floor(Date.now() / 1000)}`;
const tempId = await adminCreateProblem({ title: tempTitle, difficulty: 'Easy', content: 'tmp' });
process.env.TEST_TEMP_PROBLEM_ID = String(tempId);
```

#### 1.3.3 globalTeardown 钩子（套件结束执行一次）

```javascript
// 1. 兜底删除 TC-036 / TC-040 / TC-046 期间动态创建的题目（标题带 unix_timestamp 后缀）
const allProblems = await fetchProblems();
for (const p of allProblems) {
  if (/_(1\d{9,})$/.test(p.title)) {
    await deleteProblem(p.id);
  }
}

// 2. 兜底删除 TC-041~044 测试期间创建的临时题（即使 confirm 测试中断也保证清理）
if (process.env.TEST_TEMP_PROBLEM_ID) {
  await deleteProblem(Number(process.env.TEST_TEMP_PROBLEM_ID));
}
```

#### 1.3.4 动态变量约定表

| 变量名 | 含义 | 注入时机 | 消费用例 |
|--------|------|----------|----------|
| `TEST_PROBLEM_ID` | 首个 Easy 题的 id | `globalSetup` | TC-010, TC-020~034 |
| `TEST_DIFF_KEYWORD` | 该 Easy 题标题前 2 字符 | `globalSetup` | TC-018 |
| `TEST_NORMAL_USER` | 普通用户名（带时间戳） | `globalSetup` | TC-049, TC-054, TC-055 |
| `TEST_TEMP_PROBLEM_ID` | 临时删除目标题 id | `globalSetup` | TC-041, TC-042, TC-043, TC-044 |
| `TS` | 当前秒级 unix 时间戳 | `beforeEach` | TC-001, TC-036, TC-040, TC-046 |

> **使用方式**：测试步骤中 `${TEST_PROBLEM_ID}`、`${TS}` 等占位符由 Playwright 的 `test.beforeEach` / `globalSetup` 通过 `process.env` 注入，运行时替换。

#### 1.3.5 题库可重置保护（TC-045 专属）

`TC-045 题库为空` **必须**作为**独立 suite** 运行，不得与其他 suite 混跑：

- **suite `beforeAll`**：备份当前 `problems` / `test_cases` 表，并 `TRUNCATE TABLE test_cases; TRUNCATE TABLE problems;`（**保留 `users` 表**，避免影响其他测试账号）。
- **suite `afterAll`**：从 `database/init.sql` 种子数据重新导入 `problems` / `test_cases`，还原测试环境。
- **用例体内保护**：
  ```javascript
  test('TC-045 题库为空', async () => {
    test.skip((await getProblemCount()) > 0, '题库非空，跳过；该用例须在独立 suite 跑');
    // ...
  });
  ```

---

## 二、测试模块规划

根据 `SPEC.md`、前端 HTML/JS 实现与后端 Handler 代码，提取出以下核心测试模块：

| 序号 | 模块 | 覆盖功能点 | 关联接口 |
|------|------|------------|----------|
| 1 | 认证模块 | 注册、登录、登出、密码强度、表单校验 | `POST /api/register` `POST /api/login` `POST /api/logout` `GET /api/me` |
| 2 | 题目模块 | 列表加载、难度筛选、关键字搜索、详情、404 | `GET /api/problems` `GET /api/problems/:id` |
| 3 | 代码运行与提交模块 | "运行测试"、自定义用例、提交、空代码、AC/WA/CE/TLE/RE、MLE | `POST /api/run` `POST /api/submit` |
| 4 | 管理后台模块 | 访问鉴权、创建题目、添加/删除测试用例、删除确认弹窗 | `POST /api/admin/problems` `DELETE /api/admin/problems/:id` |
| 5 | 权限与安全模块 | 未登录跳转、跨角色越权访问、角色标识 | 全部受保护接口 |
| 6 | 页面导航与交互 | 落地页、登录/注册跳转、返回列表 | `/index.html` 等静态资源 |

---

## 三、详细测试用例

### 3.1 认证模块

#### TC-001: 用户注册成功

| 项目 | 内容 |
|------|------|
| **用例ID** | TC-001 |
| **用例名称** | 用户注册成功 |
| **测试目的** | 验证合法注册的全链路流程 |
| **前置条件** | 准备一个未注册过的用户名（`testuser_{unix_timestamp}`） |
| **测试步骤** | 1. 访问 `/register.html`<br>2. 在 `#registerForm input[name="username"]` 输入 `testuser_{unix_timestamp}`<br>3. 在 `input[name="password"]` 输入 `Test1234`<br>4. 在 `input[name="confirm"]` 输入 `Test1234`<br>5. 点击 `#registerSubmit` |
| **预期结果** | 1. `#registerAlert` 显示绿色成功提示"账号已创建，正在跳转…"<br>2. 约 700ms 后 URL 变为 `/login.html`<br>3. 后端 `POST /api/register` 返回 201 |

#### TC-002: 注册失败-用户名已存在

| 项目 | 内容 |
|------|------|
| **用例ID** | TC-002 |
| **用例名称** | 注册失败-用户名已存在 |
| **测试目的** | 验证用户名唯一性校验 |
| **前置条件** | `admin` 账号已存在 |
| **测试步骤** | 1. 访问 `/register.html`<br>2. `input[name="username"]` 输入 `admin`<br>3. `input[name="password"]` 输入 `Test1234`<br>4. `input[name="confirm"]` 输入 `Test1234`<br>5. 点击 `#registerSubmit` |
| **预期结果** | 1. `username` 字段下 `.field__error` 提示"这个用户名已被占用"<br>2. `.field.is-error` 高亮该字段<br>3. 焦点停留在 username 输入框<br>4. 后端返回 400 |

#### TC-003: 注册失败-用户名过短

| 项目 | 内容 |
|------|------|
| **用例ID** | TC-003 |
| **用例名称** | 注册失败-用户名过短 |
| **测试目的** | 验证用户名长度下限校验 |
| **前置条件** | 无 |
| **测试步骤** | 1. 访问 `/register.html`<br>2. `input[name="username"]` 输入 `ab`（2 字符）<br>3. `input[name="password"]` 输入 `Test1234`<br>4. `input[name="confirm"]` 输入 `Test1234`<br>5. 点击 `#registerSubmit` |
| **预期结果** | 1. `username` 字段 `.field__error` 提示"长度需在 3 到 64 个字符之间"<br>2. 表单不提交，前端拦截 |

#### TC-004: 注册失败-密码过短

| 项目 | 内容 |
|------|------|
| **用例ID** | TC-004 |
| **用例名称** | 注册失败-密码过短 |
| **测试目的** | 验证密码长度下限校验 |
| **前置条件** | 无 |
| **测试步骤** | 1. 访问 `/register.html`<br>2. `input[name="username"]` 输入 `shortpw`<br>3. `input[name="password"]` 输入 `Aa1`（3 字符）<br>4. `input[name="confirm"]` 输入 `Aa1`<br>5. 点击 `#registerSubmit` |
| **预期结果** | 1. `password` 字段 `.field__error` 提示"至少需要 6 个字符"<br>2. 表单不提交 |

#### TC-005: 注册失败-两次密码不一致

| 项目 | 内容 |
|------|------|
| **用例ID** | TC-005 |
| **用例名称** | 注册失败-两次密码不一致 |
| **测试目的** | 验证确认密码逻辑 |
| **前置条件** | 无 |
| **测试步骤** | 1. 访问 `/register.html`<br>2. `input[name="username"]` 输入 `mismatchuser`<br>3. `input[name="password"]` 输入 `Test1234`<br>4. `input[name="confirm"]` 输入 `Test5678`<br>5. 点击 `#registerSubmit` |
| **预期结果** | 1. `confirm` 字段 `.field__error` 提示"两次密码不一致"<br>2. 表单不提交 |

#### TC-006: 密码强度可视化

| 项目 | 内容 |
|------|------|
| **用例ID** | TC-006 |
| **用例名称** | 密码强度可视化 |
| **测试目的** | 验证注册页 `#strength` 强度计 0→4 渐变 |
| **前置条件** | 无 |
| **测试步骤** | 1. 访问 `/register.html`<br>2. 依次输入 `a` / `abc123` / `Abc12345` / `Abc!12345X` 到 `input[name="password"]`<br>3. 每次观察 `#strength[data-score]` 与 `.strength__label` 文本 |
| **预期结果** | 1. 输入 1 字符：`data-score="0"`，label 为空<br>2. 输入 6 字符纯字母数字：`data-score="2"`，label "一般"<br>3. 输入 8 字符混合大小写字母与数字：`data-score="3"`，label "良好"<br>4. 输入 10 字符且含特殊符号与大小写字母与数字：`data-score="4"`，label "很强" |
| **算法说明** | 当前 `scorePassword()` 共有 4 个加分项且独立累计（长度≥6 / 长度≥10 / 字母+数字 / 特殊符号或大小写混合），满分 4。要达到 4 分必须**同时满足**：长度 ≥ 10、含字母与数字、含特殊符号或大小写混合。例如 `Abc!12345X`（10 字符，含 `!`、大写 `A/X`、小写 `b/c`、数字）。 |

#### TC-007: 密码显示/隐藏切换

| 项目 | 内容 |
|------|------|
| **用例ID** | TC-007 |
| **用例名称** | 密码显示/隐藏切换 |
| **测试目的** | 验证眼睛图标切换密码可见性 |
| **前置条件** | 无 |
| **测试步骤** | 1. 访问 `/login.html`<br>2. 在 `input[name="password"]` 输入 `Test1234`<br>3. 点击 `[data-toggle="password"]`<br>4. 再次点击切换 |
| **预期结果** | 1. 第一次点击：input.type 变为 `text`，眼睛图标切换为"eye-off"<br>2. 按钮 `aria-pressed="true"`<br>3. 第二次点击：恢复 `password` 隐藏，`aria-pressed="false"` |

#### TC-008: 管理员登录成功

| 项目 | 内容 |
|------|------|
| **用例ID** | TC-008 |
| **用例名称** | 管理员登录成功 |
| **测试目的** | 验证合法登录与跳转、Cookie 与 sessionStorage 写入 |
| **前置条件** | 无 |
| **测试步骤** | 1. 访问 `/login.html`<br>2. `input[name="username"]` 输入 `admin`<br>3. `input[name="password"]` 输入 `admin123`<br>4. 点击 `#loginSubmit` |
| **预期结果** | 1. `#loginAlert` 显示绿色"欢迎回来，正在跳转…"<br>2. 约 600ms 后跳转 `/problem_list.html`<br>3. 浏览器收到 `Set-Cookie: oj_session=...; HttpOnly`<br>4. `sessionStorage.oj_username="admin"`<br>5. `sessionStorage.oj_role="admin"` |

#### TC-009: 登录失败-密码错误

| 项目 | 内容 |
|------|------|
| **用例ID** | TC-009 |
| **用例名称** | 登录失败-密码错误 |
| **测试目的** | 验证错误凭据处理 |
| **前置条件** | 无 |
| **测试步骤** | 1. 访问 `/login.html`<br>2. `input[name="username"]` 输入 `admin`<br>3. `input[name="password"]` 输入 `wrongpassword`<br>4. 点击 `#loginSubmit` |
| **预期结果** | 1. `#loginAlert` 红色错误提示"用户名或密码错误"<br>2. URL 仍为 `/login.html`<br>3. 未设置 `oj_session` Cookie<br>4. 后端返回 401 |

#### TC-010: 登录带 return 参数回到原页

| 项目 | 内容 |
|------|------|
| **用例ID** | TC-010 |
| **用例名称** | 登录带 return 参数回到原页 |
| **测试目的** | 验证深链场景下登录后回跳 |
| **前置条件** | `globalSetup` 已将首个 Easy 题 id 写入 `${TEST_PROBLEM_ID}` |
| **测试步骤** | 1. 未登录状态访问 `/problem.html?id=${TEST_PROBLEM_ID}`<br>2. 页面跳转到 `/login.html?return=%2Fproblem.html%3Fid%3D${TEST_PROBLEM_ID}`<br>3. 输入 `admin` / `admin123` 点击登录 |
| **预期结果** | 1. 登录成功后跳回 `/problem.html?id=${TEST_PROBLEM_ID}`<br>2. `sessionStorage.oj_username="admin"` |

#### TC-011: 已登录访问登录页自动跳过

| 项目 | 内容 |
|------|------|
| **用例ID** | TC-011 |
| **用例名称** | 已登录访问登录页自动跳过 |
| **测试目的** | 验证重入登录页时的体验优化 |
| **前置条件** | `admin` 已登录，Session Cookie 有效 |
| **测试步骤** | 1. 在 tab A 登录后访问 `/problem_list.html`<br>2. 复制该 URL，在新 tab B 直接访问 `/login.html` |
| **预期结果** | `/login.html` 通过 `/api/me` 验证后自动 `location.replace` 跳到 `/problem_list.html`，不显示登录表单 |

#### TC-012: 用户登出（题目列表页）

| 项目 | 内容 |
|------|------|
| **用例ID** | TC-012 |
| **用例名称** | 用户登出（题目列表页） |
| **测试目的** | 验证登出后 Cookie 清理、sessionStorage 清理、跳转 |
| **前置条件** | `admin` 已登录 |
| **测试步骤** | 1. 访问 `/problem_list.html`<br>2. 点击 `#userMenu [data-action="logout"]`<br>3. 等待 Toast 与跳转 |
| **预期结果** | 1. 显示 Toast "已退出登录"<br>2. 响应头 `Set-Cookie: oj_session=; Max-Age=0`<br>3. `sessionStorage.oj_username` / `oj_role` 被清除<br>4. 约 700ms 后跳转到 `/login.html` |

---

### 3.2 题目模块

#### TC-013: 题目列表加载

| 项目 | 内容 |
|------|------|
| **用例ID** | TC-013 |
| **用例名称** | 题目列表加载 |
| **测试目的** | 验证列表渲染与难度计数 |
| **前置条件** | `admin` 已登录，题库非空 |
| **测试步骤** | 1. 访问 `/problem_list.html`<br>2. 等待 `#tableWrapper` 不再 `aria-busy="true"`<br>3. 断言 `.problem-table__row` 至少 1 行<br>4. 检查 `.filter__chip-count` 数字 |
| **预期结果** | 1. 渲染题目表格，列：题号 / 标题 / 难度 / 箭头<br>2. 四个难度 chip 右侧显示对应题目数量（合计 = 全部计数）<br>3. 每行 `.difficulty` 显示难度条 + 中文标签 |

#### TC-014: 难度筛选-简单

| 项目 | 内容 |
|------|------|
| **用例ID** | TC-014 |
| **用例名称** | 难度筛选-简单 |
| **测试目的** | 验证 Easy 难度筛选 |
| **前置条件** | `admin` 已登录，题库中至少存在 Easy 与其他难度 |
| **测试步骤** | 1. 访问 `/problem_list.html`<br>2. 点击 `.filter__chip[data-difficulty="Easy"]`<br>3. 等待列表刷新 |
| **预期结果** | 1. Easy chip 拥有 `is-active` 与 `aria-selected="true"`<br>2. 所有可见行 `.difficulty[data-difficulty="Easy"]`<br>3. 列表行数 == "简单"计数 |

#### TC-015: 难度筛选-中等

| 项目 | 内容 |
|------|------|
| **用例ID** | TC-015 |
| **用例名称** | 难度筛选-中等 |
| **测试目的** | 验证 Medium 难度筛选 |
| **前置条件** | 同 TC-014 |
| **测试步骤** | 1. 点击 `.filter__chip[data-difficulty="Medium"]` |
| **预期结果** | 仅显示 Medium 题目；chip 切换为 `is-active` |

#### TC-016: 难度筛选-困难

| 项目 | 内容 |
|------|------|
| **用例ID** | TC-016 |
| **用例名称** | 难度筛选-困难 |
| **测试目的** | 验证 Hard 难度筛选 |
| **前置条件** | 同 TC-014 |
| **测试步骤** | 1. 点击 `.filter__chip[data-difficulty="Hard"]` |
| **预期结果** | 仅显示 Hard 题目；chip 切换为 `is-active` |

#### TC-017: 难度筛选-重置为全部

| 项目 | 内容 |
|------|------|
| **用例ID** | TC-017 |
| **用例名称** | 难度筛选-重置为全部 |
| **测试目的** | 验证从筛选态回到全集 |
| **前置条件** | 当前已选 Medium 筛选 |
| **测试步骤** | 1. 点击 `.filter__chip[data-difficulty="all"]` |
| **预期结果** | "全部" chip 重新 `is-active`，列表恢复全集 |

#### TC-018: 按标题关键字搜索

| 项目 | 内容 |
|------|------|
| **用例ID** | TC-018 |
| **用例名称** | 按标题关键字搜索 |
| **测试目的** | 验证搜索防抖与匹配规则 |
| **前置条件** | `globalSetup` 已将该 Easy 题标题前 2 字符写入 `${TEST_DIFF_KEYWORD}` |
| **测试步骤** | 1. 访问 `/problem_list.html`<br>2. 在 `#searchInput` 输入 `${TEST_DIFF_KEYWORD}`<br>3. 等待 200ms（防抖 80ms） |
| **预期结果** | 1. 列表只显示标题包含该关键字的题目<br>2. 搜索对大小写不敏感<br>3. 不影响当前难度筛选 |

#### TC-019: 搜索无匹配结果

| 项目 | 内容 |
|------|------|
| **用例ID** | TC-019 |
| **用例名称** | 搜索无匹配结果 |
| **测试目的** | 验证搜索空态与重置 |
| **前置条件** | `admin` 已登录 |
| **测试步骤** | 1. 在 `#searchInput` 输入 `__no_such_problem__`<br>2. 观察空态按钮<br>3. 点击"查看全部"按钮 |
| **预期结果** | 1. 显示空态 `.empty-state` 包含"没有匹配的题目"<br>2. 点击"查看全部"后搜索框清空、难度回到 all、列表恢复全集 |

#### TC-020: 查看题目详情

| 项目 | 内容 |
|------|------|
| **用例ID** | TC-020 |
| **用例名称** | 查看题目详情 |
| **测试目的** | 验证详情页元素完整渲染 |
| **前置条件** | `admin` 已登录；`globalSetup` 已将首个 Easy 题 id 写入 `${TEST_PROBLEM_ID}` |
| **测试步骤** | 1. 在题目列表点击第一行（或直接访问 `/problem.html?id=${TEST_PROBLEM_ID}`）<br>2. 等待 `#problemContent[aria-busy="false"]` |
| **预期结果** | 1. URL 变为 `/problem.html?id=${TEST_PROBLEM_ID}`<br>2. `#problemTitle` 显示题目标题<br>3. `#problemEyebrow` 显示"题库 · 题目 #${TEST_PROBLEM_ID}"<br>4. `#problemMeta` 显示难度条<br>5. `#editor` 内出现 Ace 编辑器（`.ace_text-input` 存在）<br>6. `#testCaseList` 至少存在一个 `.test-case-row--readonly`（官方用例） |

#### TC-021: 题目详情页 404

| 项目 | 内容 |
|------|------|
| **用例ID** | TC-021 |
| **用例名称** | 题目详情页 404 |
| **测试目的** | 验证不存在的题目 ID 渲染 |
| **前置条件** | `admin` 已登录 |
| **测试步骤** | 1. 访问 `/problem.html?id=99999` |
| **预期结果** | 1. 页面渲染 `.empty-state` 包含"题目不存在"<br>2. 含"返回列表"按钮，点击后跳转 `/problem_list.html`<br>3. 后端 `/api/problems/99999` 返回 404 |

#### TC-022: 题目详情页缺 ID

| 项目 | 内容 |
|------|------|
| **用例ID** | TC-022 |
| **用例名称** | 题目详情页缺 ID |
| **测试目的** | 验证 URL 缺省参数处理 |
| **前置条件** | `admin` 已登录 |
| **测试步骤** | 1. 访问 `/problem.html`（无 `?id=` 参数） |
| **预期结果** | 显示 `.empty-state` 包含"题目不存在"与副标题"链接缺少题号" |

---

### 3.3 代码运行与提交模块

> **前置约定**：TC-023 ~ TC-034 全部基于"`admin` 已登录"前置；测试题目使用 `globalSetup` 动态获取的首个 Easy 题，id 引用 `${TEST_PROBLEM_ID}`（非硬编码）。若题库为空，`globalSetup` 会自动种子化一道「两数之和」题，无需依赖 TC-036 提前创建。

#### TC-023: 提交代码-答案正确 (AC)

| 项目 | 内容 |
|------|------|
| **用例ID** | TC-023 |
| **用例名称** | 提交代码-答案正确 (AC) |
| **测试目的** | 验证 AC 全流程 |
| **前置条件** | `admin` 已登录，进入 `id=${TEST_PROBLEM_ID}` 详情页 |
| **测试步骤** | 1. 等待编辑器加载完成<br>2. 在 Ace 编辑器中输入：`#include <iostream>\nusing namespace std;\nint main(){ int a,b; cin>>a>>b; cout<<a+b<<endl; return 0; }`（直接 `editor.setValue(code, -1)`）<br>3. 点击 `#submitBtn`<br>4. 等待 `#resultArea` 不再 pending |
| **预期结果** | 1. 提交按钮显示 `is-loading` 并禁用<br>2. 结果区出现 `.result-card--ac`<br>3. `.result-card__badge` 文本为 `AC`<br>4. `.result-card__label` 文本为"通过"<br>5. 显示 `executionTimeMs`（数值 ≥ 0） |

#### TC-024: 提交代码-答案错误 (WA)

| 项目 | 内容 |
|------|------|
| **用例ID** | TC-024 |
| **用例名称** | 提交代码-答案错误 (WA) |
| **测试目的** | 验证 WA 判题 |
| **前置条件** | `admin` 已登录，进入 `id=${TEST_PROBLEM_ID}` 详情页 |
| **测试步骤** | 1. 编辑器输入 `cout << a - b << endl;`（输出差值）<br>2. 点击 `#submitBtn` |
| **预期结果** | 1. `.result-card--wa`<br>2. 顶部 badge 显示 `WA`<br>3. 副标题"答案错误，输出与预期不符" |

#### TC-025: 提交代码-编译错误 (CE)

| 项目 | 内容 |
|------|------|
| **用例ID** | TC-025 |
| **用例名称** | 提交代码-编译错误 (CE) |
| **测试目的** | 验证 CE 判题 |
| **前置条件** | `admin` 已登录，进入 `id=${TEST_PROBLEM_ID}` 详情页 |
| **测试步骤** | 1. 编辑器输入 `int main(){ cout << "x" }`（缺分号）<br>2. 点击 `#submitBtn` |
| **预期结果** | 1. `.result-card--ce`<br>2. 顶部 badge 显示 `CE`<br>3. 编译信息区出现 `.result-card__output`，文本包含 `error:` 或 `expected ';' before` |

#### TC-026: 提交代码-运行超时 (TLE)

| 项目 | 内容 |
|------|------|
| **用例ID** | TC-026 |
| **用例名称** | 提交代码-运行超时 (TLE) |
| **测试目的** | 验证 5s 超时机制 |
| **前置条件** | `admin` 已登录，进入 `id=${TEST_PROBLEM_ID}` 详情页 |
| **测试步骤** | 1. 编辑器输入 `int main(){ while(true){} return 0; }`<br>2. 点击 `#submitBtn`<br>3. 等待 ≤ 8s |
| **预期结果** | 1. 约 5s 内返回<br>2. `.result-card--tle`<br>3. badge 显示 `TLE`<br>4. 副标题"运行超时" |

#### TC-027: 提交代码-运行错误 (RE)

| 项目 | 内容 |
|------|------|
| **用例ID** | TC-027 |
| **用例名称** | 提交代码-运行错误 (RE) |
| **测试目的** | 验证 RE 判题 |
| **前置条件** | `admin` 已登录，进入 `id=${TEST_PROBLEM_ID}` 详情页 |
| **测试步骤** | 1. 编辑器输入 `int* p=nullptr; *p=1;`（段错误）<br>2. 点击 `#submitBtn` |
| **预期结果** | 1. `.result-card--re`<br>2. badge 显示 `RE`<br>3. 副标题"运行错误" |

#### TC-028: 提交代码-空代码

| 项目 | 内容 |
|------|------|
| **用例ID** | TC-028 |
| **用例名称** | 提交代码-空代码 |
| **测试目的** | 验证空代码前端拦截 |
| **前置条件** | `admin` 已登录，进入 `id=${TEST_PROBLEM_ID}` 详情页 |
| **测试步骤** | 1. 点击 `#resetBtn` 清空编辑器<br>2. 在编辑器输入 `"   "`（仅空白）<br>3. 点击 `#submitBtn` |
| **预期结果** | 1. 不发请求<br>2. `.result-card--network` 出现<br>3. 副标题"代码不能为空" |

#### TC-029: 运行测试-官方用例

| 项目 | 内容 |
|------|------|
| **用例ID** | TC-029 |
| **用例名称** | 运行测试-官方用例 |
| **测试目的** | 验证 `/api/run` 流程与逐用例结果展示 |
| **前置条件** | `admin` 已登录；`beforeEach` 已清空 sessionStorage，进入 `id=${TEST_PROBLEM_ID}` 详情页（无残留自定义用例） |
| **测试步骤** | 1. 编辑器输入 AC 答案<br>2. 点击 `#runTestBtn`<br>3. 等待结果 |
| **预期结果** | 1. 按钮显示 `is-loading` 并禁用<br>2. `.result-card--test` 与 `.result-card--ac` 同时存在<br>3. 顶部 badge 显示 `N/N` 通过计数<br>4. 列表中每个 `.test-case` 含 `test-case--ac`，`source` 标记"官方"<br>5. 每个用例显示 `executionTimeMs` |

#### TC-030: 运行测试-添加自定义用例

| 项目 | 内容 |
|------|------|
| **用例ID** | TC-030 |
| **用例名称** | 运行测试-添加自定义用例 |
| **测试目的** | 验证 LeetCode 风格自定义用例 |
| **前置条件** | `admin` 已登录；`beforeEach` 已清空 sessionStorage，进入 `id=${TEST_PROBLEM_ID}` 详情页 |
| **测试步骤** | 1. 点击 `#addTestCaseBtn`<br>2. 在最后一个 `.test-case-row[data-source="custom"]` 的 `textarea[data-field="input"]` 输入 `42 58`<br>3. 再次点击 `#runTestBtn` |
| **预期结果** | 1. 列表增加一行 `test-case-row--custom`<br>2. 运行后结果区出现自定义用例（`source: 自定义`），无"预期"行<br>3. 顶部 badge 不计入自定义用例的 pass/fail<br>4. 副标题"全部 N 个官方用例通过，另运行 M 个自定义用例" |

#### TC-031: 重置代码按钮

| 项目 | 内容 |
|------|------|
| **用例ID** | TC-031 |
| **用例名称** | 重置代码按钮 |
| **测试目的** | 验证重置逻辑 |
| **前置条件** | `admin` 已登录；`beforeEach` 已清空 sessionStorage；进入 `id=${TEST_PROBLEM_ID}` 详情页；编辑器已被修改 |
| **测试步骤** | 1. 修改编辑器内容为 `garbage`<br>2. 点击 `#resetBtn` |
| **预期结果** | 1. 编辑器内容恢复为初始模板（或服务端返回的 `template`）<br>2. `#resultArea` 清空<br>3. 焦点回到编辑器<br>4. `sessionStorage.oj_editor_code_{id}` 被删除 |

#### TC-032: 代码编辑后重新加载页面

| 项目 | 内容 |
|------|------|
| **用例ID** | TC-032 |
| **用例名称** | 代码编辑后重新加载页面 |
| **测试目的** | 验证 sessionStorage 持久化 |
| **前置条件** | `beforeEach` 已清空 sessionStorage；进入 `id=${TEST_PROBLEM_ID}` 详情页 |
| **测试步骤** | 1. 在编辑器中输入 `MY_CUSTOM_CODE`<br>2. 等待 ≥ 200ms（自动保存）<br>3. `page.reload()` |
| **预期结果** | 重新加载后编辑器内仍是 `MY_CUSTOM_CODE`，而非默认模板 |

#### TC-033: 自定义用例可删除

| 项目 | 内容 |
|------|------|
| **用例ID** | TC-033 |
| **用例名称** | 自定义用例可删除 |
| **测试目的** | 验证删除交互 |
| **前置条件** | `admin` 已登录；`beforeEach` 已清空 sessionStorage；进入 `id=${TEST_PROBLEM_ID}` 详情页 |
| **测试步骤** | 1. 点击 `#addTestCaseBtn` 三次<br>2. 在第二个自定义行点击 `.test-case-row__remove`<br>3. 验证该行消失 |
| **预期结果** | 1. 该自定义用例行被删除，剩余行重新编号<br>2. 官方用例行无删除按钮 |

#### TC-034: 未登录提交被拦截

| 项目 | 内容 |
|------|------|
| **用例ID** | TC-034 |
| **用例名称** | 未登录提交被拦截 |
| **测试目的** | 验证提交鉴权 |
| **前置条件** | 普通用户但 `sessionStorage.oj_username` 已清空（服务端 Session 仍有效） |
| **测试步骤** | 1. 清除 `sessionStorage.oj_username`<br>2. 进入 `/problem.html?id=${TEST_PROBLEM_ID}`<br>3. 点击 `#submitBtn` |
| **预期结果** | 1. 跳转到 `/login.html?return=%2Fproblem.html%3Fid%3D${TEST_PROBLEM_ID}`<br>2. 登录后回到原题 |

---

### 3.4 管理后台模块

#### TC-035: 管理员访问管理后台

| 项目 | 内容 |
|------|------|
| **用例ID** | TC-035 |
| **用例名称** | 管理员访问管理后台 |
| **测试目的** | 验证后台入口与渲染 |
| **前置条件** | `admin` 已登录 |
| **测试步骤** | 1. 访问 `/problem_list.html`<br>2. 点击 `#userMenu .user-menu__link[href="/admin.html"]`<br>3. 进入 `/admin.html` |
| **预期结果** | 1. URL 变为 `/admin.html`<br>2. 显示 `#newProblemForm` 表单<br>3. 右侧 `#tableWrapper` 渲染题目表格<br>4. 每行含 `.problem-table__delete` 按钮<br>5. 顶部导航 `管理` chip `is-active` |

#### TC-036: 创建新题目

| 项目 | 内容 |
|------|------|
| **用例ID** | TC-036 |
| **用例名称** | 创建新题目 |
| **测试目的** | 验证完整创建流程 |
| **前置条件** | `admin` 已登录 |
| **测试步骤** | 1. 访问 `/admin.html`<br>2. `input[name="title"]` 输入 `两数之和_${TS}`（`${TS}` 由 `beforeEach` 注入秒级时间戳）<br>3. `input[name="difficulty"][value="Easy"]` 选中（默认）<br>4. `textarea[name="content"]` 输入 `给定两个整数 a 和 b，输出它们的和。`<br>5. `textarea[name="template"]` 输入 `int main(){ int a,b; cin>>a>>b; cout<<a+b<<endl; return 0; }`<br>6. 用例 1：input=`1 2`、expected=`3`；用例 2：input=`100 200`、expected=`300`<br>7. 点击 `#submitNewBtn` |
| **预期结果** | 1. 按钮进入 `is-loading` 并禁用<br>2. 出现 Toast "题目已添加"<br>3. 右侧题目列表自动刷新并出现新题（按 id 升序）<br>4. `#problemCount` 计数 +1<br>5. 表单被重置，测试用例恢复为默认两行<br>6. 后端 `POST /api/admin/problems` 返回 201 |

#### TC-037: 创建题目-缺少标题

| 项目 | 内容 |
|------|------|
| **用例ID** | TC-037 |
| **用例名称** | 创建题目-缺少标题 |
| **测试目的** | 验证必填校验 |
| **前置条件** | `admin` 已登录 |
| **测试步骤** | 1. 进入 `/admin.html`<br>2. 不填 `input[name="title"]`<br>3. 其余字段正常填写<br>4. 点击 `#submitNewBtn` |
| **预期结果** | 1. 出现 Toast "请填写标题"<br>2. 表单不提交，不发请求 |

#### TC-038: 创建题目-缺少描述

| 项目 | 内容 |
|------|------|
| **用例ID** | TC-038 |
| **用例名称** | 创建题目-缺少描述 |
| **测试目的** | 验证描述必填 |
| **前置条件** | `admin` 已登录 |
| **测试步骤** | 1. 填标题，不填 `textarea[name="content"]`<br>2. 点击 `#submitNewBtn` |
| **预期结果** | 出现 Toast "请填写题目描述" |

#### TC-039: 创建题目-添加多个测试用例

| 项目 | 内容 |
|------|------|
| **用例ID** | TC-039 |
| **用例名称** | 创建题目-添加多个测试用例 |
| **测试目的** | 验证动态增删 |
| **前置条件** | `admin` 已登录 |
| **测试步骤** | 1. 点击 `#addCaseBtn` 3 次<br>2. 断言共 5 行 `.form__test-case`（默认 2 + 新增 3）<br>3. 在第一行点击 `[data-remove]`<br>4. 断言共 4 行且编号连续 |
| **预期结果** | 1. 测试用例按 `[data-index]` 渲染<br>2. 删除后行数减 1 并重新编号<br>3. 仅剩 1 行时 `[data-remove]` 不可点 |

#### TC-040: 创建题目-空测试用例跳过

| 项目 | 内容 |
|------|------|
| **用例ID** | TC-040 |
| **用例名称** | 创建题目-空测试用例跳过 |
| **测试目的** | 验证容错 |
| **前置条件** | `admin` 已登录 |
| **测试步骤** | 1. `input[name="title"]` 输入 `empty_cases_${TS}`（`${TS}` 由 `beforeEach` 注入秒级时间戳，避免重跑冲突）<br>2. 填好描述/难度<br>3. 测试用例全部留空<br>4. 点击 `#submitNewBtn` |
| **预期结果** | 题目仍可创建成功（请求中不携带 `testCases` 数组），Toast "题目已添加" |

#### TC-041: 删除题目-确认

| 项目 | 内容 |
|------|------|
| **用例ID** | TC-041 |
| **用例名称** | 删除题目-确认 |
| **测试目的** | 验证删除全链路 |
| **前置条件** | `admin` 已登录；`globalSetup` 已创建临时题 `delete_target_${TS}`，其 id 写入 `${TEST_TEMP_PROBLEM_ID}`；`globalTeardown` 兜底再删一次 |
| **测试步骤** | 1. 进入 `/admin.html`<br>2. 点击 `id=${TEST_TEMP_PROBLEM_ID}` 行的 `.problem-table__delete`<br>3. `#deleteModal` 弹出后点击 `#confirmDeleteBtn` |
| **预期结果** | 1. Modal 出现 `#deleteModalBody` 文案"将永久删除题目 #${TEST_TEMP_PROBLEM_ID}「delete_target_${TS}」"<br>2. 焦点默认在 `#cancelDeleteBtn`<br>3. 确认后出现 Toast "正在删除…"，再出现"已删除「delete_target_${TS}」"<br>4. 列表行数 -1，`#problemCount` 同步更新<br>5. 后端 `DELETE /api/admin/problems/${TEST_TEMP_PROBLEM_ID}` 返回 200 |

#### TC-042: 删除题目-取消

| 项目 | 内容 |
|------|------|
| **用例ID** | TC-042 |
| **用例名称** | 删除题目-取消 |
| **测试目的** | 验证取消按钮 |
| **前置条件** | `admin` 已登录；`globalSetup` 已创建临时题，其 id 写入 `${TEST_TEMP_PROBLEM_ID}`；`globalTeardown` 兜底删除 |
| **测试步骤** | 1. 在 `id=${TEST_TEMP_PROBLEM_ID}` 行触发删除按钮，弹出 Modal<br>2. 点击 `#cancelDeleteBtn` |
| **预期结果** | 1. Modal `hidden=true`<br>2. 列表无变化<br>3. 无后端 DELETE 请求 |

#### TC-043: 删除题目-Esc 关闭弹窗

| 项目 | 内容 |
|------|------|
| **用例ID** | TC-043 |
| **用例名称** | 删除题目-Esc 关闭弹窗 |
| **测试目的** | 验证键盘交互 |
| **前置条件** | `admin` 已登录；`globalSetup` 已创建临时题，其 id 写入 `${TEST_TEMP_PROBLEM_ID}`；`globalTeardown` 兜底删除 |
| **测试步骤** | 1. 在 `id=${TEST_TEMP_PROBLEM_ID}` 行触发删除按钮，弹出 Modal<br>2. 按 `Escape` 键 |
| **预期结果** | Modal 关闭，列表无变化，无后端 DELETE 请求 |

#### TC-044: 删除题目-点击背景关闭

| 项目 | 内容 |
|------|------|
| **用例ID** | TC-044 |
| **用例名称** | 删除题目-点击背景关闭 |
| **测试目的** | 验证背景交互 |
| **前置条件** | `admin` 已登录；`globalSetup` 已创建临时题，其 id 写入 `${TEST_TEMP_PROBLEM_ID}`；`globalTeardown` 兜底删除 |
| **测试步骤** | 1. 在 `id=${TEST_TEMP_PROBLEM_ID}` 行触发删除按钮，弹出 Modal<br>2. 点击 `.modal__backdrop` |
| **预期结果** | Modal 关闭，列表无变化 |

#### TC-045: 题库为空

| 项目 | 内容 |
|------|------|
| **用例ID** | TC-045 |
| **用例名称** | 题库为空 |
| **测试目的** | 验证空态渲染 |
| **前置条件** | **本用例必须作为独立 suite 单独运行**。suite `beforeAll` 备份 `problems` / `test_cases` 表后 `TRUNCATE` 清空（**保留 `users` 表**）；suite `afterAll` 从 `database/init.sql` 种子数据重新导入恢复 |
| **测试步骤** | 1. 进入 `/admin.html` |
| **预期结果** | 1. `#tableWrapper` 显示 `.empty-state` 包含"暂无题目"<br>2. `#problemCount` 显示 `0 题`<br>3. 表单仍可正常使用 |
| **重跑保护** | 用例体内加 `test.skip((await getProblemCount()) > 0, '题库非空，跳过')`；若与 `TC-013` / `TC-014` 等依赖题库非空的用例混跑，必须**先关停其他 suite** |

#### TC-046: 创建题目-难度非法

| 项目 | 内容 |
|------|------|
| **用例ID** | TC-046 |
| **用例名称** | 创建题目-难度非法 |
| **测试目的** | 验证后端难度枚举校验 |
| **前置条件** | `admin` 已登录 |
| **测试步骤** | 1. 携带 admin Session Cookie，使用 `fetch('/api/admin/problems', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ title: \`illegal_${TS}\`, difficulty: 'Super', content: 'x' }) })`（`${TS}` 由 `beforeEach` 注入秒级时间戳） |
| **预期结果** | 后端返回 400 + `{"error":"Invalid difficulty. Must be Easy, Medium, or Hard"}` |

---

### 3.5 权限与安全模块

#### TC-047: 未登录访问题目列表

| 项目 | 内容 |
|------|------|
| **用例ID** | TC-047 |
| **用例名称** | 未登录访问题目列表 |
| **测试目的** | 验证未登录时的 UI 兜底 |
| **前置条件** | `context.clearCookies()`，`sessionStorage` 为空 |
| **测试步骤** | 1. 直接访问 `/problem_list.html` |
| **预期结果** | 1. `problem.js` 调用 `/api/me` 失败（401）<br>2. `#userMenu` 渲染"登录""注册"链接<br>3. `#tableWrapper` 仍可正常拉取 `/api/problems`（公开接口）<br>4. 点击题目卡片进入详情时提交等操作受限 |

#### TC-048: 未登录访问管理后台

| 项目 | 内容 |
|------|------|
| **用例ID** | TC-048 |
| **用例名称** | 未登录访问管理后台 |
| **测试目的** | 验证后台鉴权 |
| **前置条件** | 未登录 |
| **测试步骤** | 1. 清除所有 Cookie<br>2. 访问 `/admin.html` |
| **预期结果** | 1. `admin.js` 调用 `/api/me` 返回 401<br>2. 自动 `location.replace('/login.html?return=%2Fadmin.html')` |

#### TC-049: 普通用户访问管理后台

| 项目 | 内容 |
|------|------|
| **用例ID** | TC-049 |
| **用例名称** | 普通用户访问管理后台 |
| **测试目的** | 验证角色越权拦截 |
| **前置条件** | `globalSetup` 已注册并登录普通用户 `${TEST_NORMAL_USER}` |
| **测试步骤** | 1. 用 `${TEST_NORMAL_USER}` 登录（已在 `globalSetup` 完成）<br>2. 访问 `/admin.html` |
| **预期结果** | 1. `/api/me` 返回 `role=user`<br>2. 出现 Toast "需要管理员权限"（约 2.2s）<br>3. 跳转 `/problem_list.html`<br>4. 直接 `fetch('POST /api/admin/problems')` 返回 403 `{"error":"Forbidden"}` |

#### TC-050: 管理员角色标识显示

| 项目 | 内容 |
|------|------|
| **用例ID** | TC-050 |
| **用例名称** | 管理员角色标识显示 |
| **测试目的** | 验证 UI 角色区分 |
| **前置条件** | `admin` 已登录 |
| **测试步骤** | 1. 访问 `/problem_list.html`<br>2. 检查 `#userMenu` 内部 |
| **预期结果** | 1. 出现 `.admin-mark`（盾牌 SVG）<br>2. 显示"你好，admin"<br>3. 出现 `a[href="/admin.html"].user-menu__link` 入口 |

#### TC-051: 普通用户角色标识

| 项目 | 内容 |
|------|------|
| **用例ID** | TC-051 |
| **用例名称** | 普通用户角色标识 |
| **测试目的** | 验证 UI 角色区分（普通用户） |
| **前置条件** | 普通用户登录 |
| **测试步骤** | 1. 访问 `/problem_list.html`<br>2. 检查 `#userMenu` 内部 |
| **预期结果** | 1. 不出现 `.admin-mark`<br>2. 显示"你好，<username>"<br>3. 不出现 `a[href="/admin.html"]` 链接 |

#### TC-052: 已登录用户重开标签免登录

| 项目 | 内容 |
|------|------|
| **用例ID** | TC-052 |
| **用例名称** | 已登录用户重开标签免登录 |
| **测试目的** | 验证 Cookie 跨标签持久化 |
| **前置条件** | `admin` 已登录 |
| **测试步骤** | 1. 在 tab A 登录 `admin`<br>2. 新开 tab B 访问 `/problem_list.html`<br>3. 检查 `sessionStorage` 与 UI |
| **预期结果** | 1. tab B 无 `oj_username`，但 `verifyAuth` 调用 `/api/me` 返回 200<br>2. 顶部正确显示用户名与管理员入口<br>3. `sessionStorage.oj_username` 与 `oj_role` 被自动填充 |

#### TC-053: 登出后再访问后台

| 项目 | 内容 |
|------|------|
| **用例ID** | TC-053 |
| **用例名称** | 登出后再访问后台 |
| **测试目的** | 验证登出后鉴权恢复 |
| **前置条件** | 刚登出 |
| **测试步骤** | 1. 登出后访问 `/admin.html` |
| **预期结果** | 跳转 `/login.html?return=%2Fadmin.html` |

#### TC-054: 越权 POST 题库 API

| 项目 | 内容 |
|------|------|
| **用例ID** | TC-054 |
| **用例名称** | 越权 POST 题库 API |
| **测试目的** | 验证接口级鉴权 |
| **前置条件** | `globalSetup` 已注册并登录普通用户 `${TEST_NORMAL_USER}` |
| **测试步骤** | 1. 用普通用户 Cookie 调 `fetch('/api/admin/problems', { method: 'POST', body: JSON.stringify({...}) })` |
| **预期结果** | 返回 403 `{"error":"Forbidden"}` |

#### TC-055: 越权 DELETE 题库 API

| 项目 | 内容 |
|------|------|
| **用例ID** | TC-055 |
| **用例名称** | 越权 DELETE 题库 API |
| **测试目的** | 验证接口级鉴权 |
| **前置条件** | `globalSetup` 已注册并登录普通用户 `${TEST_NORMAL_USER}` |
| **测试步骤** | 1. 用普通用户 Cookie 调 `fetch('/api/admin/problems/${TEST_PROBLEM_ID}', { method: 'DELETE' })`（用动态题 id 避免对固定 1 的依赖） |
| **预期结果** | 返回 403 `{"error":"Forbidden"}` |

#### TC-056: 网络异常 Toast

| 项目 | 内容 |
|------|------|
| **用例ID** | TC-056 |
| **用例名称** | 网络异常 Toast |
| **测试目的** | 验证网络错误兜底 |
| **前置条件** | `admin` 已登录；通过 DevTools/Playwright route 拦截 `/api/problems` |
| **测试步骤** | 1. 访问 `/problem_list.html` |
| **预期结果** | 1. `#tableWrapper` 显示 `.empty-state` 包含"无法加载题目"<br>2. 含"重试"按钮，点击后重新拉取 |

---

### 3.6 页面导航与交互

#### TC-057: 落地页加载与展示

| 项目 | 内容 |
|------|------|
| **用例ID** | TC-057 |
| **用例名称** | 落地页加载与展示 |
| **测试目的** | 验证大屏首页元素完整且无 JS 错误 |
| **前置条件** | 无 |
| **测试步骤** | 1. 访问 `${BASE_URL}/index.html`<br>2. 等待网络空闲<br>3. 断言 `.hero__title`、`.stats__item`、`.feature-card` 元素可见且数量 ≥ 6 |
| **预期结果** | 1. 页面正常渲染，无 JS 报错<br>2. 顶部 `#userMenu` 显示"登录""注册"链接<br>3. Hero 区包含"立即开始""浏览题库"主按钮<br>4. 6 张特性卡片均可见 |

#### TC-058: 从落地页跳转到登录页

| 项目 | 内容 |
|------|------|
| **用例ID** | TC-058 |
| **用例名称** | 从落地页跳转到登录页 |
| **测试目的** | 验证用户菜单"登录"链接跳转 |
| **前置条件** | 无 |
| **测试步骤** | 1. 访问 `/index.html`<br>2. 点击 `#userMenu .user-menu__link[href="/login.html"]`<br>3. 等待 URL 变化 |
| **预期结果** | URL 变为 `/login.html`，浏览器历史栈新增一条 |

#### TC-059: 从落地页跳转到注册页

| 项目 | 内容 |
|------|------|
| **用例ID** | TC-059 |
| **用例名称** | 从落地页跳转到注册页 |
| **测试目的** | 验证 Hero CTA "立即开始"跳转 |
| **前置条件** | 无 |
| **测试步骤** | 1. 访问 `/index.html`<br>2. 点击 `.hero__ctas` 内的"立即开始"按钮 |
| **预期结果** | URL 变为 `/register.html` |

#### TC-060: 题目详情页"返回列表"

| 项目 | 内容 |
|------|------|
| **用例ID** | TC-060 |
| **用例名称** | 题目详情页"返回列表" |
| **测试目的** | 验证详情页返回导航 |
| **前置条件** | `admin` 已登录，题库中至少存在一道题 |
| **测试步骤** | 1. 登录后访问 `/problem_list.html`<br>2. 点击列表第一行 `.problem-table__row` 进入详情<br>3. 点击详情页左上角 `.page-header__back` |
| **预期结果** | URL 跳回 `/problem_list.html`，列表状态保留 |

---

## 四、测试数据与预置脚本

### 4.1 预置账号

| 角色 | 用户名 | 密码 | 说明 |
|------|--------|------|------|
| admin | `admin` | `admin123` | 由 `database/init.sql` 写入 |
| 普通用户 | `testuser_{unix_timestamp}` | `Test1234` | 自动化脚本动态生成 |

### 4.2 推荐测试题目

> **说明**：本节描述的「两数之和」种子题已由 `globalSetup` 自动保证存在（详见 1.3.2）：若题库中已存在任意 Easy 题，`globalSetup` 直接复用首个；若无 Easy 题则自动种子化一道。**无需测试前手动准备**。

种子化时的题目规格（由 `seedTwoSumProblem()` 实现）：

| 字段 | 值 |
|------|-----|
| 标题 | `两数之和_seed`（admin 手工调整无碍，**不**带时间戳） |
| 难度 | `Easy` |
| 描述 | `给定两个整数 a 和 b，输出它们的和。` |
| 模板 | `int main(){ int a,b; cin>>a>>b; cout<<a+b<<endl; return 0; }` |
| 用例 1 | input=`1 2`、expected=`3` |
| 用例 2 | input=`100 200`、expected=`300` |

### 4.3 测试代码片段

**AC（答案正确）**

```cpp
#include <iostream>
using namespace std;
int main() {
    int a, b;
    cin >> a >> b;
    cout << a + b << endl;
    return 0;
}
```

**WA（答案错误，输出 a-b）**

```cpp
#include <iostream>
using namespace std;
int main() {
    int a, b;
    cin >> a >> b;
    cout << a - b << endl;
    return 0;
}
```

**TLE（死循环）**

```cpp
#include <iostream>
using namespace std;
int main() {
    while (true) {}
    return 0;
}
```

**RE（段错误）**

```cpp
#include <iostream>
using namespace std;
int main() {
    int* p = nullptr;
    *p = 1;
    return 0;
}
```

**CE（缺分号）**

```cpp
#include <iostream>
using namespace std;
int main() {
    cout << "x" << endl
    return 0;
}
```

**空代码（仅空白）**

```

```

> **说明**：Ace 编辑器通过 `editor.setValue(...)` 设置内容后会自动同步；自动化脚本建议直接调用 `editor.setValue(code, -1)`。

### 4.4 动态用户名生成（伪代码）

```javascript
// Playwright 示例
const username = `testuser_${Math.floor(Date.now() / 1000)}`;
const password = 'Test1234';
```

### 4.5 幂等性矩阵

下表列出**所有具有数据库副作用**或**依赖外部状态**的用例。`beforeEach` 钩子统一负责清 Cookie + sessionStorage（详见 1.3.1），下表不再重复列出。

| 用例 ID | 副作用类型 | setup 动作 | teardown 动作 | 动态变量 | 备注 |
|---------|-----------|-----------|--------------|---------|------|
| TC-001 | +1 用户 | — | — | `${TS}` | 时间戳用户名，第二次跑不会冲突 |
| TC-002 | 无 | — | — | — | 依赖 `admin` 永久存在 |
| TC-003~005 | 无 | — | — | — | 前端校验，未提交后端 |
| TC-006~007 | 无 | — | — | — | UI 交互 |
| TC-008~009 | 无 | — | — | — | admin 登录态/失败 |
| TC-010 | 无 | `globalSetup` 取题 | — | `${TEST_PROBLEM_ID}` | 动态 id 替换 |
| TC-011~017 | 无 | — | — | — | 鉴权/列表/筛选 |
| TC-018 | 无 | `globalSetup` 取题关键字 | — | `${TEST_DIFF_KEYWORD}` | 动态关键字 |
| TC-019 | 无 | — | — | — | 搜索空态 |
| TC-020 | 无 | `globalSetup` 取题 | — | `${TEST_PROBLEM_ID}` | 动态 id |
| TC-021 | 无 | — | — | — | 99999 故意不存在 |
| TC-022 | 无 | — | — | — | 缺省参数 |
| TC-023~028 | 无 | `globalSetup` 取题 | — | `${TEST_PROBLEM_ID}` | AC/WA/CE/TLE/RE/空代码 |
| TC-029~033 | 无 | `globalSetup` 取题 + `beforeEach` 清 sessionStorage | — | `${TEST_PROBLEM_ID}` | 自定义用例不残留 |
| TC-034 | 无 | `globalSetup` 取题 | — | `${TEST_PROBLEM_ID}` | 未登录拦截 |
| TC-035~039 | 无 | — | — | — | 后台 CRUD 校验 |
| TC-036 | +1 题目 | — | `globalTeardown` 按 `_(1\d{9,})$` 正则删除 | `${TS}` | 时间戳标题 |
| TC-037~039 | 无 | — | — | — | 表单校验 |
| TC-040 | +1 题目 | — | `globalTeardown` 按 `_(1\d{9,})$` 正则删除 | `${TS}` | 时间戳标题 |
| TC-041 | -1 题目 | `globalSetup` 创建 `delete_target_${TS}` | `globalTeardown` 兜底再删一次 | `${TEST_TEMP_PROBLEM_ID}`, `${TS}` | 真实删除链路 |
| TC-042 | 0（取消） | 同 TC-041 | 同 TC-041 | 同 TC-041 | 不真删 |
| TC-043 | 0（Esc） | 同 TC-041 | 同 TC-041 | 同 TC-041 | 不真删 |
| TC-044 | 0（背景） | 同 TC-041 | 同 TC-041 | 同 TC-041 | 不真删 |
| TC-045 | **全清** | suite `beforeAll` `TRUNCATE` | suite `afterAll` `init.sql` 恢复 | — | **必须独立 suite** |
| TC-046 | 0（拒绝） | — | — | `${TS}` | 后端 400，不入库 |
| TC-047~056 | 无 | `globalSetup` 注册普通用户 | — | `${TEST_NORMAL_USER}` | 越权类依赖普通用户 |
| TC-057~060 | 无 | — | — | — | 导航类，无副作用 |

> **使用提示**：
> - 跑全量 suite 时，**禁止**把 TC-045 与其他 suite 一起执行。
> - 排查重跑失败时，优先查 `globalTeardown` 是否被正常调用（中断 / 异常退出场景）。
> - 上文 4.3 的「空代码（仅空白）」片段会被多个用例复用，若被前序用例残留污染，需配合 `editor.setValue('', -1)` + `beforeEach` 清 sessionStorage 处理。

---

## 五、执行记录

| 日期 | 测试人员 | 浏览器/分辨率 | 总用例 | 通过 | 失败 | 阻塞 | 备注 |
|------|----------|----------------|--------|------|------|------|------|
|      |          |                |        |      |      |      |      |

---

## 六、问题记录

| ID | 用例编号 | 问题描述 | 严重程度 | 复现步骤 | 状态 | 处理人 |
|----|----------|----------|----------|----------|------|--------|
| ISS-001 | TC-006 | 文档步骤 3 描述"10 字符"但样例值 `Abc12345` 实际为 8 字符；步骤 4 期望 `Abc!12345`（9 字符）得 4/很强与算法矛盾。已修正样例值为 `Abc!12345X`（10 字符）并补充算法说明。 | 低（文档问题，非实现 bug） | 1. 打开 `/register.html`<br>2. 在密码框依次输入 `a` / `abc123` / `Abc12345` / `Abc!12345X`<br>3. 观察 `#strength[data-score]`<br>原样例 `Abc!12345` 仅得 3/良好 | 已关闭 | — |

---

> **文档生成时间**：2026-06-08
> **配套技术栈**：C++17 + cpp-httplib + MySQL 8.0 + 原生 HTML/CSS/JS

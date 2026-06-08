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

> **使用说明**：测试脚本在每次启动新用例前应执行 `page.context().clearCookies()` 清理登录态；用例 TC-005 等注册场景应使用 `testuser_{unix_timestamp}` 形式动态生成用户名避免冲突。

---

## 二、测试模块规划

根据 `SPEC.md`、前端 HTML/JS 实现与后端 Handler 代码，提取出以下核心测试模块：

| 序号 | 模块 | 覆盖功能点 | 关联接口 |
|------|------|------------|----------|
| 1 | 公共页面与导航 | 落地页、登录/注册跳转、返回列表 | `/index.html` `/login.html` `/register.html` |
| 2 | 认证模块 | 注册、登录、登出、密码强度、表单校验 | `POST /api/register` `POST /api/login` `POST /api/logout` `GET /api/me` |
| 3 | 题目模块 | 列表加载、难度筛选、关键字搜索、详情、404 | `GET /api/problems` `GET /api/problems/:id` |
| 4 | 代码运行与提交 | "运行测试"、自定义用例、提交、空代码、AC/WA/CE/TLE/RE、MLE | `POST /api/run` `POST /api/submit` |
| 5 | 管理后台模块 | 访问鉴权、创建题目、添加/删除测试用例、删除确认弹窗 | `POST /api/admin/problems` `DELETE /api/admin/problems/:id` |
| 6 | 权限与安全 | 未登录跳转、跨角色越权访问、角色标识 | 全部受保护接口 |

---

## 三、详细测试用例

### 3.1 公共页面与导航模块

| 用例编号 | 用例名称 | 测试目的 | 前置条件 | 测试步骤 | 预期结果 |
|----------|----------|----------|----------|----------|----------|
| TC-001 | 落地页加载与展示 | 验证大屏首页元素完整 | 无 | 1. 访问 `${BASE_URL}/index.html`<br>2. 等待网络空闲<br>3. 断言 `.hero__title`、`.stats__item`、`.feature-card` (≥6) 元素可见 | 1. 页面正常渲染，无 JS 报错<br>2. 顶部导航显示"登录""注册"<br>3. 包含"立即开始""浏览题库"主按钮 |
| TC-002 | 从落地页跳转到登录页 | 验证用户菜单跳转 | 无 | 1. 访问 `/index.html`<br>2. 点击 `#userMenu .user-menu__link[href="/login.html"]`<br>3. 等待 URL 变化 | URL 变为 `/login.html`，落地页的 `return` 参数透传 |
| TC-003 | 从落地页跳转到注册页 | 验证 CTA 跳转 | 无 | 1. 访问 `/index.html`<br>2. 点击 `.hero__ctas` 内的"立即开始"按钮 | URL 变为 `/register.html` |
| TC-004 | 题目详情页"返回列表" | 验证详情页返回导航 | 已登录 `admin` 账号 | 1. 登录后访问 `/problem_list.html`<br>2. 点击列表第一行 `.problem-table__row` 进入详情<br>3. 点击 `.page-header__back` | URL 跳回 `/problem_list.html` |

---

### 3.2 认证模块

| 用例编号 | 用例名称 | 测试目的 | 前置条件 | 测试步骤 | 预期结果 |
|----------|----------|----------|----------|----------|----------|
| TC-005 | 用户注册成功 | 验证合法注册全流程 | 未注册的用户名 | 1. 访问 `/register.html`<br>2. 在 `#registerForm input[name="username"]` 输入 `testuser_{unix_timestamp}`<br>3. 在 `input[name="password"]` 输入 `Test1234`<br>4. 在 `input[name="confirm"]` 输入 `Test1234`<br>5. 点击 `#registerSubmit` | 1. `#registerAlert` 显示绿色成功提示"账号已创建，正在跳转…"<br>2. 约 700ms 后 URL 变为 `/login.html`<br>3. `sessionStorage.oj_username` 被清除（注册成功页不设值） |
| TC-006 | 注册失败-用户名已存在 | 验证用户名唯一性 | 已存在 `admin` | 1. 访问 `/register.html`<br>2. `input[name="username"]` 输入 `admin`<br>3. `input[name="password"]` 输入 `Test1234`<br>4. `input[name="confirm"]` 输入 `Test1234`<br>5. 点击 `#registerSubmit` | 1. `username` 字段下 `.field__error` 提示"这个用户名已被占用"<br>2. `.field.is-error` 高亮该字段<br>3. 焦点停留在 username 输入框 |
| TC-007 | 注册失败-用户名过短 | 验证长度下限 | 无 | 1. 访问 `/register.html`<br>2. `input[name="username"]` 输入 `ab`<br>3. `input[name="password"]` 输入 `Test1234`<br>4. `input[name="confirm"]` 输入 `Test1234`<br>5. 点击 `#registerSubmit` | `username` 字段 `.field__error` 提示"长度需在 3 到 64 个字符之间"，表单不提交 |
| TC-008 | 注册失败-密码过短 | 验证密码长度下限 | 无 | 1. 访问 `/register.html`<br>2. `input[name="username"]` 输入 `shortpw`<br>3. `input[name="password"]` 输入 `Aa1`<br>4. `input[name="confirm"]` 输入 `Aa1`<br>5. 点击 `#registerSubmit` | `password` 字段 `.field__error` 提示"至少需要 6 个字符" |
| TC-009 | 注册失败-两次密码不一致 | 验证确认密码逻辑 | 无 | 1. 访问 `/register.html`<br>2. `input[name="username"]` 输入 `mismatchuser`<br>3. `input[name="password"]` 输入 `Test1234`<br>4. `input[name="confirm"]` 输入 `Test5678`<br>5. 点击 `#registerSubmit` | `confirm` 字段 `.field__error` 提示"两次密码不一致" |
| TC-010 | 密码强度可视化 | 验证强度计 0→4 渐变 | 无 | 1. 访问 `/register.html`<br>2. 依次输入 `a` / `abc123` / `Abc12345` / `Abc!12345` 到 `input[name="password"]`<br>3. 观察 `#strength[data-score]` 与 `.strength__label` | 1. 输入 1 字符：`data-score="0"`，label 为空<br>2. 输入 6 字符纯字母数字：`data-score="2"`，label "一般"<br>3. 输入 10 字符混合：`data-score="3"`，label "良好"<br>4. 输入含特殊符号：`data-score="4"`，label "很强" |
| TC-011 | 密码显示/隐藏切换 | 验证眼睛图标切换 | 无 | 1. 访问 `/login.html`<br>2. 在 `input[name="password"]` 输入 `Test1234`<br>3. 点击 `[data-toggle="password"]`<br>4. 再次点击切换 | 1. 第一次点击：input.type 变为 `text`，眼睛图标切换为"eye-off"<br>2. `aria-pressed="true"`<br>3. 第二次点击：恢复 `password` 隐藏 |
| TC-012 | 管理员登录成功 | 验证登录与跳转 | 无 | 1. 访问 `/login.html`<br>2. `input[name="username"]` 输入 `admin`<br>3. `input[name="password"]` 输入 `admin123`<br>4. 点击 `#loginSubmit` | 1. `#loginAlert` 显示绿色"欢迎回来，正在跳转…"<br>2. 约 600ms 后跳转 `/problem_list.html`<br>3. 浏览器收到 `Set-Cookie: oj_session=...; HttpOnly`<br>4. `sessionStorage.oj_username="admin"`，`sessionStorage.oj_role="admin"` |
| TC-013 | 登录失败-密码错误 | 验证错误凭据处理 | 无 | 1. 访问 `/login.html`<br>2. `input[name="username"]` 输入 `admin`<br>3. `input[name="password"]` 输入 `wrongpassword`<br>4. 点击 `#loginSubmit` | 1. `#loginAlert` 红色错误提示"用户名或密码错误"<br>2. URL 仍为 `/login.html`<br>3. 未设置 `oj_session` Cookie |
| TC-014 | 登录带 return 参数回到原页 | 验证深链回跳 | 无 | 1. 访问 `/problem.html?id=1` 自动跳到 `/login.html?return=%2Fproblem.html%3Fid%3D1`<br>2. 输入 `admin` / `admin123` 登录 | 登录成功后跳回 `/problem.html?id=1`，`sessionStorage.oj_username="admin"` |
| TC-015 | 已登录访问登录页自动跳过 | 验证重入优化 | `admin` 已登录，Session 有效 | 1. 访问 `/problem_list.html` 写入 sessionStorage<br>2. 直接访问 `/login.html` | `/login.html` 通过 `/api/me` 验证后自动 `location.replace` 跳到 `return` 指定的 `/problem_list.html`（或首页） |
| TC-016 | 用户登出（题目列表页） | 验证登出清理 | `admin` 已登录 | 1. 访问 `/problem_list.html`<br>2. 点击 `#userMenu [data-action="logout"]`<br>3. 等待 Toast 与跳转 | 1. 显示 Toast "已退出登录"<br>2. `Set-Cookie: oj_session=; Max-Age=0`<br>3. `sessionStorage.oj_username` / `oj_role` 被清除<br>4. 约 700ms 后跳转到 `/login.html` |

---

### 3.3 题目模块

| 用例编号 | 用例名称 | 测试目的 | 前置条件 | 测试步骤 | 预期结果 |
|----------|----------|----------|----------|----------|----------|
| TC-017 | 题目列表加载 | 验证列表渲染与计数 | `admin` 已登录 | 1. 访问 `/problem_list.html`<br>2. 等待 `#tableWrapper` 不再 `aria-busy="true"`<br>3. 断言 `.problem-table__row` 至少 1 行<br>4. 检查 `.filter__chip-count` | 1. 渲染题目表格，列：题号 / 标题 / 难度<br>2. 四个难度 chip 右侧显示对应题目数量<br>3. 每行 `.difficulty` 显示难度条 + 中文标签 |
| TC-018 | 难度筛选-简单 | 验证筛选 | `admin` 已登录，存在多种难度 | 1. 访问 `/problem_list.html`<br>2. 点击 `.filter__chip[data-difficulty="Easy"]`<br>3. 等待列表刷新 | 1. Easy chip 拥有 `is-active` 与 `aria-selected="true"`<br>2. 所有可见行 `.difficulty[data-difficulty="Easy"]`<br>3. 搜索框与列表内容无关 |
| TC-019 | 难度筛选-中等 | 验证 Medium 筛选 | 同上 | 1. 点击 `.filter__chip[data-difficulty="Medium"]` | 仅显示 Medium 题目；chip `is-active` |
| TC-020 | 难度筛选-困难 | 验证 Hard 筛选 | 同上 | 1. 点击 `.filter__chip[data-difficulty="Hard"]` | 仅显示 Hard 题目 |
| TC-021 | 难度筛选-重置为全部 | 验证重置 | 当前已选 Medium | 1. 点击 `.filter__chip[data-difficulty="all"]` | "全部" chip 重新 `is-active`，列表恢复全集 |
| TC-022 | 按标题关键字搜索 | 验证搜索防抖 | 已知某题标题关键字（如"两数"） | 1. 访问 `/problem_list.html`<br>2. 在 `#searchInput` 输入该关键字<br>3. 等待 200ms（防抖 80ms） | 列表只显示标题包含该关键字的题目，搜索对大小写不敏感 |
| TC-023 | 搜索无匹配结果 | 验证空态 | `admin` 已登录 | 1. 在 `#searchInput` 输入 `__no_such_problem__` | 显示空态 `.empty-state` 包含"没有匹配的题目"与"查看全部"按钮，点击该按钮后重置搜索与筛选 |
| TC-024 | 查看题目详情 | 验证详情渲染 | `admin` 已登录，存在题目 ID=1 | 1. 在题目列表点击第一行（或访问 `/problem.html?id=1`）<br>2. 等待 `#problemContent[aria-busy="false"]` | 1. URL 变为 `/problem.html?id=1`<br>2. `#problemTitle` 显示题目标题<br>3. `#problemEyebrow` 显示"题库 · 题目 #1"<br>4. `#problemMeta` 显示难度条<br>5. `#editor` 内出现 Ace 编辑器（`aria-label` 或 `.ace_text-input`）<br>6. `#testCaseList` 至少存在一个 `.test-case-row--readonly`（官方用例） |
| TC-025 | 题目详情页 404 | 验证不存在题目处理 | `admin` 已登录 | 1. 访问 `/problem.html?id=99999` | 1. 页面渲染 `.empty-state` 包含"题目不存在"<br>2. 含"返回列表"按钮，点击跳转 `/problem_list.html` |
| TC-026 | 题目详情页缺 ID | 验证参数校验 | `admin` 已登录 | 1. 访问 `/problem.html`（无参数） | 显示 `.empty-state` 包含"题目不存在"与"链接缺少题号" |

---

### 3.4 代码运行与提交模块

> **前置约定**：TC-027~TC-040 全部基于"`admin` 已登录"前置；测试题目使用后端初始化脚本中的样例题（如有"两数之和" `id=1`）。若题库为空，请先通过 TC-037~TC-040 创建至少一道"两数之和"题。

| 用例编号 | 用例名称 | 测试目的 | 前置条件 | 测试步骤 | 预期结果 |
|----------|----------|----------|----------|----------|----------|
| TC-027 | 提交代码-答案正确 (AC) | 验证 AC 全流程 | 进入 `id=1` 详情页 | 1. 等待编辑器加载完成<br>2. 在 Ace 编辑器中输入并执行：`editor.setValue("#include <iostream>\\nusing namespace std;\\nint main(){ int a,b; cin>>a>>b; cout<<a+b<<endl; return 0; }")`<br>3. 点击 `#submitBtn`<br>4. 等待 `#resultArea` 不再 pending | 1. 提交按钮显示 `is-loading`<br>2. 结果区出现 `.result-card--ac`<br>3. `.result-card__badge` 文本为 `AC`<br>4. `.result-card__label` 文本为"通过"<br>5. 显示 `executionTimeMs` |
| TC-028 | 提交代码-答案错误 (WA) | 验证 WA 处理 | 同上 | 1. 编辑器输入 `cout << a - b << endl;`（输出差值）<br>2. 点击 `#submitBtn` | 1. `.result-card--wa`<br>2. 顶部 badge 显示 `WA`<br>3. 副标题"答案错误，输出与预期不符" |
| TC-029 | 提交代码-编译错误 (CE) | 验证 CE 处理 | 同上 | 1. 编辑器输入 `int main(){ cout << "x" }`（缺分号）<br>2. 点击 `#submitBtn` | 1. `.result-card--ce`<br>2. 顶部 badge 显示 `CE`<br>3. 编译信息区出现 `.result-card__output`，文本包含 `error:` 或 `expected` |
| TC-030 | 提交代码-运行超时 (TLE) | 验证 TLE | 同上 | 1. 编辑器输入 `int main(){ while(true){} return 0; }`<br>2. 点击 `#submitBtn`<br>3. 等待 ≤ 8s | 1. 约 5s 内返回<br>2. `.result-card--tle`<br>3. badge 显示 `TLE`<br>4. 副标题"运行超时" |
| TC-031 | 提交代码-运行错误 (RE) | 验证 RE | 同上 | 1. 编辑器输入 `int* p=nullptr; *p=1;`（段错误）<br>2. 点击 `#submitBtn` | 1. `.result-card--re`<br>2. badge 显示 `RE`<br>3. 副标题"运行错误" |
| TC-032 | 提交代码-空代码 | 验证空代码拦截 | 同上 | 1. 点击 `#resetBtn` 清空（默认模板会被替换）<br>2. 在编辑器输入 `"   "`（仅空白）<br>3. 点击 `#submitBtn` | 1. 不发请求<br>2. `.result-card--network` 出现<br>3. 副标题"代码不能为空" |
| TC-033 | 运行测试-官方用例 | 验证 `/api/run` 流程 | 进入详情页 | 1. 编辑器输入 AC 答案<br>2. 点击 `#runTestBtn`<br>3. 等待结果 | 1. 按钮显示 `is-loading` 期间禁用<br>2. `.result-card--test` 与 `.result-card--ac`<br>3. 顶部 badge 显示 `N/N` 通过计数<br>4. 列表中每个 `.test-case` 含 `test-case--ac`，`source` 标记"官方"<br>5. 每个用例显示 `executionTimeMs` |
| TC-034 | 运行测试-添加自定义用例 | 验证 LeetCode 风格用例 | 同上 | 1. 点击 `#addTestCaseBtn`<br>2. 在最后一个 `.test-case-row[data-source="custom"]` 的 `textarea[data-field="input"]` 输入 `42 58`<br>3. 再次点击 `#runTestBtn` | 1. 列表增加一行 `test-case-row--custom`<br>2. 运行后结果区出现自定义用例（`source: 自定义`），无"预期"行<br>3. 顶部 badge 不计入自定义用例的 pass/fail<br>4. 副标题"全部 N 个官方用例通过，另运行 M 个自定义用例" |
| TC-035 | 重置代码按钮 | 验证重置逻辑 | 编辑器已修改 | 1. 修改编辑器内容为 `garbage`<br>2. 点击 `#resetBtn` | 1. 编辑器内容恢复为初始模板（或服务端返回的 `template`）<br>2. `#resultArea` 清空<br>3. 焦点回到编辑器<br>4. `sessionStorage.oj_editor_code_{id}` 被删除 |
| TC-036 | 代码编辑后重新加载页面 | 验证 sessionStorage 持久化 | 已编辑代码 | 1. 在编辑器中输入 `MY_CUSTOM_CODE`<br>2. 等待 ≥ 200ms（自动保存）<br>3. `page.reload()` | 重新加载后编辑器内仍是 `MY_CUSTOM_CODE`，而非默认模板 |
| TC-037 | 自定义用例可删除 | 验证删除交互 | 进入详情页 | 1. 点击 `#addTestCaseBtn` 三次<br>2. 在第二个自定义行点击 `.test-case-row__remove`<br>3. 验证该行消失 | 该自定义用例行被删除，剩余行重新编号；官方用例行无删除按钮 |
| TC-038 | 未登录提交被拦截 | 验证提交鉴权 | 普通用户但 `sessionStorage` 已清空 | 1. 清除 `sessionStorage.oj_username`<br>2. 进入题目详情页（Cookie 仍有效）<br>3. 点击 `#submitBtn` | 1. 跳转到 `/login.html?return=...`<br>2. 登录后回到原题 |

---

### 3.5 管理后台模块

| 用例编号 | 用例名称 | 测试目的 | 前置条件 | 测试步骤 | 预期结果 |
|----------|----------|----------|----------|----------|----------|
| TC-039 | 管理员访问管理后台 | 验证后台入口 | `admin` 已登录 | 1. 访问 `/problem_list.html`<br>2. 点击 `#userMenu .user-menu__link[href="/admin.html"]`<br>3. 进入 `/admin.html` | 1. URL 变为 `/admin.html`<br>2. 显示 `#newProblemForm` 表单<br>3. 右侧 `#tableWrapper` 渲染题目表格<br>4. 每行含 `.problem-table__delete` 按钮<br>5. 顶部导航 `管理` chip `is-active` |
| TC-040 | 创建新题目 | 验证完整创建流程 | `admin` 已登录 | 1. 访问 `/admin.html`<br>2. `input[name="title"]` 输入 `两数之和_{unix_timestamp}`<br>3. `input[name="difficulty"][value="Easy"]` 选中（默认）<br>4. `textarea[name="content"]` 输入 `给定两个整数 a 和 b，输出它们的和。`<br>5. `textarea[name="template"]` 输入 `int main(){...}`<br>6. 用例 1：input=`1 2`、expected=`3`；用例 2：input=`100 200`、expected=`300`<br>7. 点击 `#submitNewBtn` | 1. 按钮进入 `is-loading` 并禁用<br>2. 出现 Toast "题目已添加"<br>3. 右侧题目列表自动刷新并出现新题（按 id 升序）<br>4. `#problemCount` 计数 +1<br>5. 表单被重置，测试用例恢复为默认两行 |
| TC-041 | 创建题目-缺少标题 | 验证必填校验 | `admin` 已登录 | 1. 进入 `/admin.html`<br>2. 不填 `input[name="title"]`<br>3. 其余字段正常填写<br>4. 点击 `#submitNewBtn` | 出现 Toast "请填写标题"，表单不提交 |
| TC-042 | 创建题目-缺少描述 | 验证描述必填 | `admin` 已登录 | 1. 填标题，不填 `textarea[name="content"]`<br>2. 点击 `#submitNewBtn` | 出现 Toast "请填写题目描述" |
| TC-043 | 创建题目-添加多个测试用例 | 验证动态增删 | `admin` 已登录 | 1. 点击 `#addCaseBtn` 3 次<br>2. 断言共 5 行 `.form__test-case`（默认 2 + 新增 3）<br>3. 在第一行点击 `[data-remove]`<br>4. 断言共 4 行且编号连续 | 1. 测试用例按 `[data-index]` 渲染<br>2. 删除后行数减 1 并重新编号<br>3. 仅剩 1 行时 `[data-remove]` 不可点 |
| TC-044 | 创建题目-空测试用例跳过 | 验证容错 | `admin` 已登录 | 1. 填好标题/描述/难度<br>2. 测试用例全部留空<br>3. 点击 `#submitNewBtn` | 题目仍可创建成功（请求中不携带 `testCases` 数组），Toast "题目已添加" |
| TC-045 | 删除题目-确认 | 验证删除 | `admin` 已登录，存在题目 id=1 | 1. 进入 `/admin.html`<br>2. 点击 id=1 行的 `.problem-table__delete`<br>3. `#deleteModal` 弹出后点击 `#confirmDeleteBtn` | 1. Modal 出现 `#deleteModalBody` 文案"将永久删除题目 #1「…」"<br>2. 焦点默认在 `#cancelDeleteBtn`<br>3. 确认后出现 Toast "正在删除…"，再出现"已删除「…」"<br>4. 列表行数 -1，`#problemCount` 同步更新<br>5. 后端 `DELETE /api/admin/problems/1` 返回 200 |
| TC-046 | 删除题目-取消 | 验证取消 | 同上 | 1. 触发删除按钮，弹出 Modal<br>2. 点击 `#cancelDeleteBtn` | Modal `hidden=true`，列表无变化，无后端 DELETE 请求 |
| TC-047 | 删除题目-Esc 关闭弹窗 | 验证键盘交互 | 同上 | 1. 触发删除按钮，弹出 Modal<br>2. 按 `Escape` 键 | Modal 关闭，列表无变化 |
| TC-048 | 删除题目-点击背景关闭 | 验证背景交互 | 同上 | 1. 触发删除按钮，弹出 Modal<br>2. 点击 `.modal__backdrop` | Modal 关闭，列表无变化 |
| TC-049 | 题库为空 | 验证空态 | 全部题目已被删除 | 1. 进入 `/admin.html` | 1. `#tableWrapper` 显示 `.empty-state` 包含"暂无题目"<br>2. `#problemCount` 显示 `0 题`<br>3. 表单仍可正常使用 |
| TC-050 | 创建题目-难度非法 | 验证难度枚举 | `admin` 已登录 | 1. 用 JS 绕过前端：`fetch('/api/admin/problems', {method:'POST', headers:{'Content-Type':'application/json', Cookie: ...}, body: JSON.stringify({title:'x', difficulty:'Super', content:'x'})})` | 后端返回 400 + `{"error":"Invalid difficulty. Must be Easy, Medium, or Hard"}` |

---

### 3.6 权限与安全模块

| 用例编号 | 用例名称 | 测试目的 | 前置条件 | 测试步骤 | 预期结果 |
|----------|----------|----------|----------|----------|----------|
| TC-051 | 未登录访问题目列表 | 验证路由保护 | `context.clearCookies()`，`sessionStorage` 为空 | 1. 直接访问 `/problem_list.html` | 1. `problem.js` 调用 `/api/me` 失败（401）<br>2. `#userMenu` 渲染"登录""注册"链接<br>3. `#tableWrapper` 仍可正常拉取 `/api/problems`（公开接口），但 `#userMenu` 顶部无用户名<br>4. 用户点击题目卡片进入详情时，受保护操作受限 |
| TC-052 | 未登录访问管理后台 | 验证后台鉴权 | 未登录 | 1. 清除所有 Cookie<br>2. 访问 `/admin.html` | 1. `admin.js` 调用 `/api/me` 返回 401<br>2. 自动 `location.replace('/login.html?return=...')`<br>3. 后续接口调用失败时按钮给 Toast "需要管理员权限" |
| TC-053 | 普通用户访问管理后台 | 验证角色越权拦截 | 已注册普通用户 `testuser_xxx` 登录 | 1. 用普通用户登录<br>2. 访问 `/admin.html` | 1. `/api/me` 返回 `role=user`<br>2. 出现 Toast "需要管理员权限"（约 2.2s）<br>3. 跳转 `/problem_list.html`<br>4. 直接 `fetch` `POST /api/admin/problems` 返回 403 `{"error":"Forbidden"}` |
| TC-054 | 管理员角色标识显示 | 验证 UI 角色区分 | `admin` 已登录 | 1. 访问 `/problem_list.html`<br>2. 检查 `#userMenu` 内部 | 1. 出现 `.admin-mark`（盾牌 SVG）<br>2. 显示"你好，admin"<br>3. 出现 `a[href="/admin.html"].user-menu__link` 入口 |
| TC-055 | 普通用户角色标识 | 验证 UI 角色区分 | 普通用户登录 | 1. 访问 `/problem_list.html`<br>2. 检查 `#userMenu` 内部 | 1. 不出现 `.admin-mark`<br>2. 显示"你好，<username>"<br>3. **不**出现 `a[href="/admin.html"]` 链接 |
| TC-056 | 已登录用户重开标签免登录 | 验证 Cookie 持久化 | `admin` 已登录 | 1. 在 tab A 登录 `admin`<br>2. 新开 tab B 访问 `/problem_list.html`<br>3. 检查 `sessionStorage` 与 UI | 1. tab B 无 `oj_username`，但 `verifyAuth` 调用 `/api/me` 返回 200<br>2. 顶部正确显示用户名与管理员入口<br>3. `sessionStorage.oj_username` 与 `oj_role` 被自动填充 |
| TC-057 | 登出后再访问后台 | 验证登出后鉴权 | 刚登出 | 1. 登出后访问 `/admin.html` | 跳转 `/login.html?return=%2Fadmin.html` |
| TC-058 | 越权 POST 题库 API | 验证接口级鉴权 | 普通用户登录 | 1. 普通用户用 `fetch('/api/admin/problems', {method:'POST', ...})` 创建题目 | 返回 403 `{"error":"Forbidden"}` |
| TC-059 | 越权 DELETE 题库 API | 验证接口级鉴权 | 普通用户登录 | 1. `fetch('/api/admin/problems/1', {method:'DELETE', ...})` | 返回 403 `{"error":"Forbidden"}` |
| TC-060 | 网络异常 Toast | 验证错误兜底 | `admin` 已登录；通过 DevTools 阻断 `/api/problems` | 1. 访问 `/problem_list.html` | 1. `#tableWrapper` 显示 `.empty-state` 包含"无法加载题目"<br>2. 含"重试"按钮，点击后重新拉取 |

---

## 四、测试数据与预置脚本

### 4.1 预置账号

| 角色 | 用户名 | 密码 | 说明 |
|------|--------|------|------|
| admin | `admin` | `admin123` | 由 `database/init.sql` 写入 |
| 普通用户 | `testuser_{unix_timestamp}` | `Test1234` | 自动化脚本动态生成 |

### 4.2 推荐测试题目

自动化前请确保题库至少包含 1 道"两数之和"题（Easy），用例：

| 用例 # | stdin | stdout 预期 |
|--------|-------|-------------|
| 1 | `1 2` | `3` |
| 2 | `100 200` | `300` |

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

> **说明**：Ace 编辑器通过 `editor.setValue(...)` 设置内容后，需要 `editor.navigateFileEnd()` 或重新聚焦才能让事件系统把值同步；自动化脚本建议直接调用 `editor.setValue(code, -1)`。

### 4.4 动态用户名生成（伪代码）

```javascript
// Playwright 示例
const username = `testuser_${Math.floor(Date.now() / 1000)}`;
const password = 'Test1234';
```

---

## 五、执行记录

| 日期 | 测试人员 | 浏览器/分辨率 | 总用例 | 通过 | 失败 | 阻塞 | 备注 |
|------|----------|----------------|--------|------|------|------|------|
|      |          |                |        |      |      |      |      |

---

## 六、问题记录

| ID | 用例编号 | 问题描述 | 严重程度 | 复现步骤 | 状态 | 处理人 |
|----|----------|----------|----------|----------|------|--------|
|    |          |          |          |          |      |        |

---

> **文档生成时间**：2026-06-08
> **配套技术栈**：C++17 + cpp-httplib + MySQL 8.0 + 原生 HTML/CSS/JS

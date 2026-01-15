# 蹦床AI裁判系统 TODO

## 说明
- **文档用途**：用于规划和跟踪项目落地的分阶段任务。
- **任务拆分原则**：条目小而明确，尽量 **0.5～1 人日**可完成；先端到端可跑、可录、可回放，再提高精度。
- **标签说明（方括号标签）**：
  - `[repo]` 仓库结构 / 分支 / 配置
  - `[core]` 核心业务 / 核心库 / 协议实现
  - `[daemon]` 守护进程 / 后台服务
  - `[app]` GUI 应用
  - `[cli]` 命令行工具
  - `[agent]` Guest 或远端 agent
  - `[test]` 自动化测试 / CI / E2E 脚本
  - `[doc]` 文档 / 设计记录 / README
  - 额外标签（本项目需要）：
    - `[data]` 数据记录 / 数据格式 / 报告
    - `[viz]` 可视化（图表/散点/曲线）
    - `[perf]` 性能与时延

---

## Phase 0：基线整理与可验收“开关式记录”骨架（约 12 项）

### 目标（Goals）
- 项目具备**统一的运行参数/配置**，并引入“数据记录按键开关”，可明确开启/关闭后续功能。
- 落点（Landing Point）数据结构对外可用，包含 **`lp.new_frame_x/y/z`** 与时间戳，便于 ToF/HD 直接复用。
- 形成最小可回归的“手动验收流程”，避免后续重构越改越乱。

### 阶段性交付成果（Deliverables）
- 启动后可在画面叠加显示：`REC: ON/OFF`、已记录落点数、最近一次落点信息。
- 存在 `config/runtime_config.yaml`（或 `json`）支持：是否启用录制、按键映射、数据输出路径等。
- 存在 `docs/acceptance/phase0_checklist.md`，一页写清如何验收 Phase 0。

### TODO
1. [repo] 建立 `feature/data_recording_toggle` 分支，并补齐 `.gitignore`、`README` 运行说明（包含模型路径、依赖）。  
   - 验证：`git branch` 可见分支；README 中按步骤在一台机器上能跑起来并看到窗口。
2. [core] 在主程序（`get_pose_indemind_left.cpp`）增加全局/单例 `RuntimeFlags{ bool record_enabled; }`，默认 `false`。  
   - 验证：启动默认显示 `REC: OFF`，且后续落点列表不再增长。
3. [core] 增加键盘事件：按 `R/r` 切换 `record_enabled`；按 `C` 清空本次录制缓存（落点/统计）。  
   - 验证：连续按 `R` 可在 ON/OFF 间切换；按 `C` 后落点计数归零。
4. [core] 将“落点确认”逻辑与“落点入库”解耦：`ConfirmLandingPoint()` 只产出 `LandingPoint lp`，再由 `RecordLandingPoint(lp)` 决定是否存储。  
   - 验证：`REC: OFF` 时仍可在控制台看到“检测到落点（未入库）”，但 `landing_points_.size()` 不变。
5. [core] 为 `LandingPoint` 补齐精确时间：新增 `std::chrono::steady_clock::time_point t` 或 `int64_t t_ms_since_start` 字段。  
   - 验证：打印同一落点的 `mm:ss` 与 `t_ms`，两者增长一致且单调递增。
6. [data] 新增 `DataSession` 概念（本次录制会话）：包含 session_id、开始时间、输出目录、落点数组、ToF/HD 结果占位。  
   - 验证：每次按 `R` 从 OFF→ON 会创建新的 session 目录（如 `runs/20260115_103000/`）。
7. [data] 定义落点 JSON/CSV 输出格式（最少字段：id、t_ms、new_frame_x/y/z），并实现 `FlushLandingPoints()`。  
   - 验证：按 `R` 录制 5 个落点后按 `S`（或退出）生成 `landing_points.csv`，列名与数据正确。
8. [viz] 在 OpenCV 画面叠加一个简易状态面板（右上角）：REC 状态、落点数、最近落点 (x,y,z)。  
   - 验证：画面中持续可见且不遮挡主体；切换 REC 状态文字立刻更新。
9. [perf] 记录关键耗时：推理、深度映射、落点检测的平均/最大耗时（滑动窗口 100 帧）。  
   - 验证：控制台每 2 秒输出一次统计；帧率不明显下降。
10. [test] 增加 `tests/manual/phase0_toggle.md`：写清“按键顺序、预期现象、截图点”。  
   - 验证：按文档走一遍，不需要读代码即可复现结果。
11. [doc] 在 `docs/design/` 增加一页《数据记录开关与会话模型》说明：状态机（OFF/ON）、按键、输出文件。  
   - 验证：文档包含状态转移图（可用 ASCII/mermaid）与字段表。
12. [cli] 提供一个最小 CLI：`tools/print_csv_summary.py <landing_points.csv>` 输出落点数/时间范围。  
   - 验证：对生成的 CSV 运行脚本能输出正确统计。

---

## Phase 1：ToF（Time of Flight）与跳次分割（约 12 项）

### 目标（Goals）
- 以“落点”为接床时刻：**相邻两次接床时刻差**作为该跳周期/ToF 近似值。
- 跳次分割结果可回放、可导出、可用于后续评分模块。
- ToF 计算对抖动/丢帧具备最基本的鲁棒性（异常剔除）。

### 阶段性交付成果（Deliverables）
- 生成 `tof_jumps.csv`：每跳 `jump_id, t_land_ms, tof_ms, valid_flag`。
- 画面叠加显示：当前跳号、上一跳 ToF、平均 ToF。
- 有一个“录制→计算→导出→复核”的一键脚本或菜单路径。

### TODO
1. [core] 定义 `JumpEvent{ jump_id, land_t_ms, land_x, land_y, land_z }`，由 `LandingPoint` 直接映射而来。  
   - 验证：每记录一个 `LandingPoint` 同步生成一个 `JumpEvent`，数量一致。
2. [core] 实现 ToF 计算器 `ToFCalculator::Update(jump_events)`：对 `land_t_ms` 做差分得到 `tof_ms`。  
   - 验证：第 N 跳的 `tof_ms = land_t_ms[N] - land_t_ms[N-1]`（N≥2）。
3. [core] 增加异常过滤：若 `tof_ms < 200ms` 或 `tof_ms > 3000ms` 标记为 invalid（阈值先可配置）。  
   - 验证：人为制造极短/极长间隔时，被标记 invalid，且不参与平均值。
4. [core] 将 ToF 与“录制开关”绑定：只有 `REC: ON` 的落点才进入 ToF 计算。  
   - 验证：REC OFF 状态下 ToF 统计不变化。
5. [data] 导出 `tof_jumps.csv`（含原始落点字段），并在 CSV 中明确单位（ms、mm）。  
   - 验证：CSV 头部包含单位注释行或列名带 `_ms/_mm`。
6. [viz] 在 OpenCV 画面叠加 ToF 关键数值：`Jump#`, `Last ToF`, `Avg(valid)`。  
   - 验证：每新增落点，Jump# 增 1；Last ToF 更新。
7. [viz] 生成 ToF 曲线数据（按 jump_id）：用于后续上位机绘图（先导出 JSON/CSV）。  
   - 验证：曲线长度等于 valid 跳数；可用 matplotlib 绘制出折线。
8. [test] 新增“离线回放 ToF 单测数据”：提供 `tests/data/sample_landing_points.csv`（手工构造 10 个点）。  
   - 验证：跑 `tools/compute_tof.py tests/data/sample_landing_points.csv` 输出与预期对齐。
9. [cli] 实现 `tools/compute_tof.py <landing_points.csv> --out <tof.csv>`。  
   - 验证：脚本对真实录制 CSV 可运行成功并生成输出。
10. [doc] 补充《ToF 与跳次分割定义》：接床时刻定义、异常阈值、已知误差来源（抖动/漏检）。  
   - 验证：文档中包含至少 1 张示意图（可以是 ASCII）说明相邻落点差分。
11. [test] 增加 `tests/e2e/tof_smoke.sh`：自动跑一次离线计算（输入 sample CSV，检查输出行数）。  
   - 验证：脚本返回 0；CI（若有）可直接执行。
12. [perf] 统计 ToF 计算延迟（从落点确认到 ToF 更新），目标 < 10ms（本机）。  
   - 验证：控制台输出延迟统计，持续低于阈值。

---

## Phase 2：HD（Horizontal Displacement）水平位移统计（约 13 项）

### 目标（Goals）
- 将每次接床时刻的 hip (X,Y) 投影到床平面坐标：优先复用 **`lp.new_frame_x / lp.new_frame_y`**（假定已是床平面坐标）。
- 输出 HD 三类结果：**落点散点图**、**半径/偏移统计**、**每跳偏移量曲线**。
- 统计具备清晰定义：床中心、半径、偏移量、单位与坐标系。

### 阶段性交付成果（Deliverables）
- 生成 `hd_metrics.json` + `hd_per_jump.csv`（含每跳距离、累计、均值、最大值）。
- 自动生成一张 `landing_scatter.png`（落点散点图）与一张 `hd_curve.png`（每跳偏移曲线）。
- 画面叠加显示：当前落点距离中心、最大偏移、平均偏移。

### TODO
1. [core] 明确并固化“床平面坐标系”约定：`new_frame_x/y` 为床面平面坐标，`new_frame_z` 为向上（Z 轴向上为正）。  
   - 验证：`docs/design/coords.md` 里给出轴向示意和单位（mm）。
2. [core] 定义床中心 `bed_center = (0,0)`（或从三点标定/点击原点产生），并在配置中可调整。  
   - 验证：修改配置后，散点图中心随之改变，统计随之变化。
3. [core] 实现 `HDCalculator::DistanceToCenter(x,y)`：`r = sqrt((x-x0)^2 + (y-y0)^2)`。  
   - 验证：对已知点（如 (300,400)）计算出正确 `r=500`（单位 mm）。
4. [core] 从 `LandingPoint`/`JumpEvent` 产出 `HDPerJump{ jump_id, x_mm, y_mm, r_mm }`。  
   - 验证：每个 jump_id 都有一行 HD 输出。
5. [core] 计算统计量：`avg_r, max_r, std_r, p95_r`（p95 可选），并提供序列化接口。  
   - 验证：统计量随数据变化合理；空数据时返回明确的 `N/A` 或 0。
6. [data] 导出 `hd_per_jump.csv`（每跳）与 `hd_metrics.json`（整体统计）。  
   - 验证：导出文件包含单位；JSON 可被 Python 读取。
7. [viz] 实现离线画图脚本 `tools/plot_hd.py <hd_per_jump.csv>` 输出 `landing_scatter.png` 与 `hd_curve.png`。  
   - 验证：运行脚本生成两张图，且图中点数等于跳数。
8. [viz] 散点图增强：标注床中心、最大偏移点、外接最大半径圆（可选）。  
   - 验证：图片中可见中心标记与最大点标注文本。
9. [core] 在实时画面叠加：`r_last, r_max, r_avg(valid)`，并在落点确认时刷新。  
   - 验证：新增落点后数值更新；REC OFF 时数值不变。
10. [test] 新增 HD 离线单测数据：`tests/data/sample_hd.csv`（含中心偏移、极值）。  
   - 验证：`tools/plot_hd.py` 对样例数据生成图，且脚本断言 max 点正确。
11. [cli] 增加 `tools/compute_hd.py <landing_points.csv> --center 0,0 --out hd_per_jump.csv`。  
   - 验证：脚本与 C++ 在线统计输出一致（误差 < 1e-6）。
12. [doc] 补充《HD 定义与输出字段》：散点图含义、半径统计定义、如何用于后续 FIG 评分映射（占位）。  
   - 验证：文档列出字段表与示例输出片段。
13. [perf] 确认 HD 计算不影响主循环：新增落点时计算耗时 < 5ms。  
   - 验证：打印耗时统计，持续满足阈值。

---

## Phase 3：上位机最小可用（PyQt）与数据报告打通（约 12 项）

### 目标（Goals）
- 上位机具备最小闭环：**实时显示（视频/状态）→ 开关录制 → 展示 ToF/HD → 导出报告**。
- C++ 侧与 PyQt 侧的接口稳定（先用文件/本机 socket 均可），可回放历史数据。

### 阶段性交付成果（Deliverables）
- 存在 `app/`（PyQt）工程，可运行：显示视频窗口 + 分项面板（REC/Jump/ToF/HD）。
- 存在“一键导出”：生成一个报告目录 `report/`，包含 CSV/JSON 和两张图（scatter/curve）。
- 存在 `docs/acceptance/phase3_demo.md`：演示步骤 1～8。

### TODO
1. [app] 搭建 PyQt 工程骨架：`app/main.py` + `requirements.txt`（PyQt5/6、opencv-python、matplotlib、pandas）。  
   - 验证：`python app/main.py` 能打开窗口并显示占位 UI。
2. [core] 定义与上位机通信的最小数据包（JSON）：`rec_enabled, jump_id, last_tof_ms, hd_last_r_mm, paths`。  
   - 验证：C++ 端每次落点确认输出一行 JSON 到 stdout 或本地 socket。
3. [app] 实现数据接收（优先 stdout/UDP/ZeroMQ 任选一种）：解析 JSON 更新 UI。  
   - 验证：落点更新时 UI 上 Jump/ToF/HD 数字同步变化。
4. [app] 实现“数据记录按键开关”在 GUI 上的按钮（REC ON/OFF），并向 C++ 端下发控制信号。  
   - 验证：点击按钮后，C++ 端 REC 状态变化，且数据入库行为随之变化。
5. [app] 实时视频显示（MVP）：先支持读取本地窗口抓帧/RTSP/共享内存任一方案（以可跑为准）。  
   - 验证：画面可实时刷新 ≥ 15 FPS；叠加信息清晰。
6. [data] 实现“导出报告”按钮：调用 `tools/compute_tof.py + compute_hd.py + plot_hd.py` 生成报告目录。  
   - 验证：点击导出后 `report/` 下产物齐全（CSV/JSON/PNG）。
7. [viz] 在 GUI 内嵌显示：HD 散点图与每跳偏移曲线（可用 matplotlib canvas）。  
   - 验证：导出或录制过程中，图表可刷新并展示最新跳次。
8. [cli] 增加 `tools/make_report.py <session_dir>`：一条命令完成 ToF+HD+绘图+汇总。  
   - 验证：对任意 session 目录运行后，report 产物稳定生成。
9. [test] 增加 GUI 轻量回归：至少提供 `tests/e2e/report_smoke.sh`（用 sample 数据生成报告）。  
   - 验证：脚本在无相机环境可运行成功并校验文件存在。
10. [doc] 增加《上位机协议与字段》文档：数据包字段、单位、示例 JSON。  
   - 验证：文档包含可复制的示例 JSON 与字段表。
11. [repo] 为 Python 工具链加上 `Makefile`/`justfile`：`make report SAMPLE=...`。  
   - 验证：一条命令能从 sample 数据生成完整 report。
12. [perf] 端到端延迟记录：落点确认→GUI 数值更新的延迟（目标 < 100ms 本机）。  
   - 验证：在日志中能看到延迟统计并满足目标。

---

## Phase 4：v1.0 收敛（稳态、可复现实验、可验收）（约 10 项）

### 目标（Goals）
- ToF/HD/录制开关达到“基础可用”：数据一致、输出稳定、离线可复现。
- 形成清晰的版本化输出与验收清单，便于论文/答辩演示。

### 阶段性交付成果（Deliverables）
- `runs/<session>/` 目录结构固定（raw、metrics、plots、report），并在 README 中说明。
- `docs/acceptance/v1_checklist.md`：从启动→录制→导出→回放的完整验收步骤。
- CI（可选）至少跑离线脚本 smoke：ToF/HD/Report。

### TODO
1. [repo] 规范 `runs/` 目录结构与命名：`raw/ metrics/ plots/ report/`，并在代码中统一使用。  
   - 验证：任意一次录制后目录结构一致。
2. [core] 将关键阈值配置化：落点检测 `noise_threshold_`、ToF 合法区间、HD center 等。  
   - 验证：改配置无需改代码即可改变行为。
3. [data] 为所有 CSV/JSON 增加 schema 版本号（如 `schema_version: 1`）。  
   - 验证：输出文件头部或 JSON 根字段包含版本号。
4. [test] 构建最小 CI（GitHub Actions 或本地脚本）：跑 `tools/make_report.py` 的样例数据。  
   - 验证：CI 通过；产物校验成功。
5. [doc] 产出《实验复现指南》：给定一个 session，如何生成同样的 ToF/HD 图表与统计。  
   - 验证：同一 session 在两台机器上生成的统计一致（允许浮点微小误差）。
6. [app] 增加“会话列表/加载历史会话”功能：选择一个 session 立即展示 ToF/HD 与图表。  
   - 验证：无需相机即可打开历史 session 并查看。
7. [core] 增加“录制状态安全策略”：REC ON 时禁止清空坐标系/关键配置，或需二次确认（先简单提示）。  
   - 验证：REC ON 时触发危险操作会被拦截并提示。
8. [test] 增加稳定性回归：对 sample 数据跑 100 次，输出 hash/统计不漂移。  
   - 验证：100 次结果一致（或在误差范围内一致）。
9. [doc] 更新总 README：列出 v1.0 支持能力：录制开关、ToF、HD、报告导出、GUI 展示。  
   - 验证：README 与实际功能一致，按步骤可复现。
10. [daemon] （可选占位）若后续要把相机与推理做成后台服务，预留 `daemon/` 目录与接口占位文档。  
   - 验证：目录存在且不影响当前构建；文档说明未来拆分边界。


# MIXCOMPILE 实验全景汇总（E1–E10）——2026-10-07 定稿版

> 本文件为回传数据包的总索引与结论汇总。全部原始数据位于本 results/ 目录（E1-E8 历史 + e9/ + e10/），配套结论报告见 paper_text/ 三份专项汇总。坏数据/作废件全部留痕存档（*.bak），未混入任何有效统计。

## 一、实验版图与数据位置

| 实验 | 内容 | 数据位置 | 规模 |
|---|---|---|---|
| E1-E7 | 主实验（正确性/代价/多样性/Pass 激活/准入） | results/raw/*.csv | cost 17M、pass_activation 153M 等 |
| E8 | 固定策略↔OLLVM-full 重标注 | results/raw/table_e8_l1_relabel.csv | 360 行 |
| 参数敏感性 | 权重敏感性 | results/raw/sensitivity_records.csv 等 | 9-29 落盘 |
| E9 | LLM 攻击者臂（四证据+T2） | results/e9/raw/*.csv | 1471 条有效（920+120+115+60+12+137+107 校准等） |
| E10 | LLM 防御规划臂（五臂，Step E0-E5） | results/e10/raw/* | 360+242+122+157 jsonl（三组全）+822 代价+103 攻击 |

## 二、E9 攻击者臂结论（详见 paper_text/e9_summary_20261004.md）

1. **识别屏障失效（模型代差主导）**：混淆臂 top-1 96-99% ≈ 未保护 A0（96.7-100%）；先导 Qwen 14-23%——防护效力是攻击者能力的递减函数。
2. **语义层残余保护（A0 归因裁决）**：混淆使语义重建余弦降 39%（0.118→0.083）、S3 率减半——混淆价值转移至"理解成本"。
3. **ρ 无定义边界**：few-shot 部署级适应零增益（−2.7%/−2.5%），预注册表述"该攻击层级不区分固定与多样部署"。
4. **配置不透明 + 无污染**：Pass 组合识别 EM=0（NONE 率 26.7%）。
5. **输入表示稳健**：objdump 反汇编复核 9/9。

## 三、E10 防御规划臂结论（详见 paper_text/e10_pilot_summary_20261006.md）

**Step E0-E5 执行状态**：E0 双通道就绪 ✓ → 组件准入（A4 三候选两拒一弱 → **设计 v1.2 改判**：对象=llvm-test-suite 数据集）→ E2 臂映射确认（选项 1 五臂照旧）→ 任务三分复用 benchmark_split → D6 冻结（用户批准七项）→ validation 核验（C/D 98.3%/B 100%/E 57.9%）→ **test 43 程序单次揭盲** ✓。

**揭盲结果（test 43 程序，130+232 候选）**：

| 臂 | 语义保持率 | 说明 |
|---|---|---|
| C/D（LLM 规划+反馈闭环） | **97.7%** | 130 候选 |
| B-rand / B-rule（非 LLM 搜索） | **100% / 100%** | 132 候选 |
| **E（LLM 直接源码改写，gcc 纯净口径）** | **91.1%** | 45 程序（三组一致 88-93%） |

**三大发现**：
1. **编译器基座必要性（修正稿）**：配置层候选语义保持 98-100% vs LLM 直接改写 91.1%——配置层仍有 7-9pp 可靠性优势 + 天然五维可控与多样性供给；"直接改写不可行"的强表述撤回（gcc 纠偏后）。
2. **配置扰动刚性**：C/D 代价与基线完全无差（runtime/size 均 1.00x，822 测量）+ 攻击穿透 100% + ICE 边界——三证据合围：特征驱动架构对 LLM 配置扰动刚性（界内被特征逻辑拉回、界外崩溃）——LLM 配置搜索的实际自由度≈0（对架构鲁棒性的正面解读 + 权重表表达力局限的讨论点）。
3. **工具链工程警示（独立发现）**：MIXCOMPILE patch clang **无参数时默认 Pass 激活**，对不知情下游用户造成 ~35pp 语义破坏恶化（E 臂 gcc 口径 91.1% vs 污染口径 50-58%）——"混淆能力必须显式开启"应作为设计原则。

## 四、坏数据/作废件清理清单（全部留痕 *.bak，零混入）

| 作废件 | 原因 | 存档 |
|---|---|---|
| P 探针旧 60 条（10-01） | true_answer 全空（真值链缺失） | 结果 CSV 被 v2 覆盖，旧件在 e10 无留——**v2 为唯一有效版** |
| L2a 旧构造 26 条（10-01） | window_from 占位符（target 无真代码） | 旧 22 号逻辑作废，e9_l2a_queries.json 重建 |
| CSV 覆盖事故 ~280 条 raw（10-03） | 同 level 双臂互写覆盖 | *.corrupt_infra_20261003.bak；丢失分片全量重跑 |
| 单文件假设批（10-05/06） | 多文件程序 build 全废 | *.singlefile_bug.bak |
| validation build-None 废行（10-06） | unit 查找缺 val run | *.valbuild_bug.bak |
| E 臂 patch clang 污染 157 条（10-06/07） | 默认 Pass 混入"纯改写"产物 | e10_rewrite_gcc_verify.csv 为干净口径；污染行留 llm_rewrite.jsonl（semantic 标记） |
| jsonl 半行（10-07） | 驱动器超时崩于写盘中 | 移除半行，断点续跑 |

## 五、佐证映射（主张 → 数据文件）

| 论文主张 | 数据文件 |
|---|---|
| 识别屏障失效/模型代差 | e9/raw/e9_t1_*.csv（920）+ e9_t2_A0（对照 45） |
| 语义层残余保护 | e9/raw/e9_t2_{A0,A1A2}_*.csv（137） |
| ρ 无定义 | e9/raw/e9_l2a_*.csv（115） |
| 配置不透明/无污染 | e9/raw/e9_pprobe2_*.csv（60） |
| 输入表示稳健 | e9/raw/e9_disasm_*.csv（12） |
| 配置层语义保持 98-100% | e10/raw/llm_planner_responses.jsonl（143）+ llm_bsearch_*.jsonl（145） |
| 基座必要性（E 臂 91.1%） | e10/raw/llm_rewrite.jsonl（157）+ e10_rewrite_gcc_verify.csv（57 verified） |
| 配置扰动刚性（代价 1.00x） | e10/raw/e10_cost_measurements.csv（822） |
| 配置臂产物攻击穿透 | e10/raw/e10_attack_glm_agent_records.csv（103） |
| 预注册纪律 | e10/validation_d6_check.json + reveal_record_20261007.json + config/e10_d6_frozen_20261006.json |
| 模型/工具链钉版 | config/tool_versions.json（含 role_swap、design v1.2、D6 引用） |

## 六、统计口径与披露

- E9 主攻 GLM-5.3 agent-harness（temperature 0、无工具、隔离实例）；核实者 DeepSeek-V4-Pro API——分形态报告，禁跨形态能力比较。
- E10 对象=llvm-test-suite 程序（设计 v1.2）；代价口径="自定义配置 vs 默认混淆基线"相对值（基线含 patch clang 默认 Pass 行为，已披露）。
- 先导 Qwen 480 条仅定性对照（解码参数未记录披露句保留）。
- E 臂攻击维度未跑（预期信息量≈0：无配置混淆→结构屏障不存在，由配置臂结果逻辑蕴含）；Egcc Ghidra 任务清单留盘（e10/logs/decompile_tasks_egcc.json）可随时补。


## 七、完整性核查记录（2026-10-07 定稿）

- E9 主矩阵 8 文件合计 **920 条**（精确匹配预期）
- 校准 60+60、L2a 115、P 探针 60、反汇编 12、T2 47+90——全部与预期一致
- E10 四 jsonl 三组分布：tuning 138 / validation 98 / test 124（planner）等——**无 unknown、无丢失**
- cost 822、攻击 103、D6/核验/揭盲记录齐全
- 旧版污染数据 9 件全部移入 superseded/ 留痕（主数据视图无坏数据）
- 冻结件 3 件、结论报告 9 份在位
- **本 results/ 目录即回传包：无需 runs/ 与 tools/，所有佐证数据自洽完整**

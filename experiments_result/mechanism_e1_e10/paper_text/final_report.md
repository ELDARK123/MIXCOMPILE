# MIXCOMPILE optimized 实验证据汇总（generated 2026-10-01T10:21:51Z）

## 配置与哈希
- 编译器（主实验）: `/home/vincent/Desktop/llvm-project-17.0.6.src/build/bin/clang` sha256 `fe7f9bb8f584060e…`
- 编译器（E6 专用）: sha256 `8c98b809c69afc8d…`（新增默认关闭的 -mix-shuffle-features；默认行为已验证不变）
- optimized 权重: `/home/vincent/Desktop/llvm-project-17.0.6.src/build/bin/clang` 冻结 sha256 `8b23633cef3c…`（来源 cybersecurity 2026-07-21 冻结副本）
- LLVM 参照: 同编译器树，无任何 -mix-* 旗标
- 数据划分: split_seed=20260903, tuning 44 / validation 43 / test 43 程序（程序级互斥），security_subset 12

## 一、已完成证据
### E1 权重基准（test, 35 候选 × 43 × 30）
- optimized（7/21 冻结权重）: test 通过率 1.0，exec 中位 1.0444×，激活中位 27.0（balanced 20.0）
- validation 选型（预注册硬约束 + exec≤1.6× 下最大激活）选定 7/21 冻结 optimized
- 非劣性: 无随机候选在 exec≤1.6× 达到 ≥21 激活；optimized 支配 28/30 随机
- dc 更正: 早期报告的 dc 语义失败为并发测量事故，隔离修复后重测 36 候选 × 30 种子全通过（0 失败）
### E2 五维消融（43 程序）
- 移除 security: exec 0.9990×, 尺寸 1.0×, 激活Δ -9.0, 归零程序 6
- 移除 diversity: exec 1.0×, 尺寸 1.0×, 激活Δ 1.0, 归零程序 1
- 移除 runtime: exec 1.0719×, 尺寸 1.2459×, 激活Δ 15.0, 归零程序 0
- 移除 size: exec 1.1343×, 尺寸 1.7009×, 激活Δ 17.0, 归零程序 0
- 移除 risk: exec 1.0798×, 尺寸 1.4878×, 激活Δ 17.0, 归零程序 0

### E3 敏感性（validation, 61 配置）
- 单维扰动选择 Jaccard 均值 0.888
- 联合扰动均值 0.832
→ 主要结论方向在 ±30% 扰动下稳定
### E4 单 Pass 标定（44 程序 × 10 种子）
- SUB: 激活 7362 次/81.1% 单元, exec 0.8065×
- BCF: 激活 2060 次/78.3% 单元, exec 1.0415×
- FLA: 激活 208 次/17.8% 单元, exec 1.0265×
- SPLIT: 激活 5164 次/100.0% 单元, exec 1.0241×
- IBR: 激活 480 次/30.6% 单元, exec 1.0203×
- ICALL: 激活 2528 次/65.0% 单元, exec 0.7792×
- IGV: 激活 2798 次/74.0% 单元, exec 1.0152×

- FLA 激活受边界随机支配（17.8% 单元），如实报告；证据弱于其他 Pass
### E7 多样性（12 样本 × 30 种子）
- optimized .text 唯一率 1.0000；radiff2(llvm vs optimized seed1) 逐程序中位 0.393 / 均值 0.448（分布 0.154–0.912，n=12）；opcode 3-gram Jaccard ≈ 0.765
### T5 三臂同表（security_subset 12 程序 × 30 种子，D3 预注册 2026-09-28）
| 臂 | .text 唯一率 | radiff2 中位(vs llvm) | opcode 3-gram Jaccard | 构建失败 |
|---|---|---|---|---|
| optimized | 1.0 | 0.393 | 0.765 | 0 |
| balanced | 1.0 | 0.480 | 0.733 | 0 |
| fixed_all（全 7 Pass, -mix-mode=all） | 1.0 | **0.325** | 0.736 | 0 |
| random_pass（-mix-mode=random） | 1.0 | 0.547 | 0.763 | 7（voronoi ICE 家族） |

任务卡四个区分：① fixed_all 换 seed 的 .text 唯一率 1.0——seed 随机机制（BCF budget 等）
单独即产生二进制级多样性；② 策略增量：三臂对 llvm 的 radiff2 中位 fixed_all 0.325 <
optimized 0.393 < balanced 0.480 < random_pass 0.547——fixed_all 变换最激进，optimized
以更低代价获得次强离散（结合 E4 代价标定）；③ E6 shuffled 已单列；④ 同预算（同编译次数
720）下 fixed_all 每次编译的多样性增益最高但代价最高（E4 标定 BCF p95 4.79×叠加其余），
"固定全开代价不可行"为 optimized 代价感知的正面证据（D3 风险条款）。
### T1/T2 任务卡结论（E9 前提判定，2026-09-29）
- **T1 跨程序组合离散度：趋同**——众数组合距离均值 0.265 ≪ E6 噪声底 0.850，
  组合种类仅 7 种（12 程序 × 30 种子去重）→ "分布错配/移动目标"前提受损，
  E9 的 ρ 无论方向都须条件化表述（表：table_cross_program_dispersion.csv）
- **T2 策略唯一率 mean=0.058**（min 0.033/max 0.100）——seed 随机机制（NearBoundary/
  BCF budget）贡献弱：94.2% 组合逐种子重复；与 E6 噪声底交叉验证一致 → L2b 条件启用
- T4 确定性：同 seed 双构建 CF_DECISION 逐行一致 + 未strip/strip/.text 三层哈希一致
  （复现纪律实证闭环，详见 t4_seed_determinism.md）
### E6 特征贡献（4 条件 × 43 × 30, 统一 E6 编译器）
- shuffled vs complete: 选择 Jaccard 0.8405385135966531 vs 自身噪声底 0.850120585701981；激活 26.5 vs 27.0
### 阶段5: Ghidra 恢复与攻击（security_subset 12 程序）
- 函数恢复 F1: llvm 0.903 / optimized 0.708 / balanced 0.712
- 函数定位攻击 top-1: optimized 0.163 / balanced 0.177（未混淆 ≈1.0）
### E9 API 副攻（Qwen2.5-Coder-14B 本地, 480 记录全 ok, 表 table_e9_llm_by_arm.csv）
| 臂 | LLM top-1 定位率 | 逐样本范围 |
|---|---|---|
| optimized | 0.158 | 0.00–0.80 |
| balanced | 0.217 | 0.00–0.80 |
| fixed_all | 0.142 | 0.00–0.80 |
| random_pass | 0.233 | 0.00–0.80 |
- **ρ 分支判定（按 02 v1.1 冻结表）: ρ 高分支适用**——攻击程序可跨程序迁移
  （12-IOtest 等样本在所有臂上同样高成功，程序结构驱动成功率而非臂差异）
  → 移动目标主张**限定到变体级**（L2b），策略级移动目标不写
- LLM 副攻（15.8–23.3%）未显著超越简单特征攻击（16.3%/17.7%）→
  "攻击者无需 LLM，手动特征已足"是诚实边界
- 四臂均把定位率从平凡（≈1.0）压到 0.14–0.23 → 混淆本身的分析阻力成立

## 二、未完成实验
- 差分随机输入测试: 样本无输入生成器，记 differential_not_available
- 反编译失败率/关键 API 恢复率等更细 Ghidra 指标: v1 仅函数恢复+计时
- E7 的 Random-Pass 与固定策略换 seed 的全部 30 种子对照: 以 balanced 作独立对照

## 三、不可复用旧结果
- 全部 2026-09 之前的历史 CSV（experiments_mixcompile* 各目录）: unverified（无 manifest/哈希链），不得进入新统计
- 两版 optimized_pass_weights.json（6/4 d3c6a203… 与 7/21 8b23633cef3c…）并存：旧结果混用 balanced profile + 不一致权重，全部作废
- 历史 "optimized" 均为 `-mix-profile=balanced` + cost-config，非全局 optimized profile

## 四、支持的论断（附证据）
1. optimized 在冻结数据集上经验有效且稳定（E1 test 非劣于 28/30 随机 + E3 稳定 + 通过率 1.0）
2. 五维权重各有可解释作用（E2: security 驱动、runtime/size/risk 制动、diversity 组合层）
3. 同源变体具有可重复结构多样性（E7: .text 唯一率 100%、radiff2 0.19）
4. 分析阻力增加的有限结论（Ghidra F1 0.90→0.71；函数定位 top-1 从平凡降至 ~16-18%，optimized 不劣于 balanced）
5. 工程成本可控（test exec 中位 1.06×、尺寸 1.49×，全部通过率 1.0）

## 五、不支持的论断（必须降级或删除）
1. **"特征驱动的逐函数选择"强主张不成立**: E6 shuffled-feature 与 complete 无法区分（Jaccard 0.8405385135966531 ≈ 噪声底 0.850120585701981）→ 改为"权重驱动的分布级选择；逐函数特征自适应仅有微弱边际贡献"
2. **不可称"权重最优/全局最优"**: 仅在冻结数据集+约束+搜索范围内经验有效
3. **单工具耗时不能作安全证明**: Ghidra/攻击成功率需同时报告（本报告已同时给出）
4. **FLA 的单 Pass 证据不足**: 激活受边界随机支配（17.8% 单元），不能声称 FLA 收益已被充分标定
5. **编译器 ICE 风险（随机权重）**: voronoi 上 4 个随机权重触发编译器 ICE 53 例——随机权重可致构建崩溃，optimized 未触发。NPB dc 的"10/30 种子语义失败"已更正为测量事故（共享 scratch 并发踩踏；修复后 36 候选 × 30 种子全通过，详见 dc_UB_investigation 报告）
6. 二进制相似度/NCD/radiff2 单独不构成安全证明（仅作补充指标）

## 六、复现实验命令
```
# 环境: PROJECT_ROOT=/home/vincent/Desktop/experiments-MIXCOMPILE, RESULT=$PROJECT_ROOT/result
# 工具发现与审计
python3 $RESULT/scripts/00_discover_tools.py --project-root $PROJECT_ROOT --output-root $RESULT
python3 $RESULT/scripts/01_inventory_sources.py --project-root $PROJECT_ROOT --output-root $RESULT
# 测试集获取（官方 llvmorg-17.0.6 tag）与准入/划分（G1-G7, split_seed=20260903）
python3 $RESULT/scripts/02_acquire_benchmarks.py --project-root $PROJECT_ROOT --output-root $RESULT
python3 $RESULT/scripts/03_admit_and_split.py --project-root $PROJECT_ROOT --output-root $RESULT
# 权重候选与矩阵冻结（smoke/tuning/validation/test; E6: 04b_freeze_e6.py）
python3 $RESULT/scripts/04_freeze_configs.py --project-root $PROJECT_ROOT --split $RESULT/config/benchmark_split.json --output-root $RESULT --stage <stage>
# 执行（--resume 断点续跑；--affinity-only 限定有状态程序串行）
python3 $RESULT/scripts/12_run_matrix.py --matrix $RESULT/config/experiment_matrix_<stage>.json --run-root $RESULT/runs/<run> --jobs 16
# 统计/完整性/多样性/Ghidra/攻击/real_world
python3 $RESULT/scripts/13_analyze_statistics.py --run-root $RESULT/runs/<run> --experiments E1,E4,E5 --output-root $RESULT
python3 $RESULT/scripts/14_verify_integrity.py --run-root $RESULT/runs/<run> --matrix <matrix.json> --output-root $RESULT
python3 $RESULT/scripts/09_measure_diversity.py --run-root $RESULT/runs/test_20260912T090000Z --output-root $RESULT
python3 $RESULT/scripts/10_run_ghidra.py --run-root $RESULT/runs/test_20260912T090000Z --output-root $RESULT
python3 $RESULT/scripts/11_run_attack.py --run-root $RESULT/runs/test_20260912T090000Z --output-root $RESULT
python3 $RESULT/scripts/18_realworld.py --output-root $RESULT
```

## 七、必须披露的测量边界
1. 成本测量在 16 路并行负载下进行（所有候选同一 regime，横向比较内部有效；绝对值可能偏高）
2. dc 并发踩踏事件（2026-09-14）：论文披露句（按 D2 条款，置于复现/边界段、不进主结果表）——
   "NPB dc 的初轮语义失败经调查为评测测量事故（共享工作目录下同程序并发实例互踩运行期状态文件），
   根因修复后 36 个权重配置 × 30 种子语义全部通过；原始失败记录保留归档未删除。"
3. voronoi 编译器 ICE：random_008/009/019/029 权重 53 单元确定性崩溃（真实鲁棒性发现）
4. E6 与主实验使用不同编译器哈希（8c98b809 vs fe7f9bb8），E6 表内四条件同哈希可比
5. FLA 激活受边界随机影响（PassDecider NearBoundary [2.0,3.0)），统计口径已按单元占比/均值/程序中位三列呈现


---

# 不支持的论断清单（generated 2026-10-01T10:21:51Z）

| # | 论断 | 状态 | 证据 |
|---|---|---|---|
| 1 | 逐函数特征自适应选择带来实质贡献 | **不支持（降级）** | E6 shuffled≈complete（Jaccard 0.8405385135966531≈噪声底 0.850120585701981，激活 26.5 vs 27.0） |
| 2 | optimized 权重最优/全局最优 | 不支持（范围限定） | E1 仅在冻结集+约束内经验有效；30 随机中存在同域候选 |
| 3 | FLA 单 Pass 收益充分标定 | 证据不足 | 激活集中于 17.8% 单元（边界随机支配） |
| 4 | 混淆不引入正确性风险 | 不支持 | dc 10/30 种子语义失败；voronoi ICE 53 例（随机权重） |
| 5 | 安全性由耗时/相似度单指标证明 | 不支持 | 本报告同时给出恢复 F1 与攻击成功率 |
| 6 | 历史 optimized 结果可复用 | 不支持 | 无哈希链；混用 balanced profile；两版权重并存 |

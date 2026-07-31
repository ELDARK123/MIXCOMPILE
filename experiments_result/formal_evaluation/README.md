# MIXCOMPILE 正式实验结果归档

本目录集中保存 llama.cpp、OpenSSL 和 LLVM test-suite 的正式实验结果及复现所需的关键配置。文件从对应的 `runs/` 目录复制而来，原始运行目录保持不变，并仍作为完整日志和中间产物的权威来源。

## 目录结构

```text
result/
├── README.md                 # 本说明
├── index.json                # 来源批次和主要文件的机器可读索引
├── llama/
│   ├── tables/               # 构建、二进制和 llama-bench 汇总表
│   ├── raw/                  # llama-bench 每次测量数据
│   ├── config/               # 实验参数、权重、路径和执行脚本快照
│   └── manifests/            # 三个来源批次的运行清单
├── openssl/
│   ├── tables/               # 五方构建及吞吐量汇总表
│   ├── raw/                  # openssl speed 每次测量数据
│   ├── config/               # 实验参数、权重、路径和执行脚本快照
│   └── manifests/            # 运行清单
└── llvm-test-suite/
    ├── tables/               # 静态验证、radiff2 和 Ghidra 汇总表
    ├── raw/                  # 静态体积、radiff2、Ghidra 明细及调度
    ├── config/               # 构建、样本划分、权重和 Ghidra 配置
    └── manifests/            # 构建、正确性和运行环境清单
```

## llama.cpp

主要结果位于 `llama/tables/`。`baselines_*` 包含 GCC、LLVM 和 balanced，`optimized_*` 包含 optimized，`ollvm_full_timeout_*` 如实记录 OLLVM-full 构建超时。

- 性能工具：`llama-bench`
- 模型：`Mistral-7B-Instruct-v0.3-Q4_K_M.gguf`
- 模型 SHA-256：`1270d22c0fbb3d092fb725d4d96c457b7b687a5f5a715abe1e818da303e562b6`
- 后端：CPU，6 线程，GPU layers 为 0
- 场景：`pp512` 和 `tg128`
- 每个场景：3 次预热，20 次正式测量
- OLLVM-full：构建 10800.139960 秒后按历史阈值记为超时，未生成 `llama-bench`，不补算或推测运行性能

复现时先检查 `llama/config/*_paths.local.json` 中的本机路径，再使用相应实验 JSON、权重文件和 `run_formal_llama.py`。运行环境及工具链哈希见 `llama/manifests/`。

## OpenSSL

`openssl/tables/summary.csv` 是五方吞吐量汇总，`openssl/raw/speed_repetitions.csv` 保存每次正式测量。

- 对比项：GCC、LLVM、balanced、optimized、OLLVM-full
- OpenSSL：3.6.1，`no-asm`、静态、无 module
- 算法：AES-128-CBC、ChaCha20-Poly1305、SHA-512
- 块大小：16、64、256、1024、8192、16384 字节
- 每个组合：3 次预热，20 次 1 秒正式测量

复现参数见 `openssl/config/experiment.json`，pass 权重分别见 baseline 和 optimized 权重文件；工具链及环境哈希见 `openssl/manifests/run_manifest.json`。

## LLVM test-suite

本归档只保留用户指定的最终指标：静态体积、radiff2 和 Ghidra。此前未完成的 runtime 测量没有复制到 `result/`，也不应作为正式结论使用。

主要文件：

- `tables/radiff2_summary.csv`：5 个编译器版本全部 10 个无向两两组合的相似度汇总
- `raw/radiff2_pairs.csv`：10 个组合共 306 条逐样本 radiff2 明细
- `raw/static_per_sample.csv`：GCC、LLVM、balanced、optimized 的逐样本静态体积
- `raw/ollvm_static_per_sample.csv`：OLLVM-full 支持样本的静态体积
- `tables/ghidra_by_sample.csv`：逐编译器、逐样本 Ghidra 汇总
- `tables/ghidra_ratios.csv`：反编译耗时相对 LLVM 的比值
- `raw/ghidra_runs.csv`：75 次 Ghidra 分析明细
- `raw/ghidra_input_binary_manifest.csv`：Ghidra 输入二进制及哈希
- `raw/ghidra_schedule.json`：随机化分析顺序

Ghidra 使用 5 个样本、5 个编译器版本和 3 次重复，共 75 次分析；75 次均完成并成功解析。该指标是受控的自动化逆向成本代理，不是密码学安全证明。

radiff2 两两比较覆盖 GCC、LLVM、balanced、optimized 和 OLLVM-full，共 `C(5,2)=10` 个组合。普通四方每组计划 31 个样本；OLLVM-full 因 Burg 不在支持构建集合中，每个相关组合计划 30 个样本。汇总表同时保留成功数、超时数、正确性合格成功数以及两种中位数，分析时不应忽略超时和正确性资格。

`tables/builds.csv`、正确性表和 `manifests/` 用于证明输入二进制的构建与正确性状态，不属于额外的 runtime 实验结果。OLLVM-full 的 Burg 构建异常及个别输出验证失败保留在相应表和 manifest 中，使用结果时必须结合 eligibility/status 字段筛选。

## 数据使用约定

1. 论文汇总优先读取各实验的 `tables/`。
2. 统计复核读取 `raw/`，不要从显示精度较低的汇总表反推原始值。
3. 复现前调整 `paths.local.json` 中的绝对路径，并核对 manifest 中编译器、模型和配置哈希。
4. `runs/` 保存完整日志、中间文件及原始上下文；本目录是面向论文与复现的精简归档。
5. LLVM test-suite runtime 已明确排除，不得与 llama.cpp 或 OpenSSL 的运行性能数据混用。

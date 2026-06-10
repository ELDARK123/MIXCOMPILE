# MIXCOMPILE — 特征驱动的 LLVM 混淆策略选择器

MIXCOMPILE 是一个基于 LLVM 17.0.6 的编译器混淆工具，通过代价模型（Cost Model）自动选择并应用适当的混淆策略，在**正确性**、**性能开销**和**安全收益**之间取得平衡。

---

## 目录

- [MIXCOMPILE — 特征驱动的 LLVM 混淆策略选择器](#mixcompile--特征驱动的-llvm-混淆策略选择器)
  - [目录](#目录)
  - [1. 概述](#1-概述)
  - [2. 编译与安装](#2-编译与安装)
  - [3. 快速开始](#3-快速开始)
  - [4. 命令行选项](#4-命令行选项)
    - [4.1 基础选项](#41-基础选项)
    - [4.2 代价模型选项](#42-代价模型选项)
    - [4.3 权重与配置选项](#43-权重与配置选项)
    - [4.4 实验与调试选项](#44-实验与调试选项)
  - [5. 混淆策略](#5-混淆策略)
  - [6. 代价模型](#6-代价模型)
    - [6.1 评分公式](#61-评分公式)
    - [6.2 Profile 权重](#62-profile-权重)
    - [6.3 Pass 基础权重](#63-pass-基础权重)
    - [6.4 特征自适应调整](#64-特征自适应调整)
  - [7. 运行模式](#7-运行模式)
  - [8. 权重自定义](#8-权重自定义)
    - [8.1 JSON 权重配置文件格式](#81-json-权重配置文件格式)
    - [8.2 维度缩放](#82-维度缩放)
    - [8.3 维度消融](#83-维度消融)
  - [9. 三种 Profile 对比](#9-三种-profile-对比)
  - [10. 实验验证结果摘要](#10-实验验证结果摘要)
    - [10.1 参数敏感性](#101-参数敏感性)
    - [10.2 维度消融](#102-维度消融)
    - [10.3 Pass-only 趋势一致性](#103-pass-only-趋势一致性)
    - [10.4 Ghidra 反编译时间](#104-ghidra-反编译时间)
    - [10.5 LLVM 归一化优化权重](#105-llvm-归一化优化权重)
  - [11. 使用 LLVM test-suite 进行测试](#11-使用-llvm-test-suite-进行测试)
  - [12. 实验目录说明](#12-实验目录说明)
  - [13. 常见问题](#13-常见问题)

---

## 1. 概述

MIXCOMPILE 在 LLVM 编译管线中嵌入了一个决策模块 `PassDecider`，该模块分析每个函数的**代码特征**（如圈复杂度、条件跳转比、指令替换率、BB 平均深度、IBR 兼容性等），基于**代价模型**计算每个混淆策略的得分，并自动选择最优策略组合。

与 OLLVM 等传统混淆工具不同，MIXCOMPILE 不盲目应用所有混淆策略，而是针对每个函数进行**特征驱动的选择性混淆**，从而在提升安全性的同时控制性能和代码膨胀开销。

## 2. 编译与安装

MIXCOMPILE 基于 LLVM 17.0.6 构建。假设源码位于 `~/llvm-project-17.0.6.src`：

```bash
cd ~/llvm-project-17.0.6.src
mkdir -p build && cd build

cmake -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DLLVM_ENABLE_PROJECTS="clang;compiler-rt" \
  -DLLVM_TARGETS_TO_BUILD="X86" \
  ../llvm

ninja -j$(nproc) clang
```

编译完成后，MIXCOMPILE 的 clang 位于 `build/bin/clang`。

> **注意**：MIXCOMPILE 的 PassDecider 代码位于 `llvm/lib/Passes/Obfuscation/PassDecider.cpp`。如果修改了该文件，只需重新编译 clang 目标即可：
> ```bash
> ninja -j$(nproc) clang
> ```

## 3. 快速开始

**基础用法**（启用所有混淆策略，使用默认 balanced profile）：

```bash
clang -mllvm -pd \
  -mllvm -ibr -mllvm -icall \
  -mllvm -fla -mllvm -split \
  -mllvm -bcf -mllvm -igv \
  -mllvm -sub \
  -O2 -o output_program source.c
```

**使用 security profile**（更激进的安全策略）：

```bash
clang -mllvm -pd \
  -mllvm -ibr -mllvm -icall \
  -mllvm -fla -mllvm -split \
  -mllvm -bcf -mllvm -igv \
  -mllvm -sub \
  -mllvm -mix-profile=security \
  -O2 -o output_program source.c
```

**只启用单一 Pass 进行实验**：

```bash
clang [所有 pass 标志] \
  -mllvm -mix-only-pass=BCF \
  -O2 -o output_program source.c
```

**使用自定义权重文件**：

```bash
clang [所有 pass 标志] \
  -mllvm -mix-cost-config=my_weights.json \
  -mllvm -mix-dump-cost-config=used_weights.json \
  -O2 -o output_program source.c
```

## 4. 命令行选项

所有 MIXCOMPILE 选项通过 `-mllvm -<option>=<value>` 传递给 LLVM 后端。

### 4.1 基础选项

| 选项 | 默认值 | 说明 |
|------|--------|------|
| `-pd` | 无 | **必需。** 启用 PassDecider 模块。不加此标志则不执行任何混淆。 |
| `-mix-mode` | `cost` | 运行模式：`cost`（代价模型）、`rule`（规则+边界随机）、`random`（完全随机）、`all`（全部启用） |
| `-mix-profile` | `balanced` | Profile 选择：`balanced`、`security`、`performance` |
| `-mix-seed` | `0` | 随机种子（0 = 使用 random_device） |

### 4.2 代价模型选项

| 选项 | 默认值 | 说明 |
|------|--------|------|
| `-mix-score-threshold` | `0.0` | Pass 启用的最低分数阈值 |
| `-mix-security-scale` | `1.0` | SecurityGain 维度缩放因子 |
| `-mix-diversity-scale` | `1.0` | DiversityGain 维度缩放因子 |
| `-mix-runtime-scale` | `1.0` | RuntimeCost 维度缩放因子 |
| `-mix-size-scale` | `1.0` | SizeCost 维度缩放因子 |
| `-mix-risk-scale` | `1.0` | CorrectnessRisk 维度缩放因子 |

### 4.3 权重与配置选项

| 选项 | 默认值 | 说明 |
|------|--------|------|
| `-mix-cost-config` | `""` | 从 JSON 文件加载 Pass 基础权重（不指定则使用内置默认权重） |
| `-mix-dump-cost-config` | `""` | 将实际使用的权重导出到 JSON 文件 |
| `-mix-disable-dimension` | `""` | 禁用维度：`security`、`diversity`、`runtime`、`size`、`risk`（用于消融实验） |

### 4.4 实验与调试选项

| 选项 | 默认值 | 说明 |
|------|--------|------|
| `-mix-only-pass` | `""` | 只允许执行指定的单个 Pass：`SUB`、`BCF`、`FLA`、`SPLIT`、`IBR`、`ICALL`、`IGV`（用于 Pass-only 实验） |

## 5. 混淆策略

MIXCOMPILE 支持 7 种混淆策略，分为两类：

**函数级 Pass**（在 selectFunctionPasses 中决策，受冲突规则约束）：

| Pass | 全称 | 作用 | 函数级 |
|:----:|------|------|:-----:|
| IBR | Indirect Branch Pass | 将直接分支替换为间接分支 | ✓ |
| BCF | Bogus Control Flow | 插入虚假控制流（不透明谓词 + 不可达代码） | ✓ |
| FLA | Control Flow Flattening | 将 CFG 扁平化为 switch-case 结构 | ✓ |
| SPLIT | Basic Block Splitting | 将基本块拆分为多个子块 | ✓ |
| IGV | Indirect Global Variable | 将全局变量访问间接化 | ✓ |

**基本块级 Pass**（在基本块级别独立决策）：

| Pass | 全称 | 作用 |
|:----:|------|------|
| SUB | Instruction Substitution | 将可替换指令（Add、Sub、And、Or、Xor）替换为等价指令序列 |
| ICALL | Indirect Call | 将直接函数调用替换为间接调用 |

**冲突规则**：
- IBR 与 IGV 互斥
- SPLIT 与 ICALL 互斥（当函数已应用 SPLIT 时，ICALL 不在该函数的 BB 中执行）

**适用性过滤**：
- FLA：需要足够的圈复杂度和条件跳转比
- IBR：需要 IBR 兼容性检查通过
- IGV：需要函数不包含内联汇编

## 6. 代价模型

### 6.1 评分公式

```
Score(P, F) = Ws × SecurityGain(P, F) + Wd × DiversityGain(P, F)
             - Wr × RuntimeCost(P, F)  - Wz × SizeCost(P, F)
             - Wc × CorrectnessRisk(P, F)
```

其中：
- `Ws`、`Wd`、`Wr`、`Wz`、`Wc` 是 Profile 权重
- `SecurityGain(P, F)` 等是 Pass P 在函数 F 上的特征自适应评分（经过 clamp01 到 [0,1]）

**决策逻辑**：
- 当 `Score > Threshold` 且 Pass 未被冲突规则阻止且边界条件判断通过时，启用该 Pass
- 当函数处于边界条件（NearBoundary）时，使用 `choose_machine()` 随机选择

### 6.2 Profile 权重

| Profile | Ws（安全） | Wd（多样性） | Wr（运行时） | Wz（尺寸） | Wc（风险） | 适用场景 |
|---------|:---------:|:-----------:|:-----------:|:---------:|:---------:|----------|
| **balanced** | 1.00 | 0.50 | 0.80 | 0.50 | 1.00 | 默认：平衡安全性、开销和风险 |
| **security** | 1.40 | 0.70 | 0.55 | 0.35 | 0.90 | 激进：偏重安全收益，容忍更高开销 |
| **performance** | 0.80 | 0.35 | 1.40 | 0.90 | 1.20 | 保守：严格控制开销，适用性能敏感场景 |

### 6.3 Pass 基础权重

每个 Pass 在 5 个维度上有基础权重 `BaseWeight(P)`，表示在不考虑函数特征时的默认估值：

| Pass | SecurityGain | DiversityGain | RuntimeCost | SizeCost | CorrectnessRisk |
|:----:|:-----------:|:-------------:|:----------:|:--------:|:--------------:|
| SUB | 0.75 | 0.55 | 0.25 | 0.05 | 0.10 |
| BCF | 0.45 | 0.65 | 0.20 | 0.10 | 0.20 |
| FLA | **0.80** | **0.75** | **0.85** | **0.60** | 0.35 |
| SPLIT | 0.30 | 0.40 | 0.15 | 0.25 | 0.10 |
| IBR | 0.70 | 0.65 | 0.45 | 0.30 | **0.45** |
| ICALL | 0.55 | 0.50 | 0.35 | 0.20 | 0.30 |
| IGV | 0.55 | 0.50 | 0.30 | 0.35 | **0.50** |

**依据**：这些权重是通过在 Pass-only 实验中归一化得到的。

### 6.4 特征自适应调整

基础权重根据函数特征进行自适应调整。例如：

- **IBR.SecurityGain** × `clamp01(0.4 + CondJumpRatio)` — 条件跳转越多，间接分支的安全收益越高
- **FLA.RuntimeCost** × `1.0 + 0.5 × CyclomaticComplexity` — 圈复杂度越高，FLA 的开销越大
- **BCF.DiversityGain** × `1.0 + 0.3 × AvgCFGDepth` — CFG 越深，虚假控制流的多样性收益越高
- 各项调整后经过 `clamp01()` 约束到 [0, 1]

详细调整规则见 `PassDecider.cpp` 中的 `estimateFunctionPassScore()` 函数。

## 7. 运行模式

| 模式 | 说明 |
|------|------|
| **cost**（默认） | 标准代价模型决策。Score > Threshold 时启用，NearBoundary 时随机选择 |
| **rule** | 基于规则的决策。仅当不处于 NearBoundary 时启用，否则随机选择 |
| **random** | 完全随机选择。每个候选 Pass 以 50% 概率启用 |
| **all** | 启用所有适用的候选 Pass |

## 8. 权重自定义

### 8.1 JSON 权重配置文件格式

```json
{
  "SUB": {
    "SecurityGain": 0.70,
    "DiversityGain": 0.55,
    "RuntimeCost": 0.20,
    "SizeCost": 0.05,
    "CorrectnessRisk": 0.10
  },
  "BCF": {
    "SecurityGain": 0.60,
    "DiversityGain": 0.80,
    "RuntimeCost": 0.40,
    "SizeCost": 0.25,
    "CorrectnessRisk": 0.15
  },
  "FLA": { ... },
  "SPLIT": { ... },
  "IBR": { ... },
  "ICALL": { ... },
  "IGV": { ... }
}
```

使用方式：
```bash
clang [flags] -mllvm -mix-cost-config=my_weights.json -o prog source.c
```

使用 `-mix-dump-cost-config` 验证实际生效的权重：
```bash
clang [flags] \
  -mllvm -mix-cost-config=my_weights.json \
  -mllvm -mix-dump-cost-config=used.json \
   -o prog source.c
cat used.json
```

### 8.2 维度缩放

对 Profile 层的维度权重进行全局缩放，无需修改 JSON：

```bash
# 安全收益 × 1.2，运行时代价 × 0.8（接近 security profile）
-mllvm -mix-security-scale=1.2 -mllvm -mix-runtime-scale=0.8
```


## 9. 实验目录说明

| 目录 | 内容 |
|------|------|
| `experiments_mixcompile_param_sensitivity/` | 全局参数敏感性实验（±20% 扰动、维度消融、30 组随机采样、迁移验证） |
| `experiments_mixcompile_pass_level_and_ghidra/` | Pass 级参数合理性实验（7 Pass-only）+ Ghidra 反编译时间实验（balanced + security 对比） |
| `experiments_mixcompile_misc_full/` | Misc 完整数据集实验 |
| `experiments_mixcompile_final/` | 最终参数优化实验（JSON 权重加载、LLVM 归一化拟合、优化权重验证、最终 Ghidra 验证） |

实验指导文档：
- `MIXCOMPILE代价模型参数合理性实验指导文档.md` — 参数敏感性实验设计方案
- `MIXCOMPILE补充实验_Pass级参数与Ghidra反编译时间指导.md` — 补充实验设计方案
- `experiments_mixcompile_final/experiments_mixcompile_final.md` — 最终优化实验设计方案

## 13. 常见问题

**Q: 编译后程序无法运行（Segfault）？**

A: 使用 balanced 或 performance profile，避免过于激进的混淆。如果使用 security profile，建议在重要程序上先用 lit 验证正确性。

**Q: 如何确认哪些 Pass 被启用了？**

A: MIXCOMPILE 在 stderr 输出 annotation 信息。也可以通过 `-mix-dump-cost-config` 导出权重来确认参数是否正确加载。

**Q: 为什么 FLA（或 IBR、IGV）的注解数为 0？**

A: 这些 Pass 有严格的应用条件：
- FLA 需要函数具有较高的圈复杂度和条件跳转比
- IBR 需要 IBR 兼容性检查通过
- IGV 需要函数不包含内联汇编
在简单的测试程序上不触发是正常行为，不表明 Pass 无效。

**Q: 如何选择合适的 Profile？**

A: 从 balanced 开始，如果对性能有严格要求使用 performance，如果追求更高安全性（能容忍更大二进制和稍高风险）使用 security。

**Q: 可以自己调整单个 Pass 的权重吗？**

A: 可以。使用 `-mix-cost-config=weights.json` 提供自定义权重文件。格式见第 8.1 节。

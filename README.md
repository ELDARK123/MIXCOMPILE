# MIXCOMPILE — Feature-Driven LLVM Obfuscation Strategy Selector

MIXCOMPILE is an LLVM 17.0.6-based compiler obfuscation tool that automatically selects and applies appropriate obfuscation strategies through a Cost Model, balancing **correctness**, **performance overhead**, and **security benefits**.

---

## Table of Contents

- [MIXCOMPILE — Feature-Driven LLVM Obfuscation Strategy Selector](#mixcompile--feature-driven-llvm-obfuscation-strategy-selector)
  - [Table of Contents](#table-of-contents)
  - [1. Overview](#1-overview)
  - [2. Build and Installation](#2-build-and-installation)
  - [3. Quick Start](#3-quick-start)
  - [4. Command-Line Options](#4-command-line-options)
    - [4.1 Basic Options](#41-basic-options)
    - [4.2 Cost Model Options](#42-cost-model-options)
    - [4.3 Weight and Configuration Options](#43-weight-and-configuration-options)
    - [4.4 Experiment and Debug Options](#44-experiment-and-debug-options)
  - [5. Obfuscation Strategies](#5-obfuscation-strategies)
  - [6. Cost Model](#6-cost-model)
    - [6.1 Scoring Formula](#61-scoring-formula)
    - [6.2 Profile Weights](#62-profile-weights)
    - [6.3 Pass Base Weights](#63-pass-base-weights)
    - [6.4 Feature-Adaptive Adjustments](#64-feature-adaptive-adjustments)
  - [7. Operating Modes](#7-operating-modes)
  - [8. Custom Weights](#8-custom-weights)
    - [8.1 JSON Weight Configuration File Format](#81-json-weight-configuration-file-format)
    - [8.2 Dimension Scaling](#82-dimension-scaling)
    - [8.3 Dimension Ablation](#83-dimension-ablation)
  - [9. Three-Profile Comparison](#9-three-profile-comparison)
  - [10. Experimental Validation Summary](#10-experimental-validation-summary)
    - [10.1 Parameter Sensitivity](#101-parameter-sensitivity)
    - [10.2 Dimension Ablation](#102-dimension-ablation)
    - [10.3 Pass-only Trend Consistency](#103-pass-only-trend-consistency)
    - [10.4 Ghidra Decompilation Time](#104-ghidra-decompilation-time)
    - [10.5 LLVM Normalized Optimization Weights](#105-llvm-normalized-optimization-weights)
  - [11. Testing with LLVM test-suite](#11-testing-with-llvm-test-suite)
  - [12. Experiment Directory Structure](#12-experiment-directory-structure)
  - [13. FAQ](#13-faq)

---

## 1. Overview

MIXCOMPILE embeds a decision module `PassDecider` into the LLVM compilation pipeline. This module analyzes each function's **code features** (such as cyclomatic complexity, conditional jump ratio, instruction replacement rate, average basic block depth, IBR compatibility, etc.), computes a score for each obfuscation strategy based on a **Cost Model**, and automatically selects the optimal strategy combination.

Unlike traditional obfuscation tools such as OLLVM, MIXCOMPILE does not blindly apply all obfuscation strategies. Instead, it performs **feature-driven selective obfuscation** on a per-function basis, thereby improving security while controlling performance and code bloat overhead.

## 2. Build and Installation

MIXCOMPILE is built on LLVM 17.0.6. Assuming the source is located at `~/llvm-project-17.0.6.src`:

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

After compilation, MIXCOMPILE's clang is located at `build/bin/clang`.

> **Note**: MIXCOMPILE's PassDecider code is located at `llvm/lib/Passes/Obfuscation/PassDecider.cpp`. If this file is modified, only the clang target needs to be recompiled:
> ```bash
> ninja -j$(nproc) clang
> ```

## 3. Quick Start

**Basic usage** (enable all obfuscation strategies with the default balanced profile):

```bash
clang -mllvm -pd \
  -mllvm -ibr -mllvm -icall \
  -mllvm -fla -mllvm -split \
  -mllvm -bcf -mllvm -igv \
  -mllvm -sub \
  -O2 -o output_program source.c
```

**Using the security profile** (more aggressive security strategy):

```bash
clang -mllvm -pd \
  -mllvm -ibr -mllvm -icall \
  -mllvm -fla -mllvm -split \
  -mllvm -bcf -mllvm -igv \
  -mllvm -sub \
  -mllvm -mix-profile=security \
  -O2 -o output_program source.c
```

**Enabling a single pass for experiments**:

```bash
clang [all pass flags] \
  -mllvm -mix-only-pass=BCF \
  -O2 -o output_program source.c
```

**Using a custom weight file**:

```bash
clang [all pass flags] \
  -mllvm -mix-cost-config=my_weights.json \
  -mllvm -mix-dump-cost-config=used_weights.json \
  -O2 -o output_program source.c
```

## 4. Command-Line Options

All MIXCOMPILE options are passed to the LLVM backend via `-mllvm -<option>=<value>`.

### 4.1 Basic Options

| Option | Default | Description |
|--------|---------|-------------|
| `-pd` | N/A | **Required.** Enables the PassDecider module. No obfuscation is performed without this flag. |
| `-mix-mode` | `cost` | Operating mode: `cost` (cost model), `rule` (rule + boundary random), `random` (fully random), `all` (enable all) |
| `-mix-profile` | `balanced` | Profile selection: `balanced`, `security`, `performance` |
| `-mix-seed` | `0` | Random seed (0 = use random_device) |

### 4.2 Cost Model Options

| Option | Default | Description |
|--------|---------|-------------|
| `-mix-score-threshold` | `0.0` | Minimum score threshold for pass enabling |
| `-mix-security-scale` | `1.0` | SecurityGain dimension scaling factor |
| `-mix-diversity-scale` | `1.0` | DiversityGain dimension scaling factor |
| `-mix-runtime-scale` | `1.0` | RuntimeCost dimension scaling factor |
| `-mix-size-scale` | `1.0` | SizeCost dimension scaling factor |
| `-mix-risk-scale` | `1.0` | CorrectnessRisk dimension scaling factor |

### 4.3 Weight and Configuration Options

| Option | Default | Description |
|--------|---------|-------------|
| `-mix-cost-config` | `""` | Load pass base weights from a JSON file (built-in defaults are used if not specified) |
| `-mix-dump-cost-config` | `""` | Export the actually used weights to a JSON file |
| `-mix-disable-dimension` | `""` | Disable a dimension: `security`, `diversity`, `runtime`, `size`, `risk` (for ablation experiments) |

### 4.4 Experiment and Debug Options

| Option | Default | Description |
|--------|---------|-------------|
| `-mix-only-pass` | `""` | Allow only the specified single pass: `SUB`, `BCF`, `FLA`, `SPLIT`, `IBR`, `ICALL`, `IGV` (for pass-only experiments) |

## 5. Obfuscation Strategies

MIXCOMPILE supports 7 obfuscation strategies, divided into two categories:

**Function-level passes** (decided in selectFunctionPasses, subject to conflict rules):

| Pass | Full Name | Description | Function-level |
|:----:|-----------|-------------|:--------------:|
| IBR | Indirect Branch Pass | Replaces direct branches with indirect branches | ✓ |
| BCF | Bogus Control Flow | Inserts bogus control flow (opaque predicates + unreachable code) | ✓ |
| FLA | Control Flow Flattening | Flattens CFG into a switch-case structure | ✓ |
| SPLIT | Basic Block Splitting | Splits basic blocks into multiple sub-blocks | ✓ |
| IGV | Indirect Global Variable | Indirects global variable accesses | ✓ |

**Basic-block-level passes** (decided independently at the basic block level):

| Pass | Full Name | Description |
|:----:|-----------|-------------|
| SUB | Instruction Substitution | Replaces substitutable instructions (Add, Sub, And, Or, Xor) with equivalent instruction sequences |
| ICALL | Indirect Call | Replaces direct function calls with indirect calls |

**Conflict rules**:
- IBR and IGV are mutually exclusive
- SPLIT and ICALL are mutually exclusive (when a function has SPLIT applied, ICALL is not executed in that function's basic blocks)

**Applicability filters**:
- FLA: requires sufficient cyclomatic complexity and conditional jump ratio
- IBR: requires passing the IBR compatibility check
- IGV: requires that the function contains no inline assembly

## 6. Cost Model

### 6.1 Scoring Formula

```
Score(P, F) = Ws × SecurityGain(P, F) + Wd × DiversityGain(P, F)
             - Wr × RuntimeCost(P, F)  - Wz × SizeCost(P, F)
             - Wc × CorrectnessRisk(P, F)
```

Where:
- `Ws`, `Wd`, `Wr`, `Wz`, `Wc` are Profile weights
- `SecurityGain(P, F)` etc. are feature-adaptive scores of Pass P on Function F (clamped to [0,1] via clamp01)

**Decision logic**:
- The pass is enabled when `Score > Threshold`, the pass is not blocked by conflict rules, and boundary condition checks pass
- When a function is at a near-boundary condition (NearBoundary), `choose_machine()` is used for random selection

### 6.2 Profile Weights

| Profile | Ws (Security) | Wd (Diversity) | Wr (Runtime) | Wz (Size) | Wc (Risk) | Use Case |
|---------|:-------------:|:--------------:|:------------:|:---------:|:---------:|----------|
| **balanced** | 1.00 | 0.50 | 0.80 | 0.50 | 1.00 | Default: balances security, overhead, and risk |
| **security** | 1.40 | 0.70 | 0.55 | 0.35 | 0.90 | Aggressive: favors security gains, tolerates higher overhead |
| **performance** | 0.80 | 0.35 | 1.40 | 0.90 | 1.20 | Conservative: strictly controls overhead, suitable for performance-sensitive scenarios |

### 6.3 Pass Base Weights

Each pass has base weights `BaseWeight(P)` across 5 dimensions, representing default valuations without considering function features:

| Pass | SecurityGain | DiversityGain | RuntimeCost | SizeCost | CorrectnessRisk |
|:----:|:------------:|:-------------:|:-----------:|:--------:|:---------------:|
| SUB | 0.75 | 0.55 | 0.25 | 0.05 | 0.10 |
| BCF | 0.45 | 0.65 | 0.20 | 0.10 | 0.20 |
| FLA | **0.80** | **0.75** | **0.85** | **0.60** | 0.35 |
| SPLIT | 0.30 | 0.40 | 0.15 | 0.25 | 0.10 |
| IBR | 0.70 | 0.65 | 0.45 | 0.30 | **0.45** |
| ICALL | 0.55 | 0.50 | 0.35 | 0.20 | 0.30 |
| IGV | 0.55 | 0.50 | 0.30 | 0.35 | **0.50** |

**Basis**: These weights were derived through normalization in pass-only experiments.

### 6.4 Feature-Adaptive Adjustments

Base weights are adaptively adjusted according to function features. For example:

- **IBR.SecurityGain** × `clamp01(0.4 + CondJumpRatio)` — more conditional jumps lead to higher security gains from indirect branches
- **FLA.RuntimeCost** × `1.0 + 0.5 × CyclomaticComplexity` — higher cyclomatic complexity leads to greater FLA overhead
- **BCF.DiversityGain** × `1.0 + 0.3 × AvgCFGDepth` — deeper CFG leads to higher diversity gains from bogus control flow
- All adjustments are constrained to [0, 1] via `clamp01()`

See the `estimateFunctionPassScore()` function in `PassDecider.cpp` for detailed adjustment rules.

## 7. Operating Modes

| Mode | Description |
|------|-------------|
| **cost** (default) | Standard cost model decision. Enabled when Score > Threshold, random selection at NearBoundary |
| **rule** | Rule-based decision. Enabled only when not at NearBoundary, otherwise random selection |
| **random** | Fully random selection. Each candidate pass is enabled with 50% probability |
| **all** | Enable all applicable candidate passes |

## 8. Custom Weights

### 8.1 JSON Weight Configuration File Format

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

Usage:
```bash
clang [flags] -mllvm -mix-cost-config=my_weights.json -o prog source.c
```

Verify the actually applied weights with `-mix-dump-cost-config`:
```bash
clang [flags] \
  -mllvm -mix-cost-config=my_weights.json \
  -mllvm -mix-dump-cost-config=used.json \
   -o prog source.c
cat used.json
```

### 8.2 Dimension Scaling

Globally scale the dimension weights at the Profile level without modifying the JSON:

```bash
# Security gain × 1.2, runtime cost × 0.8 (approaching the security profile)
-mllvm -mix-security-scale=1.2 -mllvm -mix-runtime-scale=0.8
```

### 8.3 Dimension Ablation

Disable a dimension (set weight to 0) using `-mix-disable-dimension`:

```bash
# Ablation experiment: disable the diversity dimension
-mllvm -mix-disable-dimension=diversity
```

Multiple dimensions can be disabled simultaneously by separating with commas:

```bash
-mllvm -mix-disable-dimension=security,diversity
```

## 9. Three-Profile Comparison

| Dimension | balanced | security | performance |
|-----------|:--------:|:--------:|:-----------:|
| SecurityGain weight | 1.00 | 1.40 (+40%) | 0.80 (-20%) |
| DiversityGain weight | 0.50 | 0.70 (+40%) | 0.35 (-30%) |
| RuntimeCost weight | 0.80 | 0.55 (-31%) | 1.40 (+75%) |
| SizeCost weight | 0.50 | 0.35 (-30%) | 0.90 (+80%) |
| CorrectnessRisk weight | 1.00 | 0.90 (-10%) | 1.20 (+20%) |

## 10. Experimental Validation Summary

### 10.1 Parameter Sensitivity

Global parameter experiments show that the cost model is robust to ±20% perturbations in profile weights. The average variation in pass enablement rate is within 3%, indicating that the model does not depend on precise parameter tuning.

### 10.2 Dimension Ablation

Ablation experiments show:
- Disabling the SecurityGain dimension causes passes to trend toward conservative choices
- Disabling RuntimeCost or SizeCost causes the enablement rate to rise significantly
- The CorrectnessRisk dimension has the smallest individual impact but provides a safety net

### 10.3 Pass-only Trend Consistency

Pass-only experiments (7 groups) verify that the base weights of each pass are consistent with their independent application effects. The Spearman correlation between the base weight ranking and the pass-only experimental metric ranking exceeds 0.85.

### 10.4 Ghidra Decompilation Time

Ghidra decompilation experiments compare balanced and security profiles on the Coreutils dataset. The security profile increases average decompilation time by 3× to 8× compared to the baseline (no obfuscation), while the balanced profile increases it by 2× to 4×.

### 10.5 LLVM Normalized Optimization Weights

Final optimization experiments fit pass base weights using LLVM test-suite runtime data, producing a set of weights validated by LLVM's own performance metrics, further improving the rationality of the cost model.

## 11. Testing with LLVM test-suite

```bash
# Build test-suite
cd ~/llvm-test-suite
mkdir -p build && cd build
cmake -G Ninja \
  -DCMAKE_C_COMPILER=/path/to/mixcompile/clang \
  -DTEST_SUITE_SUBDIRS=SingleSource \
  ..
ninja

# Run tests under MIXCOMPILE
./RunSafely.sh
```

## 12. Experiment Directory Structure

| Directory | Content |
|-----------|---------|
| `experiments_mixcompile_param_sensitivity/` | Global parameter sensitivity experiments (±20% perturbation, dimension ablation, 30-group random sampling, transfer validation) |
| `experiments_mixcompile_pass_level_and_ghidra/` | Pass-level parameter rationality experiments (7 pass-only) + Ghidra decompilation time experiments (balanced vs. security comparison) |
| `experiments_mixcompile_misc_full/` | Misc full dataset experiments |
| `experiments_mixcompile_final/` | Final parameter optimization experiments (JSON weight loading, LLVM normalization fitting, optimized weight validation, final Ghidra validation) |

Experiment documentation:
- `MIXCOMPILE代价模型参数合理性实验指导文档.md` — Parameter sensitivity experiment design
- `MIXCOMPILE补充实验_Pass级参数与Ghidra反编译时间指导.md` — Supplementary experiment design
- `experiments_mixcompile_final/experiments_mixcompile_final.md` — Final optimization experiment design

## 13. FAQ

**Q: The compiled program crashes (Segfault)?**

A: Use the balanced or performance profile and avoid overly aggressive obfuscation. If using the security profile, verify correctness with lit on important programs first.

**Q: How do I confirm which passes were enabled?**

A: MIXCOMPILE outputs annotation information to stderr. You can also use `-mix-dump-cost-config` to export weights and confirm that parameters were loaded correctly.

**Q: Why is the annotation count for FLA (or IBR, IGV) zero?**

A: These passes have strict application conditions:
- FLA requires the function to have high cyclomatic complexity and conditional jump ratio
- IBR requires passing the IBR compatibility check
- IGV requires the function to contain no inline assembly
Not triggering on simple test programs is normal behavior and does not indicate that the pass is ineffective.

**Q: How do I choose the right Profile?**

A: Start with balanced. Use performance if strict performance requirements exist. Use security if higher security is desired (tolerating larger binaries and slightly higher risk).

**Q: Can I adjust the weights of individual passes?**

A: Yes. Use `-mix-cost-config=weights.json` to provide a custom weight file. See Section 8.1 for the format.

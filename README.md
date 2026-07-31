# MIXCOMPILE - Feature-Driven LLVM Obfuscation Pass Selection

MIXCOMPILE is an LLVM 17.0.6-based diversified compilation prototype. It inserts a
`PassDecider` module into the LLVM pass pipeline, extracts program features from LLVM IR,
and writes metadata that controls downstream obfuscation passes. The current version
supports profile-based scoring, JSON-loaded pass weights, deterministic seeding,
workload-aware BCF budgeting, and diagnostic output for control-flow pass decisions.

The formal Cybersecurity experiment uses the `optimized` profile with a frozen JSON weight
file. The built-in defaults remain available for calibration and debugging, but they are
not the same as the formal optimized experiment configuration.

---

## Table of Contents

- [1. Overview](#1-overview)
- [2. Repository Layout](#2-repository-layout)
- [3. Build](#3-build)
- [4. Quick Start](#4-quick-start)
- [5. Command-Line Options](#5-command-line-options)
- [6. Passes and Metadata](#6-passes-and-metadata)
- [7. Cost Model](#7-cost-model)
- [8. BCF Workload Model](#8-bcf-workload-model)
- [9. Selection Rules](#9-selection-rules)
- [10. Custom Weight Files](#10-custom-weight-files)
- [11. Reproducibility](#11-reproducibility)
- [12. Formal Experiment Summary](#12-formal-experiment-summary)
- [13. Troubleshooting](#13-troubleshooting)

---

## 1. Overview

MIXCOMPILE differs from uniform obfuscation systems such as an "all passes enabled" OLLVM
configuration. Instead of applying every transformation to every function, it:

1. extracts function-level and basic-block-level IR features;
2. scores candidate obfuscation passes under a selected deployment profile;
3. filters unsafe or excessive candidates through applicability and budget checks;
4. writes LLVM metadata such as `BCF_annotations`, `FLA_annotations`, `SUB_annotations`;
5. lets the downstream transformation passes execute only where their metadata allows it.

The current decision model has five dimensions:

- `SecurityGain`
- `DiversityGain`
- `RuntimeCost`
- `SizeCost`
- `CorrectnessRisk`

The scalar score is:

```text
Score(P, x, W) =
  W_security  * SecurityGain(P, x)
+ W_diversity * DiversityGain(P, x)
- W_runtime   * RuntimeCost(P, x)
- W_size      * SizeCost(P, x)
- W_risk      * CorrectnessRisk(P, x)
```

where `P` is a pass, `x` is the current function or basic block feature vector, and `W`
is the selected profile.

## 2. Repository Layout

```text
MIXCOMPILE/
  Passes/
    PassBuilder.cpp
    CMakeLists.txt
    Obfuscation/
      PassDecider.cpp
      PassDecider.h
      BogusControlFlow.cpp
      Flattening.cpp
      SplitBasicBlock.cpp
      Substitution.cpp
      IndirectBranch.cpp
      IndirectCall.cpp
      IndirectGlobalVariable.cpp
      StringEncryption.cpp
      Utils.cpp
      Utils.h
      CryptoUtils.cpp
      CryptoUtils.h
  README.md
  README_zh.md
```

The main files for the updated decision logic are:

- `Passes/Obfuscation/PassDecider.cpp`: feature extraction, scoring, selection, metadata output, BCF budget diagnostics.
- `Passes/Obfuscation/BogusControlFlow.cpp`: BCF transformation, `-bcf_prob`, `-bcf_loop`, and metadata check.
- `Passes/Obfuscation/Utils.cpp`: `-mix-seed`, deterministic random seed derivation, annotation helpers.
- `Passes/PassBuilder.cpp`: command-line pass flags and LLVM pass pipeline insertion order.

## 3. Build

MIXCOMPILE is integrated into LLVM 17.0.6. Assuming this repository's `Passes` directory has
been copied into the LLVM source tree under `llvm/lib/Passes`, build Clang as usual:

```bash
cd ~/llvm-project-17.0.6.src
mkdir -p build
cd build

cmake -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DLLVM_ENABLE_PROJECTS="clang;compiler-rt" \
  -DLLVM_TARGETS_TO_BUILD="X86" \
  ../llvm

ninja -j$(nproc) clang
```

After editing files in `llvm/lib/Passes/Obfuscation/`, rebuilding `clang` is normally enough:

```bash
ninja -j$(nproc) clang
```

## 4. Quick Start

### 4.1 Default balanced selection

`-pd` enables PassDecider. The transformation pass flags still need to be provided; PassDecider
then decides which functions or basic blocks actually receive metadata.

```bash
clang -O2 source.c -o output_program \
  -mllvm -pd \
  -mllvm -ibr \
  -mllvm -bcf \
  -mllvm -fla \
  -mllvm -split \
  -mllvm -igv \
  -mllvm -sub \
  -mllvm -icall
```

### 4.2 Formal optimized profile

Use the optimized deployment profile together with the frozen optimized pass-weight JSON:

```bash
clang -O2 source.c -o output_program \
  -mllvm -pd \
  -mllvm -ibr -mllvm -bcf -mllvm -fla -mllvm -split \
  -mllvm -igv -mllvm -sub -mllvm -icall \
  -mllvm -mix-profile=optimized \
  -mllvm -mix-cost-config=../experiments-MIXCOMPILE/experiments_MIXCOMPILE_cybersecurity/result/llvm-test-suite/config/optimized_pass_weights.json \
  -mllvm -mix-seed=1 \
  -mllvm -mix-bcf-max-instructions=2000 \
  -mllvm -mix-bcf-max-expected-modified-bb=500
```

### 4.3 Dump the actually used weight file

```bash
clang -O2 source.c -o output_program \
  -mllvm -pd \
  -mllvm -ibr -mllvm -bcf -mllvm -fla -mllvm -split \
  -mllvm -igv -mllvm -sub -mllvm -icall \
  -mllvm -mix-profile=optimized \
  -mllvm -mix-cost-config=weights.json \
  -mllvm -mix-dump-cost-config=used_weights.json
```

### 4.4 Single-pass debugging

```bash
clang -O2 source.c -o output_program \
  -mllvm -pd \
  -mllvm -ibr -mllvm -bcf -mllvm -fla -mllvm -split \
  -mllvm -igv -mllvm -sub -mllvm -icall \
  -mllvm -mix-only-pass=BCF
```

## 5. Command-Line Options

### 5.1 Pass enablement flags

| Option | Purpose |
|---|---|
| `-pd` | Enables PassDecider metadata selection. |
| `-ibr` | Enables the indirect branch transformation pass. |
| `-bcf` | Enables bogus control flow. |
| `-fla` | Enables control-flow flattening. |
| `-split` | Enables basic block splitting. |
| `-igv` | Enables indirect global variable rewriting. |
| `-sub` | Enables instruction substitution. |
| `-icall` | Enables indirect call rewriting. |
| `-sobf` | Enables string obfuscation. This pass is present but is not part of the current PassDecider cost model. |
| `-fncmd` | Enables legacy function-name-controlled obfuscation. |

### 5.2 MIXCOMPILE decision options

| Option | Default | Values / Meaning |
|---|---:|---|
| `-mix-mode` | `cost` | `cost`, `rule`, `random`, or `all`. |
| `-mix-profile` | `balanced` | `balanced`, `optimized`, `security`, or `performance`. |
| `-mix-score-threshold` | `0.0` | Minimum score needed in `cost` mode. |
| `-mix-seed` | `0` | Fixed random seed. `0` uses `random_device`. |
| `-mix-only-pass` | empty | Restrict decisions to one pass: `SUB`, `BCF`, `FLA`, `SPLIT`, `IBR`, `ICALL`, or `IGV`. |
| `-mix-cost-config` | empty | JSON file containing pass base weights. |
| `-mix-dump-cost-config` | empty | Output path for the actually used pass base weights. |
| `-mix-disable-dimension` | empty | Comma-separated dimensions to disable: `security`, `diversity`, `runtime`, `size`, `risk`. |

### 5.3 Dimension scaling

These options scale the five feature-adaptive dimensions after pass-specific feature
modulation:

| Option | Default |
|---|---:|
| `-mix-security-scale` | `1.0` |
| `-mix-diversity-scale` | `1.0` |
| `-mix-runtime-scale` | `1.0` |
| `-mix-size-scale` | `1.0` |
| `-mix-risk-scale` | `1.0` |

The function-level and basic-block-level dimensions are clamped to `[0, 1]` after scaling,
except during the special BCF-FLA comparison described below.

### 5.4 BCF controls

| Option | Default | Meaning |
|---|---:|---|
| `-bcf_prob` | `70` | Probability, in percent, that each basic block is selected by BCF. Must satisfy `0 < x <= 100`. |
| `-bcf_loop` | `2` | Number of BCF iterations on a function. Must be positive. |
| `-mix-bcf-max-instructions` | `2000` | Maximum function instruction count eligible for BCF. |
| `-mix-bcf-max-expected-modified-bb` | `500` | Maximum estimated modified basic blocks eligible for BCF. |

## 6. Passes and Metadata

### 6.1 Function-level passes

| Pass | Metadata key | Positive value | Negative value | Applicability filter |
|---|---|---|---|---|
| `IBR` | `IBR_annotations` | `ibr` | `noibr` | `CondJumpRatio >= 0.15` and IBR compatibility check passes. |
| `BCF` | `BCF_annotations` | `bcf` | `nobcf` | `CyclomaticComplexity >= 3` and BCF budgets pass. |
| `FLA` | `FLA_annotations` | `fla` | `nofla` | `AvgCFGDepth >= 2.0`. |
| `SPLIT` | `SPLIT_annotations` | `split` | `nosplit` | `MaxBBSize >= 5`. |
| `IGV` | `IGV_annotations` | `igv` | `noigv` | Function has no inline assembly. |

### 6.2 Basic-block-level passes

| Pass | Metadata key | Positive value | Applicability filter |
|---|---|---|---|
| `SUB` | `SUB_annotations` | `sub` | Basic block contains substitutable `add`, `sub`, `and`, `or`, or `xor` instructions. |
| `ICALL` | `ICALL_annotations` | `icall` | Basic block contains a call and has fewer than five instructions. |

`SUB` and `ICALL` are attached to the first instruction of the basic block. Function-level
passes are attached to the function.

## 7. Cost Model

### 7.1 Deployment profiles

| Profile | Security | Diversity | Runtime | Size | Risk | Intended use |
|---|---:|---:|---:|---:|---:|---|
| `balanced` | 1.00 | 0.50 | 0.80 | 0.50 | 1.00 | Calibration and default exploratory runs. |
| `optimized` | 1.10 | 1.05 | 0.90 | 0.25 | 0.95 | Formal Cybersecurity experiment profile. |
| `security` | 1.40 | 0.70 | 0.55 | 0.35 | 0.90 | More aggressive security/diversity preference. |
| `performance` | 0.80 | 0.35 | 1.40 | 0.90 | 1.20 | Conservative overhead control. |

### 7.2 Built-in pass base weights

If `-mix-cost-config` is not provided, PassDecider uses the built-in pass base weights:

| Pass | SecurityGain | DiversityGain | RuntimeCost | SizeCost | CorrectnessRisk |
|---|---:|---:|---:|---:|---:|
| `SUB` | 0.75 | 0.55 | 0.25 | 0.05 | 0.10 |
| `BCF` | 0.45 | 0.65 | 0.20 | 0.10 | 0.20 |
| `FLA` | 0.80 | 0.75 | 0.85 | 0.60 | 0.35 |
| `SPLIT` | 0.30 | 0.40 | 0.15 | 0.25 | 0.10 |
| `IBR` | 0.70 | 0.65 | 0.45 | 0.30 | 0.45 |
| `ICALL` | 0.55 | 0.50 | 0.35 | 0.20 | 0.30 |
| `IGV` | 0.55 | 0.50 | 0.30 | 0.35 | 0.50 |

### 7.3 Formal optimized pass base weights

The formal experiment uses a JSON file whose SHA-256 is:

```text
8b23633cef3c2fbdae7e14d8d774e5d7f684ef7aa487aa6dcd5da8c9db340cf4
```

Those optimized weights are:

| Pass | SecurityGain | DiversityGain | RuntimeCost | SizeCost | CorrectnessRisk |
|---|---:|---:|---:|---:|---:|
| `SUB` | 0.490126 | 0.199157 | 0.075497 | 0.048800 | 0.108150 |
| `BCF` | 0.434216 | 0.871526 | 0.773944 | 0.709331 | 0.254053 |
| `FLA` | 0.707056 | 0.488146 | 0.736028 | 0.215843 | 0.365736 |
| `SPLIT` | 0.306698 | 0.172056 | 0.108178 | 0.094880 | 0.114597 |
| `IBR` | 0.572982 | 0.597764 | 0.242986 | 0.236418 | 0.520390 |
| `ICALL` | 0.458715 | 0.157404 | 0.169846 | 0.106568 | 0.376419 |
| `IGV` | 0.516724 | 0.362678 | 0.104081 | 0.135240 | 0.530250 |

## 8. BCF Workload Model

The current PassDecider no longer treats BCF as a static cost. It estimates BCF workload
from the current function and the actual BCF command-line parameters.

Definitions:

```text
N_BB = number of basic blocks
N_I  = number of instructions
c    = cyclomatic complexity
q    = -bcf_prob
L    = -bcf_loop
p    = clamp(q / 100, 0, 1)
```

Expected BCF-modified basic blocks:

```text
ExpansionSum = sum_{i=0}^{L-1} (1 + 3p)^i
ExpectedModifiedBB = N_BB * p * ExpansionSum
```

Runtime and size multipliers:

```text
RuntimeMultiplier = 1 + log2(1 + 18 * ExpectedModifiedBB) / 10

AvgBBInsts = N_I / N_BB
EstimatedAddedInsts = ExpectedModifiedBB * (AvgBBInsts + 18)
SizeMultiplier = 1 + log2(1 + EstimatedAddedInsts) / 12
```

BCF is applicable only when:

```text
CyclomaticComplexity >= 3
TotalInsts <= -mix-bcf-max-instructions
ExpectedModifiedBB <= -mix-bcf-max-expected-modified-bb
```

The default formal budgets are:

```text
-mix-bcf-max-instructions=2000
-mix-bcf-max-expected-modified-bb=500
```

## 9. Selection Rules

### 9.1 Operating modes

| Mode | Behavior |
|---|---|
| `cost` | Enables a candidate if `Score > threshold`; near-boundary cases are randomized. |
| `rule` | Ignores score and enables candidates that pass rule filters; near-boundary cases are randomized. |
| `random` | Randomly enables applicable candidates with approximately 50% probability. |
| `all` | Enables all applicable candidates, then applies conflict filtering. |

### 9.2 Near-boundary randomization

Near-boundary cases use `choose_machine()`, seeded by `-mix-seed` through the MIXCOMPILE
random engine:

| Pass | Near-boundary condition |
|---|---|
| `IBR` | `0.15 <= CondJumpRatio < 0.25` |
| `BCF` | `3 <= CyclomaticComplexity < 5` |
| `FLA` | `2.0 <= AvgCFGDepth < 3.0` |
| `SPLIT` | `5 <= MaxBBSize < 8` |
| `IGV` | Always treated as near-boundary. |
| `SUB` | `SubInsts > 0` and either `SubInsts < 3` or `SubRatio < 0.5`. |
| `ICALL` | Has a call and `BBSize < 5`. |

### 9.3 Conflict handling

The current conflict relation is:

| Conflict | Handling |
|---|---|
| `IBR` vs `FLA` | `IBR` is retained and `FLA` is removed before final ranking. |
| `IBR` vs `IGV` | The later-ranked pass is skipped by generic conflict filtering. |
| `SPLIT` vs `ICALL` | If a function selects `SPLIT`, `ICALL` is not applied to its basic blocks. |

When both BCF and FLA survive their initial filters and `IBR` does not remove FLA,
PassDecider performs a second BCF-FLA comparison. This comparison intentionally does not
clamp the BCF and FLA feature factors, so highly complex functions do not collapse to the
same capped score.

The second comparison uses:

```text
BCF factor = (CyclomaticComplexity - 2) / 4
FLA factor = AvgCFGDepth / 3
FLA runtime factor = 1 + CyclomaticComplexity / 6
```

The higher comparison score is retained. If the comparison scores tie, the initial
clamped scores break the tie.

### 9.4 Diagnostic output

For each function, PassDecider emits a diagnostic line beginning with:

```text
[MIXCOMPILE][CF_DECISION]
```

The line includes:

```text
function
NumBBs
TotalInsts
CyclomaticComplexity
CondJumpRatio
AvgCFGDepth
bcf_prob
bcf_loop
ExpectedModifiedBB
BCFMaxInstructions
BCFMaxExpectedModifiedBB
BCFBudgetEligible
RuntimeMultiplier
SizeMultiplier
BCF_first_total
FLA_first_total
BCF_compare_total
FLA_compare_total
selected_pass
```

This output is the preferred source for checking why BCF was rejected, why FLA was retained,
or which control-flow pass was finally selected.

## 10. Custom Weight Files

`-mix-cost-config` expects a JSON object whose provided pass entries each contain all five
dimensions as finite values in `[0, 1]`. Pass entries omitted from the JSON fall back to the
built-in base weights.

```json
{
  "SUB": {
    "SecurityGain": 0.490126,
    "DiversityGain": 0.199157,
    "RuntimeCost": 0.075497,
    "SizeCost": 0.048800,
    "CorrectnessRisk": 0.108150
  },
  "BCF": {
    "SecurityGain": 0.434216,
    "DiversityGain": 0.871526,
    "RuntimeCost": 0.773944,
    "SizeCost": 0.709331,
    "CorrectnessRisk": 0.254053
  }
}
```

Pass names are normalized to uppercase. Missing dimensions or out-of-range values cause a
fatal error.

To disable one or more dimensions without editing the JSON:

```bash
-mllvm -mix-disable-dimension=diversity
-mllvm -mix-disable-dimension=security,diversity
```

To scale dimensions globally:

```bash
-mllvm -mix-security-scale=1.2 -mllvm -mix-runtime-scale=0.8
```

## 11. Reproducibility

For deterministic experiments, set all of the following:

```bash
-mllvm -mix-seed=1
-mllvm -mix-profile=optimized
-mllvm -mix-cost-config=<optimized_pass_weights.json>
-mllvm -mix-bcf-max-instructions=2000
-mllvm -mix-bcf-max-expected-modified-bb=500
```

The formal experiment freeze is:

```text
freeze_20260723T095408Z
```

The formal configuration archives are under:

```text
../experiments-MIXCOMPILE/experiments_MIXCOMPILE_cybersecurity/result/
```

Important files:

```text
llvm-test-suite/config/optimized_pass_weights.json
llvm-test-suite/config/experiment.json
llama/config/optimized_experiment.json
openssl/config/experiment.json
```

## 12. Formal Experiment Summary

These results describe the current formal Cybersecurity-oriented experiment. They replace
older README claims that MIXCOMPILE generally stays within 5% overhead or accelerates
SHA-512 under OpenSSL.

### 12.1 Correctness

The optimized MIXCOMPILE configuration passed all 31 formal LLVM test-suite validation
binaries.

### 12.2 llama.cpp

Arithmetic-mean throughput over 20 formal measurements:

| Variant | pp512 tokens/s | tg128 tokens/s |
|---|---:|---:|
| GCC | 31.994 | 11.276 |
| LLVM | 37.012 | 12.019 |
| balanced | 31.840 | 11.109 |
| optimized | 32.329 | 11.119 |

Interpretation:

- `optimized` is 1.0% faster than GCC for pp512.
- `optimized` is 1.4% slower than GCC for tg128.
- `optimized` is 12.7% slower than LLVM for pp512.
- `optimized` is 7.5% slower than LLVM for tg128.
- OLLVM-full did not produce a llama.cpp executable within the historical 10800.14 s limit, so it is reported as a build timeout rather than a runtime baseline.

### 12.3 OpenSSL

Arithmetic-mean throughput at 16384-byte blocks, MB/s:

| Algorithm | GCC | LLVM | optimized | OLLVM-full |
|---|---:|---:|---:|---:|
| AES-128-CBC | 384.7 | 414.0 | 238.7 | 43.8 |
| ChaCha20-Poly1305 | 506.9 | 512.7 | 387.8 | 83.2 |
| SHA-512 | 685.0 | 653.0 | 241.9 | 169.4 |

Across all tested OpenSSL block sizes, optimized retains 20.0% to 75.6% of LLVM throughput
depending on algorithm and block size, while consistently outperforming OLLVM-full.

### 12.4 radiff2 similarity

Median correctness-eligible radiff2 similarity:

| Pair | Median |
|---|---:|
| GCC - LLVM | 0.8800 |
| GCC - optimized | 0.5620 |
| LLVM - optimized | 0.5900 |
| OLLVM-full - optimized | 0.3140 |

Lower similarity is used as a binary-diversity proxy. It should not be read as a complete
security proof.

### 12.5 Ghidra decompilation-time proxy

Median per-program Ghidra time ratio relative to LLVM:

| Variant | Median ratio |
|---|---:|
| GCC | 0.677x |
| balanced | 8.615x |
| optimized | 8.109x |
| OLLVM-full | 3.626x |

All 75 Ghidra analyses completed and were parsed. This result measures one automated
reverse-engineering workflow and does not by itself prove resistance to human analysis,
other decompilers, or semantic attacks.

## 13. Troubleshooting

### The binary receives no obfuscation even though `-pd` is set.

`-pd` only runs PassDecider. You must also enable the candidate transformation passes:

```bash
-mllvm -ibr -mllvm -bcf -mllvm -fla -mllvm -split -mllvm -igv -mllvm -sub -mllvm -icall
```

### BCF is not selected.

Check the `[MIXCOMPILE][CF_DECISION]` line. Common reasons are:

- `CyclomaticComplexity < 3`
- `TotalInsts > -mix-bcf-max-instructions`
- `ExpectedModifiedBB > -mix-bcf-max-expected-modified-bb`
- BCF lost the second BCF-FLA comparison
- `-mix-only-pass` excludes BCF

### FLA disappears when IBR is also selected.

This is expected. The current code removes FLA when both IBR and FLA are candidates, then
continues with the remaining conflict-filtered ranking.

### ICALL is not applied inside a function.

If the function selected `SPLIT`, PassDecider suppresses `ICALL` in that function's basic
blocks. Otherwise, ICALL applies only to small basic blocks with calls.

### The run is not reproducible.

Set `-mix-seed` explicitly and make sure the same JSON weight file, profile, BCF budget,
compiler binary, and optimization flags are used.

### The optimized result does not match the formal experiment.

Check that all of these are true:

- `-mix-profile=optimized` is used.
- The formal optimized JSON is loaded with `-mix-cost-config`.
- The JSON hash is `8b23633cef3c2fbdae7e14d8d774e5d7f684ef7aa487aa6dcd5da8c9db340cf4`.
- `-mix-seed=1` is used.
- BCF budgets are `2000` instructions and `500` expected modified basic blocks.
- The same compile flags and benchmark protocol are used.

# Missing or Unverified Paper Data

The following items could not be fully validated from the experiment packages
currently included in or referenced by this archive.

## 1. Parameter summary tables are present, but raw coverage is incomplete

The sensitivity, ablation, random-perturbation, and transfer summary tables are
now archived under `calibration_partial/tables/`. Their rounded values reproduce
the four corresponding paper tables.

The original parameter experiment directory does not currently contain complete
raw coverage for all four groups. Its raw compile, runtime, size, radiff2, and
configuration-registry files cover 33 configurations: original, default, 30
random configurations, and transfer. The ten sensitivity and eight ablation
configurations occur in the summary tables but are absent from those raw files.
Thus random perturbation and transfer have raw support, while sensitivity and
ablation remain summary-only evidence in the available archive.

The completion report states that 51 configurations had once been written to
the raw files, but the files currently present contain the earlier 33-config
version. The missing raw rows or the complete original archive should be
restored before claiming full independent reproducibility.

## 2. The per-pass calibration table is only partially supported

`calibration_partial/tables/table_pass_metric_baseline.csv` contains success,
size ratio, execution ratio, radiff2 similarity, and activation count, but it
does not contain the compile-time ratios shown in the paper.

It also conflicts with the current English TeX table in these rows:

| Field | Available CSV | Current paper |
|---|---:|---:|
| FLA size ratio | 1.0070 | 1.117 |
| IBR execution-time ratio | 1.2026 | 1.033 |
| IGV execution-time ratio | 1.0588 | 1.029 |

The available table marks FLA, IBR, and IGV as `insufficient_activation=yes`
with zero activations. The paper values therefore require another source or a
paper correction before submission.

## 3. Two different optimized weight generations are present

The formal cybersecurity experiment uses the frozen vectors beginning with:

- BCF: 0.434216, 0.871526, 0.773944, 0.709331, 0.254053.

The older `experiments_mixcompile_final/config/optimized_pass_weights.json`
instead uses:

- BCF: 0.6, 0.8, 0.4, 0.25, 0.15.

The current paper matches the formal cybersecurity vectors. Consequently,
`formal_evaluation/*/config/optimized_pass_weights.json` is authoritative, while
the files under `calibration_partial/` are retained only as historical
calibration evidence.

## 4. Older Ghidra data was not used for the current paper result

`experiments_mixcompile_final/results/raw/ghidra_final_raw.csv` contains ten
programs, one repetition per variant, and reports arithmetic averages near
10.806 s for the older optimized configuration. The current paper instead uses
five programs, three repetitions per program, per-program medians, and a median
optimized/LLVM ratio of 8.109x from the cybersecurity formal experiment.

The older Ghidra raw file and summaries were therefore not copied into the
paper-facing archive. Mixing these two protocols would make the paper result
statistically inconsistent.

## 5. Candidate-search files do not provide usable paper evidence

`experiments_mixcompile_final/results/tables/table_candidate_search.csv` marks
the default entry and all fifteen candidate configurations as invalid and
contains no objective or measurement values. Those files were not copied as
paper data.

## 6. Calibration profile name does not match the recorded experiment

The current paper calls the sensitivity and ablation campaign an
`optimized-profile` calibration. However, the experiment flags and registry
record `-mix-profile=balanced`, threshold 0.0, seed 1, with dimension scales
perturbed around 1.0. In the current code, balanced and optimized are different
global cost vectors. Renaming the old balanced experiment in prose does not make
its measurements optimized-profile results.

The paper should either describe these tables as the original balanced/default
calibration campaign, or rerun the configurations using
`-mix-profile=optimized` and the formal optimized pass-weight JSON.

## 7. Calibration source hash differs from the current code

The available parameter registry records PassDecider source SHA-256
`395227bf9a710b6600c1a366412221f561bccbb12806d031b52eb22f99640a3f`.
The current repository file has SHA-256
`c5152d559697ea4cef5fda0f9f3b622f020ab5d25af6e52294490eeec8c563b3`.
The tables therefore describe an earlier implementation revision. The paper
must preserve the archived implementation/hash as the experimental artifact or
rerun the calibration on the current code.

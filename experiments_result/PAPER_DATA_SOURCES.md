# Paper Data Source Map

## Formal configuration

| Paper content | Paper-facing file | Authoritative source |
|---|---|---|
| Frozen optimized pass base vectors | `paper_extracted/optimized_pass_base_vectors.csv` | `formal_evaluation/llvm-test-suite/config/optimized_pass_weights.json` |
| Formal experiment parameters and hashes | None derived | `formal_evaluation/*/config/` and `formal_evaluation/*/manifests/` |

The optimized weight JSON is duplicated in the llama.cpp, OpenSSL, and LLVM
test-suite formal packages. The LLVM test-suite copy is used as the canonical
source in the extracted table.

## Correctness

| Paper content | Paper-facing file | Authoritative source |
|---|---|---|
| GCC, LLVM, and optimized: 31/31 validation binaries passed | `paper_extracted/correctness_validation_summary.csv` | `formal_evaluation/llvm-test-suite/tables/correctness.csv` |
| OLLVM-full support and failure status | `paper_extracted/ollvm_full_correctness_status.csv` | `formal_evaluation/llvm-test-suite/tables/ollvm_correctness.csv` |

## llama.cpp performance

| Paper content | Paper-facing file | Authoritative source |
|---|---|---|
| pp512 and tg128 arithmetic means, 20 measurements | `paper_extracted/llama_arithmetic_means.csv` | `formal_evaluation/llama/raw/baselines_llama_bench_repetitions.csv` and `optimized_llama_bench_repetitions.csv` |
| OLLVM-full 10800.14 s build timeout | None derived | `formal_evaluation/llama/tables/ollvm_full_timeout_builds.csv` and corresponding run manifest |

Rounded paper values are GCC 31.994/11.276, LLVM 37.012/12.019, and optimized
32.329/11.119 tokens/s for pp512/tg128.

## OpenSSL performance

| Paper content | Paper-facing file | Authoritative source |
|---|---|---|
| Arithmetic means for all algorithms and block sizes used by the figure | `paper_extracted/openssl_arithmetic_means_all_blocks.csv` | `formal_evaluation/openssl/raw/speed_repetitions.csv` |
| Arithmetic means at 16384-byte blocks | `paper_extracted/openssl_arithmetic_means_16384.csv` | Same raw file |
| Optimized throughput range relative to LLVM | `paper_extracted/openssl_optimized_vs_llvm_ranges.csv` | Recalculated from the all-block arithmetic means |

The validated optimized/LLVM ranges are 23.293--57.941% for AES-128-CBC,
22.505--75.638% for ChaCha20-Poly1305, and 20.000--37.221% for SHA-512.

## Binary diversity

| Paper content | Paper-facing file | Authoritative source |
|---|---|---|
| Formal radiff2 table and heatmap values | `paper_extracted/radiff2_formal_medians.csv` | `formal_evaluation/llvm-test-suite/tables/radiff2_summary.csv` |
| Per-program comparisons, eligibility, and timeout records | None derived | `formal_evaluation/llvm-test-suite/raw/radiff2_pairs.csv` |

The extracted table intentionally excludes rows involving the former balanced
label because the current paper reports GCC, LLVM, optimized, and OLLVM-full.

## Ghidra automated-analysis cost

| Paper content | Paper-facing file | Authoritative source |
|---|---|---|
| Cross-program median decompilation times and ratios | `paper_extracted/ghidra_formal_medians_and_ratios.csv` | `formal_evaluation/llvm-test-suite/tables/ghidra_by_sample.csv` and `ghidra_ratios.csv` |
| All 75 analysis runs | None derived | `formal_evaluation/llvm-test-suite/raw/ghidra_runs.csv` |

The optimized cross-program median is 4.739043484 s, and the median per-program
ratio relative to LLVM is 8.108811085x.

## Parameter calibration

`calibration_partial/tables/table_pass_metric_baseline.csv` partially supports
the paper's per-pass calibration table. The accompanying default, refitted, and
older optimized weight files are retained under `calibration_partial/config/`.
They must not be treated as the authoritative formal optimized weights; see
`MISSING_OR_UNVERIFIED.md`.

| Paper content | Archived summary | Original source |
|---|---|---|
| Ten single-dimension plus/minus 20% configurations | `calibration_partial/tables/table_sensitivity.csv` | `experiments_mixcompile_param_sensitivity/results/tables/table_sensitivity.csv` |
| Full model and dimension ablations | `calibration_partial/tables/table_ablation.csv` | `experiments_mixcompile_param_sensitivity/results/tables/table_ablation.csv` |
| Thirty joint random perturbations | `calibration_partial/tables/table_random_sampling.csv` | `experiments_mixcompile_param_sensitivity/results/tables/table_random_sampling.csv` |
| Stanford transfer result | `calibration_partial/tables/table_transfer.csv` | `experiments_mixcompile_param_sensitivity/results/tables/table_transfer.csv` |

The fourth source path supplied for copying repeated `table_random_sampling.csv`.
The transfer table was copied in its place because it is the fourth missing paper
table identified in the preceding audit.

# MIXCOMPILE Paper Experiment Results

This directory is a curated archive of the experiment data used by the current
MIXCOMPILE paper. It was assembled from these source packages:

- `experiments-MIXCOMPILE/experiments_MIXCOMPILE_cybersecurity/result`
- `experiments-MIXCOMPILE/experiments_mixcompile_final`
- `experiments-MIXCOMPILE/experiments_mixcompile_param_sensitivity/results/tables`

## Directory layout

- `formal_evaluation/`: complete formal result package copied from the
  cybersecurity experiment. This is the authoritative source for llama.cpp,
  OpenSSL, LLVM test-suite correctness, radiff2, and the current Ghidra results.
- `paper_extracted/`: paper-facing values recalculated or filtered from the
  formal raw data. Throughput uses arithmetic means; radiff2 and Ghidra use the
  medians defined in the paper.
- `calibration_partial/`: configuration and summary tables from the older
  calibration package plus the sensitivity, ablation, random-perturbation, and
  transfer summary tables. The summary values reproduce the corresponding paper
  tables, but the profile label and raw-data limitations documented below remain.
- `PAPER_DATA_SOURCES.md`: mapping from paper results to archived files.
- `CODE_PAPER_CORRESPONDENCE.md`: audit of data, paper text, and current code.
- `MISSING_OR_UNVERIFIED.md`: missing data, conflicting values, and superseded
  results found during validation.
- `SHA256SUMS.csv`: file size and SHA-256 inventory for integrity checks.

## Statistical rules

- llama.cpp and OpenSSL: arithmetic mean over the 20 rows whose phase is
  `measure` and whose parse/status field is `OK`.
- radiff2: median similarity over successful, correctness-eligible comparisons.
- Ghidra: first reduce three repetitions to a per-program median, then take the
  median across five programs. Ratios are computed per program relative to LLVM
  before the cross-program median is taken.
- Correctness: count `PASS` entries in the formal correctness table.

The summary files originally stored in `formal_evaluation/llama/tables` and
`formal_evaluation/openssl/tables` report medians. They are retained as source
records, but the current paper's performance tables use the arithmetic means in
`paper_extracted/`.

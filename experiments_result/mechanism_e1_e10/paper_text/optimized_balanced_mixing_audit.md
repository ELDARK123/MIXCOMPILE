# optimized/balanced 混用审计（generated 2026-09-03T02:51:45Z）

## 1. 权重文件普查与 SHA-256 一致性

| 文件名 | 副本数 | SHA-256 一致 | 路径（mtime 排序） |
|---|---|---|---|
| baseline_pass_weights.json | 17 | **不一致** | `experiments_mixcompile_final/config/baseline_pass_weights.json` (2026-06-04T08:40:57Z, `dcf5502440d8…`); `experiments_mixcompile_cybersecurity/runs/run_20260718T004513Z/config_before/baseline_pass_weights.json` (2026-07-17T15:26:42Z, `8f94ee3880b6…`); `experiments_mixcompile_cybersecurity/runs/run_20260721T042721Z/config_snapshot/baseline_pass_weights.json` (2026-07-17T15:26:42Z, `8f94ee3880b6…`); `experiments_mixcompile_cybersecurity/runs/run_20260721T050348Z/config_snapshot/baseline_pass_weights.json` (2026-07-17T15:26:42Z, `8f94ee3880b6…`); `experiments_mixcompile_cybersecurity/runs/run_20260721T043445Z/config_snapshot/baseline_pass_weights.json` (2026-07-17T15:26:42Z, `8f94ee3880b6…`); `experiments_mixcompile_cybersecurity/runs/run_20260717T153648Z/config_snapshot/baseline_pass_weights.json` (2026-07-17T15:26:42Z, `8f94ee3880b6…`); `experiments_mixcompile_cybersecurity/runs/run_20260723T132837Z/config_snapshot/baseline_pass_weights.json` (2026-07-17T15:26:42Z, `8f94ee3880b6…`); `experiments_mixcompile_cybersecurity/runs/run_20260718T003940Z/config_snapshot/baseline_pass_weights.json` (2026-07-17T15:26:42Z, `8f94ee3880b6…`); `experiments_mixcompile_cybersecurity/runs/run_20260724T010719Z/config_snapshot/baseline_pass_weights.json` (2026-07-17T15:26:42Z, `8f94ee3880b6…`); `experiments_mixcompile_cybersecurity/runs/run_20260724T005648Z/config_snapshot/baseline_pass_weights.json` (2026-07-17T15:26:42Z, `8f94ee3880b6…`); `experiments_mixcompile_cybersecurity/runs/run_20260718T005106Z/config_snapshot/baseline_pass_weights.json` (2026-07-17T15:26:42Z, `8f94ee3880b6…`); `experiments_mixcompile_cybersecurity/runs/run_20260723T101552Z/config_snapshot/baseline_pass_weights.json` (2026-07-17T15:26:42Z, `8f94ee3880b6…`); `experiments_mixcompile_cybersecurity/runs/run_20260718T003315Z/config_snapshot/baseline_pass_weights.json` (2026-07-17T15:26:42Z, `8f94ee3880b6…`); `experiments_mixcompile_cybersecurity/runs/run_20260724T005403Z/config_snapshot/baseline_pass_weights.json` (2026-07-17T15:26:42Z, `8f94ee3880b6…`); `experiments_mixcompile_cybersecurity/runs/run_20260723T113858Z/config_snapshot/baseline_pass_weights.json` (2026-07-17T15:26:42Z, `8f94ee3880b6…`); `experiments_mixcompile_cybersecurity/config/baseline_pass_weights.json` (2026-07-17T15:26:42Z, `8f94ee3880b6…`); `experiments_mixcompile_cybersecurity/runs/run_20260721T041015Z/config_snapshot/baseline_pass_weights.json` (2026-07-21T04:10:15Z, `8f94ee3880b6…`) |
| default_balanced.json | 1 | 一致 | `experiments_mixcompile_final/config/default_balanced.json` (2026-06-04T08:40:57Z, `dcf5502440d8…`) |
| evidence_normalized_pass_weights.json | 2 | 一致 | `experiments_mixcompile_cybersecurity/runs/run_20260719T142044Z/results/evidence_normalized_pass_weights.json` (2026-07-19T14:27:55Z, `e66d529e19be…`); `experiments_mixcompile_cybersecurity/config/evidence_normalized_pass_weights.json` (2026-07-19T14:27:55Z, `e66d529e19be…`) |
| interior_evidence_normalized_pass_weights.json | 2 | 一致 | `experiments_mixcompile_cybersecurity/runs/run_20260720T072937Z/results/interior_evidence_normalized_pass_weights.json` (2026-07-20T07:29:37Z, `501cd16a4f27…`); `experiments_mixcompile_cybersecurity/config/interior_evidence_normalized_pass_weights.json` (2026-07-20T07:29:37Z, `501cd16a4f27…`) |
| llvm_normalized_weights.json | 1 | 一致 | `experiments_mixcompile_new/config/llvm_normalized_weights.json` (2026-06-24T08:27:04Z, `38931a72ca2f…`) |
| optimized_pass_weights.json | 12 | **不一致** | `experiments_mixcompile_final/config/optimized_pass_weights.json` (2026-06-04T09:00:41Z, `d3c6a203df97…`); `experiments_mixcompile_cybersecurity/runs/run_20260721T042721Z/config_snapshot/optimized_pass_weights.json` (2026-07-20T23:07:25Z, `8b23633cef3c…`); `experiments_mixcompile_cybersecurity/runs/run_20260721T050348Z/config_snapshot/optimized_pass_weights.json` (2026-07-20T23:07:25Z, `8b23633cef3c…`); `experiments_mixcompile_cybersecurity/runs/run_20260721T043445Z/config_snapshot/optimized_pass_weights.json` (2026-07-20T23:07:25Z, `8b23633cef3c…`); `experiments_mixcompile_cybersecurity/runs/run_20260723T132837Z/config_snapshot/optimized_pass_weights.json` (2026-07-20T23:07:25Z, `8b23633cef3c…`); `experiments_mixcompile_cybersecurity/runs/run_20260724T010719Z/config_snapshot/optimized_pass_weights.json` (2026-07-20T23:07:25Z, `8b23633cef3c…`); `experiments_mixcompile_cybersecurity/runs/run_20260724T005648Z/config_snapshot/optimized_pass_weights.json` (2026-07-20T23:07:25Z, `8b23633cef3c…`); `experiments_mixcompile_cybersecurity/runs/run_20260723T095609Z/config_snapshot/optimized_pass_weights.json` (2026-07-20T23:07:25Z, `8b23633cef3c…`); `experiments_mixcompile_cybersecurity/runs/run_20260724T005403Z/config_snapshot/optimized_pass_weights.json` (2026-07-20T23:07:25Z, `8b23633cef3c…`); `experiments_mixcompile_cybersecurity/runs/run_20260723T113858Z/config_snapshot/optimized_pass_weights.json` (2026-07-20T23:07:25Z, `8b23633cef3c…`); `experiments_mixcompile_cybersecurity/config/optimized_pass_weights.json` (2026-07-20T23:07:25Z, `8b23633cef3c…`); `experiments_mixcompile_cybersecurity/runs/run_20260721T041015Z/config_snapshot/optimized_pass_weights.json` (2026-07-21T04:10:15Z, `8b23633cef3c…`) |
| practical_weights.json | 1 | 一致 | `experiments_mixcompile_new/config/practical_weights.json` (2026-06-24T08:44:22Z, `4bf2601ca872…`) |
| refit_pass_weights.json | 1 | 一致 | `experiments_mixcompile_final/config/refit_pass_weights.json` (2026-06-04T08:40:57Z, `da69ac3bbae0…`) |

## 2. 各实验目录 profile / -mix-cost-config 使用证据

### experiments_mixcompile

- `config`: profiles={'balanced': 1, 'security': 1, 'performance': 1} cost_configs=[] seeds=[] optimized_mentions=0 balanced_mentions=2
- `builds`: profiles={'balanced': 2469, 'security': 1754, 'performance': 1754} cost_configs=[] seeds=[] optimized_mentions=0 balanced_mentions=2469
- `results`: profiles={'balanced': 2, 'security': 1, 'performance': 1} cost_configs=['<json>`，从'] seeds=[] optimized_mentions=0 balanced_mentions=3
### experiments_mixcompile_cybersecurity

- `scripts`: profiles={} cost_configs=['{weights}",', '{weights}"]', '{w}"]'] seeds=['1'] optimized_mentions=10 balanced_mentions=5
- `runs`: profiles={'optimized': 44, 'balanced': 11833} cost_configs=['baseline_pass_weights.json', 'baseline_pass_weights.json"', 'baseline_pass_weights.json",', 'baseline_pass_weights.json,PASS,8,"{""Puzzle"":', 'optimized_pass_weights.json"', 'optimized_pass_weights.json",', 'optimized_pass_weights.json,PASS,8,"{""Puzzle"":', '{ROOT', '{baseline_weights}"', '{optimized_weights}"', '{weights}",', '{weights}"]', '{w}"]'] seeds=['1', '10', '11', '12', '13', '14', '15', '16', '17', '18', '19', '2', '20', '21', '22', '23', '24', '25', '26', '27', '28', '29', '3', '30', '4', '5', '6', '7', '8', '9'] optimized_mentions=406 balanced_mentions=13165
- `config`: profiles={'balanced': 2, 'optimized': 1} cost_configs=['{baseline_weights}"', '{optimized_weights}"'] seeds=[] optimized_mentions=4 balanced_mentions=3
- `artifacts`: profiles={'optimized': 551, 'balanced': 279} cost_configs=['baseline_pass_weights.json', 'optimized_pass_weights.json'] seeds=['1'] optimized_mentions=1123 balanced_mentions=568
- `results`: profiles={'balanced': 1} cost_configs=[] seeds=[] optimized_mentions=1 balanced_mentions=6

```
scripts/30_run_formal_openssl.py: return paths["MIXCOMPILE_CLANG"],paths["MIXCOMPILE_CLANGXX"],["-mllvm","-pd","-mllvm","-ibr","-mllvm","-bcf","-mllvm","-fla","-mllvm","-split","-mllvm","-igv","-mllvm","-sub","-mllvm","-icall","-mllvm","-mix-mode=cost","
scripts/31_run_formal_testsuite_build.py: f"-mix-profile={profile}", weight_marker]
scripts/28_run_formal_llama.py: "-mllvm", "-mix-mode=cost", "-mllvm", f"-mix-profile={variant}",
scripts/28_run_formal_llama.py: "-mllvm", f"-mix-seed={config['seed']}", "-mllvm", f"-mix-cost-config={weights}",
scripts/26_run_passdecider_preliminary_probe.py: "-mllvm", f"-mix-profile={variant}", "-mllvm",
scripts/26_run_passdecider_preliminary_probe.py: f"-mix-seed={config['seed']}", "-mllvm", f"-mix-cost-config={weights}"]
runs/run_20260718T004513Z/driver_backup/03_build_test_suite.py: "-mllvm", "-mix-mode=cost", "-mllvm", "-mix-profile=balanced",
runs/run_20260718T004513Z/driver_backup/03_build_test_suite.py: "-mllvm", f"-mix-cost-config={ROOT / 'config/baseline_pass_weights.json'}",
runs/run_20260722T031818Z/config_snapshot/30_run_formal_openssl.py: return paths["MIXCOMPILE_CLANG"],paths["MIXCOMPILE_CLANGXX"],["-mllvm","-pd","-mllvm","-ibr","-mllvm","-bcf","-mllvm","-fla","-mllvm","-split","-mllvm","-igv","-mllvm","-sub","-mllvm","-icall","-mllvm","-mix-mode=cost","
runs/run_20260722T135804Z/config_snapshot/26_run_passdecider_preliminary_probe.py: "-mllvm", f"-mix-profile={variant}", "-mllvm",
runs/run_20260722T135804Z/config_snapshot/26_run_passdecider_preliminary_probe.py: f"-mix-seed={config['seed']}", "-mllvm", f"-mix-cost-config={weights}"]
runs/run_20260722T133040Z/config_snapshot/26_run_passdecider_preliminary_probe.py: "-mllvm", f"-mix-profile={variant}", "-mllvm",
runs/run_20260722T133040Z/config_snapshot/26_run_passdecider_preliminary_probe.py: f"-mix-seed={config['seed']}", "-mllvm", f"-mix-cost-config={weights}"]
runs/run_20260723T011500Z/config_snapshot/26_run_passdecider_preliminary_probe.py: "-mllvm", f"-mix-profile={variant}", "-mllvm",
runs/run_20260723T011500Z/config_snapshot/26_run_passdecider_preliminary_probe.py: f"-mix-seed={config['seed']}", "-mllvm", f"-mix-cost-config={weights}"]
runs/run_20260722T031440Z/config_snapshot/30_run_formal_openssl.py: return paths["MIXCOMPILE_CLANG"],paths["MIXCOMPILE_CLANGXX"],["-mllvm","-pd","-mllvm","-ibr","-mllvm","-bcf","-mllvm","-fla","-mllvm","-split","-mllvm","-igv","-mllvm","-sub","-mllvm","-icall","-mllvm","-mix-mode=cost","
runs/run_20260723T025904Z/config_snapshot/26_run_passdecider_preliminary_probe.py: "-mllvm", f"-mix-profile={variant}", "-mllvm",
runs/run_20260723T025904Z/config_snapshot/26_run_passdecider_preliminary_probe.py: f"-mix-seed={config['seed']}", "-mllvm", f"-mix-cost-config={weights}"]
runs/run_20260723T022825Z/config_snapshot/26_run_passdecider_preliminary_probe.py: "-mllvm", f"-mix-profile={variant}", "-mllvm",
runs/run_20260723T022825Z/config_snapshot/26_run_passdecider_preliminary_probe.py: f"-mix-seed={config['seed']}", "-mllvm", f"-mix-cost-config={weights}"]
runs/run_20260717T153648Z/driver_backup/03_build_test_suite.py: "-mix-profile=balanced", "-mllvm", f"-mix-seed={args.mix_seed}",
runs/run_20260717T153648Z/driver_backup/03_build_test_suite.py: "-mllvm", f"-mix-cost-config={ROOT / 'config/baseline_pass_weights.json'}"]
runs/run_20260722T142202Z/config_snapshot/26_run_passdecider_preliminary_probe.py: "-mllvm", f"-mix-profile={variant}", "-mllvm",
runs/run_20260722T142202Z/config_snapshot/26_run_passdecider_preliminary_probe.py: f"-mix-seed={config['seed']}", "-mllvm", f"-mix-cost-config={weights}"]
runs/run_20260723T132837Z/config_snapshot/30_run_formal_openssl.py: return paths["MIXCOMPILE_CLANG"],paths["MIXCOMPILE_CLANGXX"],["-mllvm","-pd","-mllvm","-ibr","-mllvm","-bcf","-mllvm","-fla","-mllvm","-split","-mllvm","-igv","-mllvm","-sub","-mllvm","-icall","-mllvm","-mix-mode=cost","
runs/run_20260721T130607Z/config_snapshot/26_run_passdecider_preliminary_probe.py: "-mllvm", f"-mix-profile={variant}", "-mllvm",
runs/run_20260721T130607Z/config_snapshot/26_run_passdecider_preliminary_probe.py: f"-mix-seed={config['seed']}", "-mllvm", f"-mix-cost-config={weights}"]
runs/run_20260724T010719Z/config_snapshot/31_run_formal_testsuite_build.py: f"-mix-profile={profile}", weight_marker]
runs/run_20260724T005648Z/config_snapshot/31_run_formal_testsuite_build.py: f"-mix-profile={profile}", weight_marker]
runs/run_20260721T141330Z/config_snapshot/28_run_formal_llama.py: "-mllvm", "-mix-mode=cost", "-mllvm", f"-mix-profile={variant}",
```
### experiments_mixcompile_final

- `scripts`: profiles={'balanced': 2} cost_configs=['{json_path}"]', '{opt_json}'] seeds=['1'] optimized_mentions=8 balanced_mentions=5
- `builds`: profiles={'balanced': 58} cost_configs=['optimized_pass_weights.json'] seeds=['1'] optimized_mentions=57 balanced_mentions=58
- `results`: profiles={} cost_configs=['<path>`).'] seeds=[] optimized_mentions=6 balanced_mentions=4

```
scripts/02_fit_and_optimize_weights.py: "-mllvm", "-mix-mode=cost", "-mllvm", "-mix-profile=balanced",
scripts/02_fit_and_optimize_weights.py: full_flags += ["-mllvm", f"-mix-cost-config={json_path}"]
scripts/03_build_and_validate.py: COST_BASE_FLAGS = "-mllvm -mix-mode=cost -mllvm -mix-profile=balanced -mllvm -mix-score-threshold=0.0 -mllvm -mix-seed=1"
scripts/03_build_and_validate.py: extra = f"-mllvm -mix-cost-config={opt_json} -mllvm -mix-dump-cost-config={LOGS_DIR / 'optimized_used.json'}"
```
### experiments_mixcompile_large_profile

- `builds`: profiles={'balanced': 1754} cost_configs=[] seeds=[] optimized_mentions=0 balanced_mentions=1754
- `results`: profiles={'balanced': 1} cost_configs=[] seeds=[] optimized_mentions=0 balanced_mentions=2
### experiments_mixcompile_misc_full

- `config`: profiles={'balanced': 6, 'security': 1, 'performance': 1} cost_configs=[] seeds=[] optimized_mentions=0 balanced_mentions=7
- `builds`: profiles={'balanced': 858, 'security': 143, 'performance': 143} cost_configs=[] seeds=[] optimized_mentions=0 balanced_mentions=858
- `results`: profiles={'balanced': 6, 'security': 1, 'performance': 1} cost_configs=[] seeds=[] optimized_mentions=0 balanced_mentions=7
### experiments_mixcompile_modes_misc

- `config`: profiles={'balanced': 1} cost_configs=[] seeds=['1', '2', '3'] optimized_mentions=0 balanced_mentions=2
- `builds`: profiles={'balanced': 143} cost_configs=[] seeds=['1', '2', '3'] optimized_mentions=0 balanced_mentions=143
- `results`: profiles={'balanced': 1} cost_configs=[] seeds=['1', '2', '3'] optimized_mentions=0 balanced_mentions=2
### experiments_mixcompile_new

- `scripts`: profiles={'balanced': 2} cost_configs=['{practical_path}'] seeds=['1'] optimized_mentions=0 balanced_mentions=3
- `builds`: profiles={'balanced': 360} cost_configs=['practical_weights.json'] seeds=['1'] optimized_mentions=0 balanced_mentions=360

```
scripts/build_all.py: COST_FLAGS = "-mllvm -mix-mode=cost -mllvm -mix-profile=balanced -mllvm -mix-score-threshold=0.0 -mllvm -mix-seed=1"
scripts/build_practical.py: COST_FLAGS = "-mllvm -mix-mode=cost -mllvm -mix-profile=balanced -mllvm -mix-score-threshold=0.0 -mllvm -mix-seed=1"
scripts/build_practical.py: ("practical", f"-mllvm -mix-cost-config={practical_path} -mllvm -mix-dump-cost-config={CONFIG / 'practical_used.json'}"),
```
### experiments_mixcompile_param_sensitivity

- `config`: profiles={'balanced': 50} cost_configs=[] seeds=['1'] optimized_mentions=0 balanced_mentions=50
- `builds`: profiles={'balanced': 1434} cost_configs=[] seeds=['1'] optimized_mentions=0 balanced_mentions=1434
- `results`: profiles={'balanced': 50} cost_configs=[] seeds=['1'] optimized_mentions=0 balanced_mentions=50
### experiments_mixcompile_pass_level_and_ghidra

- `scripts`: profiles={'balanced': 1} cost_configs=[] seeds=['1'] optimized_mentions=0 balanced_mentions=1
- `builds`: profiles={'balanced': 203} cost_configs=[] seeds=['1'] optimized_mentions=0 balanced_mentions=203
- `results`: profiles={'balanced': 2, 'security': 1} cost_configs=[] seeds=['1'] optimized_mentions=0 balanced_mentions=23

```
scripts/run_experiments.py: "-mllvm", "-mix-profile=balanced",
```
### experiments_mixcompile_re

- `.`: profiles={'optimized': 1, 'balanced': 1} cost_configs=['baseline_pass_weights.json>', 'optimized_pass_weights.json>'] seeds=[] optimized_mentions=44 balanced_mentions=14

## 3. 结论要点

- 编译器内置 profile 仅有 balanced/security/performance；历史上 'optimized' 仅通过 -mix-cost-config 权重文件实现，命令行 profile 实际为 balanced —— 这构成提示词所述混用问题的机制。
- optimized_pass_weights.json 存在多份不同 mtime 的副本；其 SHA-256 是否一致见上表（不一致的旧结果不得进入新统计汇总）。
- 可复现性分级：reusable_candidate 仅授予具有 run_manifest/registry（含哈希与种子）的目录；其余历史 CSV 一律 unverified，不得进入新的统计汇总。

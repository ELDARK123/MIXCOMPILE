# 环境审计（generated 2026-09-03T02:51:45Z）

- PROJECT_ROOT: `/home/vincent/Desktop/experiments-MIXCOMPILE`
- Python: `/usr/bin/python3` (3.10.12)

## 源码与编译器

- **llvm/lib/Passes/Obfuscation/PassDecider.cpp**: path=`/home/vincent/Desktop/llvm-project-17.0.6.src/llvm/lib/Passes/Obfuscation/PassDecider.cpp` sha256=`c5152d559697ea4cef5fda0f9f3b622f020ab5d25af6e52294490eeec8c563b3` mtime=2026-07-23T06:34:47Z version=None
- **llvm/lib/Passes/Obfuscation/PassDecider.h**: path=`/home/vincent/Desktop/llvm-project-17.0.6.src/llvm/lib/Passes/Obfuscation/PassDecider.h` sha256=`e4befe6b24f25ca6020d7cb2c10ccfa8041ff006e0b6c82a1d3d8423213e8939` mtime=2026-05-29T11:46:11Z version=None
- **mixcompile_clang**: path=`/home/vincent/Desktop/llvm-project-17.0.6.src/build/bin/clang-17` sha256=`fe7f9bb8f584060e35771f66168da80c3b9eb3c75ce9071f73e4cd39b6e3b778` mtime=2026-07-23T06:35:30Z version=clang version 17.0.6

## 测试集与外部资源

- **llvm_test_suite**: `/home/vincent/Desktop/llvm-test-suite-main/llvm-test-suite-main` exists=True {'git_head': None, 'note': 'no .git metadata; version/tag unverified (likely zip download of main)'}
- **llama_cpp**: `/home/vincent/Desktop/llama.cpp` exists=True {}
- **openssl**: `/home/vincent/Desktop/openssl-3.6.1` exists=True {}
- **ghidra**: `/home/vincent/Desktop/ghidra_12.1_PUBLIC` exists=True {}

## 当前代码与文档一致性抽查（只报告，不修复）

- estimateFunctionPassScore: found (line 420)
- clamp01: found (line 141)
- choose_machine (boundary random): found (line 26)
- conflict IBR vs IGV: found (line 70)
- conflict SPLIT vs ICALL: found (line 70)
- mix-cost-config loading: found (line 75)
- mix-dump-cost-config: found (line 80)
- mix-disable-dimension: found (line 85)
- mix-only-pass: found (line 69)
- mix-seed: NOT FOUND
- profile balanced: found (line 30)
- profile security: found (line 30)
- profile performance: found (line 30)
- five dimensions SecurityGain: found (line 45)
- five dimensions DiversityGain: found (line 50)
- five dimensions RuntimeCost: found (line 55)
- five dimensions SizeCost: found (line 60)
- five dimensions CorrectnessRisk: found (line 65)

工具明细见 `environment/tool_discovery.json`。

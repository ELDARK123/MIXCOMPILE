# T4 = 1.5 -mix-seed driver 层实机验证（2026-09-18）

## 1. flag 注册位置
- `-mix-seed` 为 LLVM `cl::opt`，注册于 **Obfuscation/Utils.cpp:31**
  （`MixSeed("mix-seed", ..., init(0))`），经 `initializeMixRandomSeed()`
  （Utils.cpp:40）初始化，0 = random_device。
- clang driver Options.td **无此选项**（预期行为）：经 `-mllvm -mix-seed=N`
  直通 LLVM 后端。此前环境审计在 PassDecider.cpp 检索 NOT FOUND 的原因即此
  ——声明在 Utils.cpp，使用在 PassDecider.cpp（`getMixRequestedSeed()`，
  PassDecider.cpp:882 附近调用）。

## 2. 同 seed 双构建一致性实测（five11, optimized 权重, seed=1, 干净目录 ×2）
| 层 | b1 | b2 | 一致 |
|---|---|---|---|
| CF_DECISION 决策日志 | 11 行 | 11 行 | **逐行一致** |
| 未 strip 整文件 | 51b3b236… | 51b3b236… | ✓ |
| strip 后整文件 | fd3acf29… | fd3acf29… | ✓ |
| .text 提取 | b553f15f… | b553f15f… | ✓ |

三层哈希 + 决策日志全部一致；未 strip 文件亦一致（无时间戳/构建 ID 伪差异）。

## 3. 结论
**同 seed → 确定性构建成立**（`nondeterministic_build` 未触发）。
论文复现段落可声明：seed + profile + 权重 JSON + 同二进制/-O 下，
CF_DECISION 决策序列与三层二进制哈希完全可复现。
（与 2026-09-16 voronoi 双构建验证相互印证。）

# ECC 研究型分支说明

## 目标

把 `T5-2` 明确收敛为独立研究轨，不把未经验证的 ECC 方案直接并入主线。

## 研究分支建议

- 分支名：`exp/ecc-erasure-aware`
- 结果目录：`benchmarks/reports/ecc-*`
- 模板：`benchmarks/templates/ecc-research-template.json`

## 当前范围

优先比较三类方案：

1. `RS + erasure`
2. `soft-decision RS`
3. `LDPC / RaptorQ` 的工程可行性

## 输入要求

- 使用 `Phase 4` 的多帧融合结果作为上游输入
- 保留每个 cell 的低置信度标记，允许把位置降级为 erasure
- 与当前主线 `hard decision + fountain + RS` 基线对比

## 决策标准

只有同时满足以下条件，才考虑进入主线：

- 同等场景下 `goodput_kbps` 稳定提升
- `time_to_first_chunk_ms` 没有明显恶化
- 实现复杂度可被现有维护成本接受

## 当前结论

当前仓库已补齐研究模板和执行边界，主线仍保持现有 `RS / fountain` 路径。

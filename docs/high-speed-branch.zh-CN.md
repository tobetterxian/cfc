# 高帧率专用分支说明

## 目标

把高帧率能力隔离为专项实验分支，只在明确支持 `CONSTRAINED_HIGH_SPEED_VIDEO` 的机型上验证。

## 当前基础

- `OpencvCamera2View` 已记录 `CONSTRAINED_HIGH_SPEED_VIDEO` 能力
- `CameraCaptureProfile` 已区分 `balanced / throughput / robust`

## 分支策略

- 建议分支名：`exp/android-high-speed-session`
- 基准模板：`benchmarks/templates/high-speed-session-template.json`
- 场景目录：`benchmarks/scenarios/high-speed/`

## 启用条件

- 兼容矩阵中明确标记支持高帧率 session
- 普通 `Camera2` 路径已经稳定
- 高密度模式收益已在目标设备验证

## 隔离要求

- 不改动主线默认 profile
- 高帧率 sender profile 单独命名，不复用 `R1/R2/R3`
- 所有实验结果单独归档，不与主线 baseline 混用

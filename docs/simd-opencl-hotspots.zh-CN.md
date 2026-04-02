# SIMD / OpenCL / UMat 热点优化说明

## 目标

在 `ROI`、灰度优先和多帧融合已经落地的前提下，只对真实热点做专项优化。

## 热点优先级

建议优先观察以下路径：

1. `Scanner.scan()`
2. `Deskewer::deskew()`
3. `cv::warpPerspective`
4. cell 颜色/符号距离计算
5. `decode_fountain()` 前后的预处理

## 当前资产

- 热点模板：`benchmarks/templates/hotspot-profile-template.json`
- 汇总脚本：`tools/bench/rank_hotspots.py`

## 推荐路线

- `UMat / OpenCL`
  - 优先试在全图预处理和 `warpPerspective`
- `NEON`
  - 优先试 cell 距离计算、threshold、颜色距离
- `ROI-first`
  - 没有 ROI 命中率收益前，不做底层 SIMD 微优化

## 验收原则

- 只接受真实热点上的收益
- 不接受以结果漂移换来的“表面提速”
- 每条优化路径都要能回退

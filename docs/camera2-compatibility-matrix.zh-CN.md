# Camera2 兼容性矩阵

## 使用说明

本表用于记录 `Phase 1` 完成后不同设备在以下能力上的表现：

- 是否默认启用 `Camera2`
- 是否支持 `MANUAL_SENSOR`
- 是否支持 `MANUAL_POST_PROCESSING`
- 是否支持 `CONSTRAINED_HIGH_SPEED_VIDEO`
- 默认选中的 capture profile
- 是否需要通过 `files/force-legacy-camera` 强制回退旧路径

## 记录模板

| 设备 | Android 版本 | Camera2 默认 | MANUAL_SENSOR | MANUAL_POST_PROCESSING | HIGH_SPEED | 默认 profile | 回退需求 | 备注 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 示例设备 | 14 | 是 | 是 | 是 | 否 | `balanced` | 否 |  |

# cfc / libcimbar 传输速率提升执行状态

状态约定：

- `TODO`：尚未开始
- `IN_PROGRESS`：正在实施
- `DONE`：代码/文档已落地
- `PENDING_VALIDATION`：已落地，等待设备或样本验证
- `BLOCKED`：受外部条件限制

## Phase 总览

| Phase | 实施状态 | 验证状态 | 备注 |
| --- | --- | --- | --- |
| Phase 0 | `DONE` | `PENDING_VALIDATION` | 已补状态文档、仓库同步说明、会话日志骨架、基线目录和汇总脚本 |
| Phase 1 | `TODO` | `TODO` | 待切换 Camera2 主路径 |
| Phase 2 | `TODO` | `TODO` | 待建立 Y-only 和 ROI 快路径 |
| Phase 3 | `TODO` | `TODO` | 待 sender 节拍和 metadata 头部改造 |
| Phase 4 | `TODO` | `TODO` | 待多帧融合与会话级 profile |
| Phase 5 | `TODO` | `TODO` | 待高密度码型、ECC、专项加速 |

## Phase 0

| 任务 | 实施状态 | 验证状态 | 备注 |
| --- | --- | --- | --- |
| T0-1 仓库同步策略梳理 | `DONE` | `PENDING_VALIDATION` | 见 `repo-sync-strategy.zh-CN.md` |
| T0-2 统一埋点与会话日志 | `DONE` | `PENDING_VALIDATION` | 已补 Java 会话日志器和 JNI telemetry 快照接口 |
| T0-3 样本录制与离线回放基线 | `DONE` | `PENDING_VALIDATION` | 已补目录约定、场景说明和样本占位结构 |
| T0-4 基线报告 | `DONE` | `PENDING_VALIDATION` | 已补 baseline 模板和 CSV 汇总脚本，待真实设备数据填充 |

## Phase 1

| 任务 | 实施状态 | 验证状态 | 备注 |
| --- | --- | --- | --- |
| T1-1 Camera2 主路径接入 | `TODO` | `TODO` |  |
| T1-2 设备能力探测与 capture profile | `TODO` | `TODO` |  |
| T1-3 latest-frame-only 背压策略 | `TODO` | `TODO` |  |
| T1-4 3A 锁定和半手动控制 | `TODO` | `TODO` |  |
| T1-5 兼容性矩阵与回退策略 | `TODO` | `TODO` |  |

## Phase 2

| 任务 | 实施状态 | 验证状态 | 备注 |
| --- | --- | --- | --- |
| T2-1 Y 平面优先链路 | `TODO` | `TODO` |  |
| T2-2 ROI 快路径 | `TODO` | `TODO` |  |
| T2-3 autodetect 锁定机制 | `TODO` | `TODO` |  |
| T2-4 ROI 快路径回归样本 | `TODO` | `TODO` |  |

## Phase 3

| 任务 | 实施状态 | 验证状态 | 备注 |
| --- | --- | --- | --- |
| T3-1 sender 节拍稳定化 | `TODO` | `TODO` |  |
| T3-2 metadata / sync / pilot 头部设计 | `TODO` | `TODO` |  |
| T3-3 协议版本兼容与灰度发布 | `TODO` | `TODO` |  |
| T3-4 颜色校准与 pilot 区域强化 | `TODO` | `TODO` |  |

## Phase 4

| 任务 | 实施状态 | 验证状态 | 备注 |
| --- | --- | --- | --- |
| T4-1 多帧短窗口缓存 | `TODO` | `TODO` |  |
| T4-2 cell 级软信息聚合 | `TODO` | `TODO` |  |
| T4-3 融合后再进 RS / fountain | `TODO` | `TODO` |  |
| T4-4 会话级 bitrate ladder | `TODO` | `TODO` |  |
| T4-5 链路质量报告 | `TODO` | `TODO` |  |

## Phase 5

| 任务 | 实施状态 | 验证状态 | 备注 |
| --- | --- | --- | --- |
| T5-1 `Conf5x5 / Conf5x5d` 产品化 | `TODO` | `TODO` |  |
| T5-2 软信息友好的 ECC 研究型分支 | `TODO` | `TODO` |  |
| T5-3 SIMD / OpenCL / UMat 优化 | `TODO` | `TODO` |  |
| T5-4 高帧率专用分支 | `TODO` | `TODO` |  |

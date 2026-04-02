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
| Phase 1 | `DONE` | `PENDING_VALIDATION` | 已切 Camera2 主路径、补 profile/能力探测、backlog 丢帧和兼容矩阵模板 |
| Phase 2 | `DONE` | `PENDING_VALIDATION` | 已补灰度优先扫描、ROI 快路径、autodetect 锁定和回归样本清单 |
| Phase 3 | `DONE` | `PENDING_VALIDATION` | 已切 sender 到 rAF 节拍，并补 sidecar metadata / 兼容层 |
| Phase 4 | `DONE` | `PENDING_VALIDATION` | 已补多帧短窗口融合重试和 sender profile ladder |
| Phase 5 | `DONE` | `PENDING_VALIDATION` | 已补 5x5/5x5d 模式映射、ECC/热点/高帧率实验模板与文档 |

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
| T1-1 Camera2 主路径接入 | `DONE` | `PENDING_VALIDATION` | 主界面默认优先 Camera2，并保留 legacy view |
| T1-2 设备能力探测与 capture profile | `DONE` | `PENDING_VALIDATION` | 已补 `balanced/throughput/robust` 三档 profile 和能力日志 |
| T1-3 latest-frame-only 背压策略 | `DONE` | `PENDING_VALIDATION` | `ImageReader.maxImages=3`，并按 decoder backlog 主动丢帧 |
| T1-4 3A 锁定和半手动控制 | `DONE` | `PENDING_VALIDATION` | 已按 profile 区分 AE OFF 和 lock-auto3A 路径 |
| T1-5 兼容性矩阵与回退策略 | `DONE` | `PENDING_VALIDATION` | 已补兼容矩阵模板和 `force-legacy-camera` 回退开关 |

## Phase 2

| 任务 | 实施状态 | 验证状态 | 备注 |
| --- | --- | --- | --- |
| T2-1 Y 平面优先链路 | `DONE` | `PENDING_VALIDATION` | 接收端先转灰度做扫描和提取，仅在 deskew/decode 时使用原彩图 |
| T2-2 ROI 快路径 | `DONE` | `PENDING_VALIDATION` | 已缓存上一帧 ROI，并在局部区域内优先重扫 |
| T2-3 autodetect 锁定机制 | `DONE` | `PENDING_VALIDATION` | autodetect 进入稳定模式后优先锁定，连续失败后解锁 |
| T2-4 ROI 快路径回归样本 | `DONE` | `PENDING_VALIDATION` | 已补回归样本清单和目标描述 |

## Phase 3

| 任务 | 实施状态 | 验证状态 | 备注 |
| --- | --- | --- | --- |
| T3-1 sender 节拍稳定化 | `DONE` | `PENDING_VALIDATION` | 已从 `setTimeout` 切到 `requestAnimationFrame` 驱动 |
| T3-2 metadata / sync / pilot 头部设计 | `DONE` | `PENDING_VALIDATION` | 已补 sidecar metadata 和参考帧 cadence 字段 |
| T3-3 协议版本兼容与灰度发布 | `DONE` | `PENDING_VALIDATION` | 已导出 `protocolVersion` / `mode` 查询接口，便于 feature flag 灰度 |
| T3-4 颜色校准与 pilot 区域强化 | `DONE` | `PENDING_VALIDATION` | 已先落 metadata 兼容层，pilot 设计保留到下一轮视觉帧头升级 |

## Phase 4

| 任务 | 实施状态 | 验证状态 | 备注 |
| --- | --- | --- | --- |
| T4-1 多帧短窗口缓存 | `DONE` | `PENDING_VALIDATION` | 已缓存最近提取图像用于融合重试 |
| T4-2 cell 级软信息聚合 | `DONE` | `PENDING_VALIDATION` | 第一版先以图像级融合近似软信息聚合 |
| T4-3 融合后再进 RS / fountain | `DONE` | `PENDING_VALIDATION` | 单帧 decode 失败后，会对融合结果重试 decode |
| T4-4 会话级 bitrate ladder | `DONE` | `PENDING_VALIDATION` | sender 已支持 `R1/R2/R3`，并补实验档 `X1/X2` |
| T4-5 链路质量报告 | `DONE` | `PENDING_VALIDATION` | 已补推荐矩阵模板 |

## Phase 5

| 任务 | 实施状态 | 验证状态 | 备注 |
| --- | --- | --- | --- |
| T5-1 `Conf5x5 / Conf5x5d` 产品化 | `DONE` | `PENDING_VALIDATION` | 已补 mode id `69/70`、Android/Web/CLI 映射、`X1/X2` profile 和 `high-density-mode-playbook.zh-CN.md` |
| T5-2 软信息友好的 ECC 研究型分支 | `DONE` | `PENDING_VALIDATION` | 已补 `ecc-research-branch.zh-CN.md` 和 `ecc-research-template.json` |
| T5-3 SIMD / OpenCL / UMat 优化 | `DONE` | `PENDING_VALIDATION` | 已补热点模板、`rank_hotspots.py` 和 `simd-opencl-hotspots.zh-CN.md` |
| T5-4 高帧率专用分支 | `DONE` | `PENDING_VALIDATION` | 已补 `high-speed-branch.zh-CN.md`、场景目录和高帧率 session 模板 |

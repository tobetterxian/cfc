# Benchmark 目录说明

本目录用于承载 Phase 0 到 Phase 5 的离线样本、会话日志和基线/对比报告。

## 目录约定

- `benchmarks/scenarios/`
  - 手工录制或导入的测试场景说明
- `benchmarks/reports/`
  - 基线报告、阶段对比报告、设备兼容矩阵
- `benchmarks/templates/`
  - 报告模板和日志字段模板

Phase 5 额外约定：

- `benchmarks/scenarios/high-density/`
  - 5x5 / 5x5d 模式的样本清单与回放说明
- `benchmarks/scenarios/ecc/`
  - erasure-aware ECC 和软信息聚合实验场景
- `benchmarks/scenarios/high-speed/`
  - 高帧率专线机型与 session 记录

## 场景建议

至少准备以下场景：

- `bright-indoor`
- `normal-indoor`
- `low-light`
- `light-shake`
- `slight-angle`
- `brightness-variants`

## 会话日志位置

App 运行时会把每次会话的日志写到应用私有目录下：

- `<app-files>/benchmarks/sessions/<session-id>/frames.csv`
- `<app-files>/benchmarks/sessions/<session-id>/summary.json`

导出会话后，可用下面的脚本汇总：

```bash
python3 tools/bench/summarize_session.py /path/to/frames.csv
```

## 产出要求

每个报告至少包含：

- `goodput_kbps`
- `time_to_first_chunk_ms`
- `time_to_complete_file_ms`
- `scan_success_ratio`
- `decode_success_ratio`
- `avg_backlog`

Phase 5 建议额外记录：

- `mode_id`
- `autodetect_warmup_frames`
- `mixed_frame_utilization_ratio`
- `hotspot_topk`
- `high_speed_session_enabled`

# 会话级 bitrate ladder 说明

当前 sender 第一版提供三档 profile：

| Profile | 目标 | mode | FPS |
| --- | --- | --- | --- |
| `R1` | robust | `Bm` | `10` |
| `R2` | balanced | `B` | `15` |
| `R3` | high-throughput | `Bu` | `20` |

Phase 5 已额外补两档实验 profile：

| Profile | 目标 | mode | FPS |
| --- | --- | --- | --- |
| `X1` | high-density-safe | `5x5` | `18` |
| `X2` | high-density-max | `5x5d` | `16` |

## 当前行为

- `Main.setProfile('R1' | 'R2' | 'R3' | 'X1' | 'X2')`
- profile 会同时调整：
  - mode
  - sender FPS
  - colorBalance 开关

## 说明

这还是会话级 profile，不是实时双向闭环码率控制。  
它的目标是先把 sender 和 receiver 的配置档位收敛成稳定的几档。

`X1/X2` 仍然属于实验档，不建议直接作为默认 profile。

# 5x5 / 5x5d 高密度模式说明

## 目标

把 `Conf5x5` 和 `Conf5x5d` 从实验配置提升为仓库内可显式选择、可验证、可回退的模式。

## 当前落地

| mode id | 模式名 | 用途 | 当前入口 |
| --- | --- | --- | --- |
| `68` | `B` | 主线基线模式 | Android / Web / CLI |
| `69` | `5x5` | 高密度实验模式 | Android autodetect、Web sender/receiver、CLI |
| `70` | `5x5d` | 更激进的高密度实验模式 | Android autodetect、Web sender/receiver、CLI |

## 代码入口

- `Config.h`
  - `69 -> Conf5x5`
  - `70 -> Conf5x5d`
- `web/main.js`
  - 新增 sender 菜单项：`5x5`、`5x5d`
  - 新增实验 profile：`X1`、`X2`
- `web/recv.js` / `web/recv.html`
  - 新增 receiver 手动切换项：`5x5`、`5x5d`
  - auto probe 在主线 4 模式 warmup 后，追加高密度模式探测
- `MultiThreadedDecoder.h`
  - Android 接收端 autodetect 先尝试主线模式，超过 warmup 帧后再探测 `69/70`
- CLI
  - `cimbar`
  - `cimbar_send`
  - `cimbar_recv`
  - `cimbar_recv2`

## profile 约定

| Profile | mode | FPS | 说明 |
| --- | --- | --- | --- |
| `X1` | `5x5` | `18` | 高密度保守档 |
| `X2` | `5x5d` | `16` | 高密度激进档，优先用于实验室稳定链路 |

## 默认策略

- 主线默认仍然使用 `R1/R2/R3`
- `X1/X2` 不作为默认 profile 自动启用
- Android autodetect 只有在主线模式连续未命中后，才追加 `69/70`

## 验证重点

- 比较 `68`、`69`、`70` 的净 `goodput_kbps`
- 记录高密度模式下的首帧锁定时间
- 记录是否出现 ROI 锁定抖动、颜色漂移或 mixed frame 增加

## 回退条件

出现以下任一情况时，维持 `68` 作为默认模式：

- `goodput_kbps` 没有稳定超过 `B`
- `time_to_first_chunk_ms` 明显恶化
- 解码稳定性依赖过于严格的光照、角度或快门条件

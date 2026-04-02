# 协议 metadata sidecar 说明

当前主线尚未直接修改 cimbar 视觉帧内部的固定 header 布局。  
为了先完成 `Phase 3` 的兼容层，当前实现先引入了一层 sidecar metadata：

- `protocolVersion`
- `mode`
- `frameCounter`
- `frameCount`
- `frameIntervalMs`
- `syncEvery`
- `isReferenceFrame`

## 当前承载位置

sidecar metadata 目前通过以下方式暴露：

- `window.CIMBAR_PROTOCOL_METADATA`
- `#nav-container` 的 `data-*` 属性
- `cimbare_get_mode()`
- `cimbare_get_protocol_version()`

## 作用

- sender 节拍统计
- 新旧 sender/receiver 的 feature flag 兼容
- 后续视觉帧头升级前的过渡层

## 后续计划

下一步如果要把 metadata 真正放进视觉帧结构，建议优先考虑：

1. 保持 fountain payload 布局向后兼容
2. 先引入 `protocol version + mode id + sync bits`
3. 再补 `pilot/palette` 和更强的颜色校准字段

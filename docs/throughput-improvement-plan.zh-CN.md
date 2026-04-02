# cfc / libcimbar 传输速率提升方案

## 1. 背景与目标

本文面向以下系统组合：

- 接收端：`/mnt/c/next/cfc`
- 编解码核心：`/mnt/c/next/libcimbar`

目标不是单纯追求实验室峰值，而是在现有“显示器/手机摄像头/离线单向传输”约束下，系统性提高以下指标：

- 有效吞吐量：`KB/s` 或 `Mbps`
- 首字节时间：从开始对准到首次稳定解出 fountain payload 的时间
- 完整文件时间：到文件落盘所需总时间
- 低光、轻微抖动、轻微偏角下的稳定性
- 不同手机间的一致性

结合现有代码和已有公开结果，建议把目标拆成三档：

- 短期目标：在现有码制不大改的前提下，实现 `1.3x ~ 1.8x` 的平均吞吐提升
- 中期目标：通过接收链路重构和自适应编码，实现 `2.0x ~ 2.8x` 的平均吞吐提升
- 长期目标：引入新码型/新 ECC/高帧率专用路径，冲击 `3x+` 的提升，但需要更高研发风险和设备约束

## 2. 当前系统现状

### 2.1 现有链路

当前链路大致是：

`文件 -> zstd -> fountain/wirehair -> cimbar 帧 -> 屏幕播放 -> 手机相机采集 -> scan/extract -> symbol/color decode -> RS -> fountain sink -> zstd 解压`

`libcimbar` 已经具备以下几个对吞吐友好的基础能力：

- `zstd` 压缩
- `wirehair` fountain code
- 多种 mode 配置
- 分离 symbol/color 解码
- 多线程接收端解码

按仓库文档，当前公开基线大致是：

- `mode B`，`8x8`，`4-color`，`ecc=30/155`
- 单帧有效载荷约 `7500 bytes`
- 公开 benchmark 约 `852 kbps`，约 `106 KB/s`

这说明系统的“码制本身”并不差，真正限制吞吐的更可能是接收端采集与同步，而不是纯 CPU 解码。

### 2.2 从代码看出的关键事实

#### A. 当前 Android 主路径实际仍在使用旧 Camera API

主布局绑定的是 `OpencvCameraView`，不是 `OpencvCamera2View`：

- `cfc/app/src/main/res/layout/activity_main.xml`

这意味着当前真正跑在主流程上的相机链路仍是旧 `android.hardware.Camera` 预览路径，而不是更易做手动曝光、帧时长控制、缓冲策略优化的 `Camera2`。

#### B. 仓库里已经有 Camera2 原型，但没有接入主界面

`OpencvCamera2View` 已实现：

- `ImageReader.acquireLatestImage()`
- `CONTROL_AE_MODE_OFF`
- `CONTROL_AWB_MODE_OFF`
- `SENSOR_FRAME_DURATION`
- `SENSOR_EXPOSURE_TIME`

但这条链路没有被主布局启用，因此目前这些手动控制并没有实际贡献吞吐。

#### C. 现有接收端每帧都做全图 scan + deskew

`MultiThreadedDecoder` 的主流程是：

- 对每帧新图 `Scanner.scan()`
- 找到四角后再 `Deskewer.deskew()`
- 然后做 `decode_fountain()`

也就是说，一旦相机已经对准并稳定，系统依旧没有“沿用上一帧 ROI/角点/透视矩阵”的快路径，导致大量重复视觉开销。

#### D. autodetect 通过轮询 mode 工作，会浪费有效帧

当模式为自动检测时，`MultiThreadedDecoder::add()` 会在 `4 / 66 / 67 / 68` 四种 mode 间轮换尝试。  
这在首次探测时是合理的，但在链路已经稳定后，如果仍长时间轮询，就会降低有效 payload 利用率并增加误判成本。

#### E. 当前多帧信息没有被真正融合

`libcimbar/TODO.md` 已明确提出 multi-frame decoding 是后续方向。  
从实现上看，当前 frame 的 scan / extract / decode 仍基本是“每帧独立完成，成功就写 sink，失败就丢弃”。

这意味着系统已经有 fountain 级别的跨帧恢复能力，但缺少“同一帧视觉观测层”的跨帧融合能力。

#### F. 配置目前仍偏静态

`Config` / `GridConf` 中当前主力配置仍是固定的：

- `Conf8x8`
- `Conf8x8_micro`
- `Conf8x8_mini`
- 实验性 `Conf5x5` / `Conf5x5d`

其中 `5x5` 路径已经存在，但尚未真正进入主流程产品化。

## 3. 当前主要瓶颈排序

结合代码、仓库说明以及业界公开实践，当前吞吐瓶颈建议按以下优先级判断：

### P0 级瓶颈：采集稳定性与有效帧率

这是最先该解决的问题。

原因：

- 仓库自己的 `PERFORMANCE.md` 已指出，现代 CPU 更快并不必然显著提升吞吐，相机通常才是瓶颈
- 目前主链路仍走旧 Camera API
- 自动曝光、自动白平衡、自动对焦会让帧时间、亮度和颜色在时间轴上不稳定
- 旧 API 下可控性差，设备差异更大

### P1 级瓶颈：每帧都做完整视觉定位，缺少 ROI 跟踪

一旦用户已经把码框稳定对准屏幕，继续对整张图做完整搜索并不经济。  
当前系统没有明显利用“前一帧已经知道码大概在哪里”的事实。

### P1 级瓶颈：缺少 mixed / imperfect frame 的利用能力

当前体系基本把“中间态帧、混合帧、轻微撕裂帧”当坏帧处理。  
而高吞吐屏幕-相机通信里，真正的优秀实践通常会：

- 尽量让 imperfect frame 也产生部分有效信息
- 通过跨帧 erasure / rateless / soft information 恢复数据

### P1 级瓶颈：模式/ECC/冗余不做链路自适应

现在 mode 和 ECC 更接近“启动时选定”而不是“根据链路质量动态切换”。  
这会导致：

- 好链路上保守，白白损失吞吐
- 差链路上激进，重传/恢复成本增大

### P2 级瓶颈：码型密度和 ECC 仍有改进空间

这是重要方向，但不是第一优先。

原因：

- 现有 8x8 mode 已能到接近 `1 Mbps`
- 代码中实验性 `5x5` 已存在，说明“更高密度”不是零起点
- 但若采集端不稳定，码型越激进，吞吐反而可能下降

## 4. 可借鉴的业界/学术优秀实践

下面只列与本项目直接相关、且能转化为工程动作的做法。

### 4.1 相机侧：优先保证“稳定且最新”的帧，而不是盲目保留所有帧

Android 官方文档明确建议实时处理场景优先使用 `ImageReader.acquireLatestImage()`，并主动丢弃旧帧，以避免处理链路越来越滞后。  
这与高吞吐视觉通信的需求完全一致：我们关心“当前最新可解的帧”，而不是历史积压帧。

对应到本项目，意味着：

- 主接收链路应切到 `Camera2 + ImageReader`
- `maxImages` 保持小而足够，一般 `3` 就够
- 后台线程只处理最新帧
- 一旦 decode backlog 增长，就主动丢老帧而不是排队等死

### 4.2 相机控制：手动/锁定 3A 是吞吐提升的基础设施

Android 官方文档说明，当 `AE` 关闭时，应用设置的 `sensor.exposureTime`、`sensor.sensitivity` 和 `sensor.frameDuration` 会生效；同时建议在关闭 AE 之前关闭或锁定 AWB/AF，以保证跨设备行为更一致。

这对本项目非常关键：

- 更短、稳定的曝光意味着更高的有效采样频率
- 稳定 AWB 能降低 color decode 漂移
- 稳定 AF 能减少局部虚焦造成的 tile 判决波动

结论很明确：  
在消费级手机上想提高视觉链路吞吐，`Camera2` 的手动或半手动控制不是“锦上添花”，而是第一优先级。

### 4.3 不要浪费 imperfect frame，要做帧内跟踪和跨帧恢复

LightSync 的核心做法非常值得借鉴：

- 用帧内跟踪去解 mixed / heterogeneous frame
- 用跨帧线性 erasure code 恢复丢失帧
- 让系统在“显示帧率高于相机采样率”时仍能工作

这与当前 `cfc/libcimbar` 的状态非常契合：

- 你们已经有 fountain / rateless 基础
- 但缺乏 mixed frame 的视觉利用能力

也就是说，现有系统距离“优秀实践”差的不是整体架构，而是中间那层“视觉层的软解码和跨帧复用”。

### 4.4 自适应码率与软信息，比固定 ECC 更有效

SoftLight 的结论很直接：

- 链路质量在不同环境下差异很大
- 需要根据链路质量自动调节传输速率
- 软信息 + rateless coding 能明显提升平均 goodput

它给本项目的启示是：

- 不要把 `mode / ecc / 冗余` 当静态参数
- 应该利用当前解码中已经存在的“distance/confidence”信息
- 对低置信度 cell / block 做 erasure 化处理，比一律硬判更划算

### 4.5 色彩与环境光校正必须是协议的一部分，而不是后处理运气

Parikh/Jancke 的 HCCB 工作和 CALC 都指向同一个工程事实：

- 颜色码制的难点从来不是“能不能编码颜色”
- 而是相机白平衡、光照、角度、模糊和颜色漂移会把码字空间压扁

可借鉴点：

- 固定 palette / pilot 区域
- 自适应归一化
- 用已知颜色块做 color assignment / calibration
- 环境光校正与背景扣除

你们现在已经有：

- anchor
- color correction
- `init_ccm()`

但协议层的 metadata / pilot 仍不够强，导致仍需要 mode 轮询和推断。

### 4.6 “渐进式视觉解码 + decoder 反馈”比单一重模型更实用

Parikh/Jancke 还提出了一个很适合当前代码库的思路：

- 使用多个便宜的定位/分割策略
- 由 decoder success/failure 反向驱动下一步策略
- 用 progressive strategy 在精度和计算量之间取平衡

这与 `libcimbar` 当前 scan/extract/decode 分层非常贴合。  
本项目不需要一上来就引入重 CNN，完全可以先把“渐进式快路径 + 失败回退 + 反馈驱动”工程化。

### 4.7 ROI 跟踪、部分像素采样、多线程流水线是成熟路径

HiLight 在接收端采用：

- 只取部分像素做解码
- 多线程分离不同步骤
- 根据内容变化做自适应

OpenCV 官方也明确说明：

- `cv::UMat` 和 Transparent API 可在运行时选择 CPU 或 OpenCL 路径

对本项目来说，这意味着两条务实路线：

- 先通过 ROI 缩小工作集，减少必须处理的像素数
- 再考虑 `UMat/OpenCL` 或 ARM NEON 做加速

先缩小输入，再做加速，性价比最高。

## 5. 面向本项目的落地方案

## 5.1 第一阶段：先把接收端“拍稳、看准、少浪费”

周期建议：`2 ~ 4 周`

### 方案 1：把主路径切到 Camera2，并建立能力协商

动作：

- 用 `OpencvCamera2View` 替换主布局中的 `OpencvCameraView`
- 启动时查询：
  - `MANUAL_SENSOR`
  - `MANUAL_POST_PROCESSING`
  - `CONSTRAINED_HIGH_SPEED_VIDEO`
  - 支持的输出尺寸
  - 支持的 FPS 范围
- 建立多档 capture profile

建议 profile：

- `balanced`
  - 目标 `30 fps`
  - 曝光时间 `8 ~ 16 ms`
  - 优先稳定白平衡/曝光
- `throughput`
  - 目标 `45 ~ 60 fps`
  - 曝光时间尽量压到 `8 ~ 10 ms`
  - 允许 ISO 上升
- `robust`
  - 目标 `24 ~ 30 fps`
  - 在低光条件下降低 fps 换更稳的单帧质量

优先实现策略：

- 先走“AE/AWB/AF 先自动收敛，然后 lock”
- 设备支持足够好时，再开放全手动 `AE_OFF + AWB_OFF`

原因：

- 全手动在某些机型兼容性一般
- 先 lock，再逐步过渡到 full manual，更稳妥

### 方案 2：保留 `acquireLatestImage` 语义，整个链路只处理最新帧

动作：

- `ImageReader.maxImages = 3`
- 新帧到达时始终拿最新帧
- 后端线程池 backlog 超阈值时直接丢旧帧
- UI 和 decoder 分离，避免 UI 线程被 JNI decode 拖慢

目标：

- 不让“旧帧积压”把延迟变成吞吐杀手

### 方案 3：解码链路改成“Y 平面优先，颜色延后”

当前 Android 侧会把预览帧转成 RGBA，再交给后续处理。  
对 cimbar 来说，更合理的做法是：

- scan / extract / symbol decode 只用 luma
- 只有进入 color decode 时才读取 chroma / RGB 信息

收益：

- 降低内存带宽
- 降低颜色转换成本
- 更适合高帧率路径

### 方案 4：加 ROI 跟踪快路径，避免每帧全图扫描

建议把接收端改成两级路径：

- 快路径
  - 已经成功定位过一次后，沿用上一帧的角点/单应矩阵
  - 在局部区域内做微调
- 慢路径
  - 快路径连续失败 `N` 次
  - 或画面大幅运动
  - 或尺度变化超过阈值
  - 再回退到全图 `Scanner.scan()`

可选实现：

- KLT/光流跟踪四个角点
- anchor 小块模板匹配
- 用上一帧 homography 预测当前 ROI

优先级建议：

- 先做“上一帧 ROI 扩边 + 局部重扫”
- 再做更复杂的角点跟踪

### 方案 5：把 autodetect 从“每帧轮询”改成“短探测后锁定”

建议逻辑：

- 会话开始的前 `M` 帧做 mode 探测
- 一旦某 mode 连续解出有效 chunk，进入 lock
- 只有在连续失败或 metadata 明确变更时才重新探测

收益：

- 减少无效 mode 尝试
- 降低误判和 cache 抖动
- 提高稳定阶段的有效吞吐

### 第一阶段预期收益

在不改 cimbar 码制的前提下，第一阶段主要吃到的是：

- 更稳定帧率
- 更低帧间色彩/曝光抖动
- 更少无效 scan
- 更少无效 mode 尝试

预计平均收益：

- `1.3x ~ 1.8x`

## 5.2 第二阶段：让“坏帧也有价值”，并做链路自适应

周期建议：`4 ~ 8 周`

### 方案 6：做多帧融合，不再把每帧当独立事件

现有 `distance/confidence`、`drift`、cell 位置等信息已经足够支持一个轻量级多帧融合器。

建议做法：

- 对同一候选 frame id 建一个短窗口缓存
- 每个 cell 保留：
  - 最佳 symbol 候选
  - 次佳候选
  - 置信度
  - 颜色观测均值
- 单帧 decode 不足阈值时，用最近 `2 ~ 5` 帧做合并后再跑 RS / fountain

这一步不是替代 fountain，而是发生在 fountain 之前的“视觉层融合”。

收益：

- 提高弱光、轻微抖动、局部遮挡时的有效帧利用率
- 让 previously useless frame 变成 partial evidence

### 方案 7：给协议补 metadata / sync / pilot

建议在帧中保留固定区域承载：

- mode id
- ecc profile
- frame id / generation id
- fountain chunk size
- color palette / pilot colors
- sync bits

这样可以：

- 去掉 mode brute-force
- 更快做颜色校正
- 支持 mixed-frame 判定
- 支持跨帧对齐

实现建议：

- 优先在边缘或固定角落加小 header 带
- 不动主 payload 网格的核心布局
- 先做向后兼容版本号

### 方案 8：建立自适应 bitrate ladder

建议定义三档到四档 profile：

- `R1 robust`
  - 更高 ecc
  - 更低 payload 密度
  - 更高冗余
- `R2 balanced`
  - 默认配置
- `R3 high-throughput`
  - 较低 ecc
  - 较高 payload 密度
- `R4 lab-only`
  - 仅在高亮、高稳设备上启用

切换依据：

- 最近 `N` 帧 scan success rate
- decode success rate
- perfect frame ratio
- 平均 cell confidence
- 颜色漂移幅度
- 曝光/亮度抖动

可先只做 session 级别切换：

- 进入会话后 1~2 秒评估链路
- 选定档位并保持

后续再做真正在线切换。

### 方案 9：让 sender 按 vsync / 刷新节拍发，而不是粗粒度定时器

当前 Web sender 主要还是 `setTimeout` 驱动。  
更稳妥的高吞吐实践是：

- 尽量贴近显示刷新节拍
- 在内容切换时插入同步/参考帧
- 避免浏览器计时器抖动导致“伪 mixed frame”

建议：

- Web sender 改成 `requestAnimationFrame + display timing` 统计
- 原生 sender 支持基于 vsync 的 frame submit
- 每 `K` 帧发一个轻量 sync/reference frame

### 方案 10：内容自适应渲染

可以借鉴 HiLight/Parikh/CALC 的思想，对发送图形做更强的工程适配：

- 强化白边/外边界亮度
- 提供明确 palette/pilot 区
- 根据背景亮度切换黑/白参考层
- 对易混淆颜色做场景回避

注意：

- 这不是去做“隐藏通信”
- 而是利用已知显示条件提高 camera readability

### 第二阶段预期收益

如果第一阶段完成良好，第二阶段是最有机会把吞吐进一步显著拉高的一段：

- `1.5x ~ 2.5x` 相对第一阶段
- 相对当前主线总体可望达到 `2.0x ~ 2.8x`

## 5.3 第三阶段：升级码制和纠错，但放在后面做

周期建议：`8+ 周`

### 方案 11：把 `Conf5x5 / Conf5x5d` 产品化

目前代码中已有实验性 `5x5` 配置，但还没有贯通到产品主流程。

建议顺序：

- 先把 runtime config 做干净
- 再把 `5x5` 做成明确 mode
- 最后补对应 metadata、测试集、自动切换策略

风险：

- cell 更小后，对焦、运动模糊、屏幕子像素结构更敏感
- 需要更强的 ROI 稳定和颜色校正

### 方案 12：从“硬判决 + RS”走向“软信息 + 更现代 ECC”

当前系统的一个结构性问题是：

- 视觉层大多还是硬判决
- RS 按字节纠错
- 实际视觉错误常常是 bit 级、局部相关、带置信度的

长期建议：

- 保留 fountain 作为 session 层 rateless
- 在 frame 内引入 erasure-aware block decoding
- 研究 LDPC / RaptorQ / soft-decision RS 的可行性

这部分收益可能很高，但实现复杂度和验证成本也最高。

### 方案 13：GPU / OpenCL / NEON 专项加速

优先级应低于“缩小工作集”和“减少无效计算”。  
等前两阶段完成后，再做以下加速更有意义：

- threshold / blur / warpPerspective 使用 `UMat`
- Android ARM64 路径做 NEON 优化
- symbol hash / Hamming distance 做批量 SIMD

注意：

- 如果继续每帧全图 scan，纯 GPU 加速通常不会带来预期中的数量级提升
- 先做 ROI，再做加速，收益最大

### 方案 14：高帧率专用分支

如果目标设备明确、型号可控，可以做单独分支：

- 检测 `CONSTRAINED_HIGH_SPEED_VIDEO`
- 建高帧率 session
- 为其设计专用 sender profile 和更小 cell / 更低冗余 profile

但这条路不要作为默认主线，因为：

- 官方文档说明该模式约束很多
- 通常会强制 AE/AWB/AF 回到 auto / fast 路径
- 设备兼容性和稳定性差异较大

它更适合：

- 内部实验
- 指定机型部署
- 展示型场景

## 6. 建议的实施优先级

### 第 1 批，立即开始

- 主路径切换到 `Camera2`
- 能力探测与 capture profile
- `latest-frame-only` 背压策略
- autodetect 锁定机制
- 统一埋点和基准测试

### 第 2 批，紧接着做

- ROI 快路径
- 局部重扫回退
- Y-only 早期路径
- sender 节拍稳定化

### 第 3 批，中期重点

- 多帧融合
- metadata / pilot / sync
- 自适应码率 ladder

### 第 4 批，长期研究

- 5x5 产品化
- 新 ECC
- GPU / SIMD 深化
- 高帧率专用模式

## 7. 建议的实验与验收方法

## 7.1 必须先补统一埋点

建议每帧记录：

- capture timestamp
- camera fps
- exposure time
- ISO / sensitivity
- white balance gains 或是否锁定
- scan success
- extract success
- decode bytes
- perfect frame
- mode
- backlog
- end-to-end latency

输出方式：

- 本地 CSV
- Logcat 摘要
- 每个会话输出一份 JSON summary

## 7.2 基准场景

至少覆盖：

- 明亮室内
- 普通室内
- 弱光室内
- 手持轻微抖动
- 轻微偏角
- 不同屏幕亮度
- 不同手机机型

## 7.3 最重要的 KPI

建议优先看四个：

- `goodput_kbps`
- `time_to_first_chunk_ms`
- `time_to_complete_file_ms`
- `decode_success_ratio`

其次再看：

- `scan_success_ratio`
- `mixed_frame_utilization_ratio`
- `avg_backlog`
- `color_confidence_drift`

## 8. 结论

对这个项目来说，吞吐提升的正确顺序不是“先换新码型”，而是：

1. 先把接收端采集与帧处理路径稳定下来
2. 再让 mixed / imperfect frame 产生价值
3. 然后做协议层 metadata 与自适应
4. 最后再上更激进的码型与 ECC

核心判断如下：

- 现有 `libcimbar` 的码制和 fountain 架构已经具备较强基础
- 当前最大短板在 `cfc` 的相机与视觉接收链路
- 最值得先做的不是“重写编码器”，而是“把每一帧拍稳、用满、少浪费”
- 如果第一阶段做好，后续很多研究型改动才会真正兑现为吞吐提升

换句话说，短期内最值钱的工作是“系统工程”，不是“理论上更密的码”。

## 9. 参考资料

以下资料直接支撑了本文中的关键判断：

1. Android Developers, `ImageReader.acquireLatestImage()`  
   https://developer.android.com/reference/android/media/ImageReader

2. Android Developers, `CameraMetadata / CaptureRequest` manual sensor controls  
   https://developer.android.com/reference/android/hardware/camera2/CameraMetadata

3. Android Developers, constrained high speed video capability  
   https://developer.android.com/reference/android/hardware/camera2/CameraConstrainedHighSpeedCaptureSession

4. OpenCV Transparent API / `cv::UMat`  
   https://docs.opencv.org/3.4/db/dfa/tutorial_transition_guide.html

5. Wenjun Hu, Hao Gu, Qifan Pu, LightSync: Unsynchronized Visual Communication over Screen-Camera Links, MobiCom 2013  
   https://picture.iczhiku.com/resource/paper/wHkETZwequTUeMvn.pdf

6. Wan Du, Jansen Christian Liando, Mo Li, Soft hint enabled adaptive visible light communication over screen-camera links, IEEE TMC 2016  
   https://jansencl.github.io/publication/2016-04-07_TMC-2016

7. Tianxing Li et al., Real-Time Screen-Camera Communication Behind Any Scene, MobiSys 2015  
   https://www.cs.columbia.edu/~xia/publication/mobisys15-hilight/mobisys15-hilight.pdf

8. Devi Parikh, Gavin Jancke, Localization and Segmentation of a 2D High Capacity Color Barcode, WACV 2008  
   https://deviparikh.com/publications/ParikhJancke_WACV_2008_barcodes_update.pdf

9. Kunyu Sun et al., CALC: Calibration for Ambient Light Correction in Screen-to-Camera Visible Light Communication, Results in Optics 2021  
   https://doi.org/10.1016/j.rio.2021.100122

10. Kensei Jo, Mohit Gupta, Shree K. Nayar, DisCo: Display-Camera Communication Using Rolling Shutter Sensors, TOG 2016  
    https://cave.cs.columbia.edu/old/publications/pdfs/Jo_TOG16.pdf

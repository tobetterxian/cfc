# cfc / libcimbar 传输速率提升分阶段任务清单

## 1. 文档目的

本文把 [throughput-improvement-plan.zh-CN.md](/mnt/c/next/cfc/docs/throughput-improvement-plan.zh-CN.md) 进一步细化为可执行的任务列表，覆盖以下两个配套仓库：

- 接收端应用：`/mnt/c/next/cfc`
- 编解码核心：`/mnt/c/next/libcimbar`

本文默认的执行原则是：

- 先补度量，再做优化
- 先稳住采集链路，再提高码密度
- 先减少无效计算，再考虑 GPU/SIMD 加速
- 每个阶段都要有明确的准入条件、准出条件和回退策略

## 2. 使用说明

建议把本文作为研发排期和验收的主文档来使用：

- 每个阶段开始前，先确认“前置依赖”已经满足
- 每个阶段结束时，只按“验收标准”判断是否进入下一阶段
- 如果某阶段核心指标未达标，不要提前进入后面的高风险码制优化

建议任务责任域分为四类：

- `Android`：相机、渲染、JNI 入口、应用层埋点
- `C++/Codec`：`scan/extract/decode`、协议、ECC、`libcimbar`
- `Web/Sender`：发送端节拍、帧布局、浏览器侧渲染
- `QA/Bench`：样本采集、回放、基准测试、数据归档

## 3. 里程碑总览

| 阶段 | 周期建议 | 目标 | 主要收益 | 是否必须 |
| --- | --- | --- | --- | --- |
| Phase 0 | 1 ~ 2 周 | 建立基线、埋点、回放与仓库同步策略 | 为后续优化提供可验证基线 | 是 |
| Phase 1 | 2 ~ 4 周 | 稳定相机采集链路 | 提升有效帧率、降低帧间抖动 | 是 |
| Phase 2 | 2 ~ 4 周 | 建立接收端快路径 | 降低 CPU/内存开销，减少无效扫描 | 是 |
| Phase 3 | 3 ~ 5 周 | 稳定 sender 节拍与协议显式信息 | 提高链路同步和模式确定性 | 是 |
| Phase 4 | 4 ~ 8 周 | 多帧融合和会话级自适应 | 让坏帧也产生价值，提升 goodput | 建议 |
| Phase 5 | 6+ 周 | 高密度码型、ECC、SIMD/GPU、高帧率专线 | 冲击实验室上限 | 可选 |

## 4. Phase 0：基线与工程基础

### 4.1 阶段目标

- 明确 `cfc` 与 `/mnt/c/next/libcimbar` 的协作边界
- 建立统一埋点、录制样本和回放基线
- 用同一套指标衡量后续每次优化收益

### 4.2 准入条件

- 当前仓库可以正常打开和修改
- 团队认可本文作为阶段排期依据

### 4.3 任务列表

#### T0-1 仓库同步策略梳理

- 优先级：`P0`
- 责任域：`Android` + `C++/Codec`
- 前置依赖：无
- 目标：明确 `/mnt/c/next/libcimbar` 与 `cfc/app/src/cpp/libcimbar` 的主从关系，避免后续优化做在错误位置
- 涉及模块：
  - `cfc/app/src/cpp/libcimbar`
  - `/mnt/c/next/libcimbar`
- 具体工作：
  - 盘点 `cfc` 中 vendored `libcimbar` 的来源和同步方式
  - 明确后续协议和解码逻辑以哪个仓库为 source of truth
  - 约定同步流程，例如“先改上游，再同步到 app 内嵌副本”
- 交付物：
  - 一页仓库同步说明
  - 一份“哪些改动必须在上游做”的清单
- 验收标准：
  - 后续每个任务都能明确写出改动是在 `cfc` 还是 `/mnt/c/next/libcimbar`
  - 不再出现双份逻辑无序漂移的情况

#### T0-2 统一埋点与会话日志

- 优先级：`P0`
- 责任域：`Android` + `C++/Codec`
- 前置依赖：`T0-1`
- 目标：让采集、扫描、解码、落盘全链路都有统一时间轴
- 涉及模块：
  - `cfc/app/src/main/java/org/cimbar/camerafilecopy/*`
  - `cfc/app/src/cpp/cfc-cpp/*`
- 具体工作：
  - 每帧记录 `capture timestamp`
  - 记录 `camera fps`、曝光时间、ISO、AF/AE/AWB 状态
  - 记录 `scan success`、`extract success`、`decode bytes`、`perfect frame`
  - 记录线程池 `backlog`、文件完成度、端到端延迟
  - 生成 `CSV` 和会话级 `JSON summary`
- 交付物：
  - 埋点字段清单
  - 一份标准日志样例
- 验收标准：
  - 能从日志重建一条完整会话的时间线
  - 不改业务逻辑时，多次跑同一场景日志字段保持稳定

#### T0-3 样本录制与离线回放基线

- 优先级：`P0`
- 责任域：`QA/Bench` + `Android`
- 前置依赖：`T0-2`
- 目标：建立不依赖实时手持操作的可复现实验流程
- 涉及模块：
  - `cfc` 相机输入侧
  - `/mnt/c/next/libcimbar` 解码路径
- 具体工作：
  - 录制至少一组明亮、普通室内、弱光、轻抖动、轻偏角样本
  - 设计“实时采集”和“离线回放”两套测试流程
  - 保留原始帧样本和对应 ground truth 输出文件
- 交付物：
  - 样本目录结构约定
  - 场景说明文档
  - 一组最小可用基准样本
- 验收标准：
  - 同一版本在相同样本上结果可重复
  - 后续阶段可以离线比较优化前后的差异

#### T0-4 基线报告

- 优先级：`P0`
- 责任域：`QA/Bench`
- 前置依赖：`T0-2`、`T0-3`
- 目标：形成后续所有优化的比较基线
- 具体工作：
  - 输出当前版本的 `goodput_kbps`
  - 记录 `time_to_first_chunk_ms`
  - 记录 `time_to_complete_file_ms`
  - 记录 `scan_success_ratio` 和 `decode_success_ratio`
  - 按设备、光照、角度维度汇总
- 交付物：
  - 一份 baseline 报告
- 验收标准：
  - 后续每次阶段验收都能与 baseline 对比

### 4.4 阶段准出条件

- 有明确的仓库同步规则
- 有统一埋点格式
- 有可复现的录制样本和基线报告

## 5. Phase 1：相机采集链路稳定化

### 5.1 阶段目标

- 把主接收链路从旧 Camera API 迁移到 `Camera2`
- 把“处理最新帧”而不是“处理全部帧”变成明确策略
- 稳定曝光、白平衡和对焦行为，减少帧间抖动

### 5.2 准入条件

- `Phase 0` 完成
- 具备至少两台测试机型做兼容性验证

### 5.3 任务列表

#### T1-1 Camera2 主路径接入

- 优先级：`P0`
- 责任域：`Android`
- 前置依赖：`T0-2`
- 目标：让主界面默认走 `OpencvCamera2View`
- 涉及模块：
  - `cfc/app/src/main/res/layout/activity_main.xml`
  - `cfc/app/src/main/java/org/cimbar/camerafilecopy/OpencvCamera2View.java`
  - `cfc/app/src/main/java/org/cimbar/camerafilecopy/MainActivity.java`
- 具体工作：
  - 以 feature flag 方式替换主相机视图
  - 保留旧 `OpencvCameraView` 作为回退路径
  - 补齐生命周期、权限、异常处理
- 交付物：
  - `Camera2` 默认主路径
  - 可切回旧相机的调试开关
- 验收标准：
  - 主流程可稳定打开相机、预览、关闭
  - 至少两台设备能完成基本文件接收

#### T1-2 设备能力探测与 capture profile

- 优先级：`P0`
- 责任域：`Android`
- 前置依赖：`T1-1`
- 目标：根据设备能力选择合理的采集策略，而不是一刀切
- 涉及模块：
  - `OpencvCamera2View.java`
  - 设备能力探测相关辅助类
- 具体工作：
  - 检测 `MANUAL_SENSOR`
  - 检测 `MANUAL_POST_PROCESSING`
  - 检测 `CONSTRAINED_HIGH_SPEED_VIDEO`
  - 构建 `balanced`、`throughput`、`robust` 三档 profile
  - 记录每台设备实际生效的参数
- 交付物：
  - 能力探测结果结构体
  - capture profile 配置表
- 验收标准：
  - 运行时日志能明确看到当前设备能力和所选 profile
  - 不支持手动控制的设备能自动降级

#### T1-3 latest-frame-only 背压策略

- 优先级：`P0`
- 责任域：`Android`
- 前置依赖：`T1-1`
- 目标：彻底消除旧帧积压导致的高延迟和无效解码
- 涉及模块：
  - `OpencvCamera2View.java`
  - JNI 入口
  - `MultiThreadedDecoder`
- 具体工作：
  - `ImageReader.maxImages = 3`
  - 明确使用 `acquireLatestImage()`
  - 当 `backlog` 超过阈值时丢弃旧帧
  - 记录丢帧原因和计数
- 交付物：
  - 新的背压和丢帧策略
  - 丢帧统计日志
- 验收标准：
  - 长时间运行时 `backlog` 不再持续增长
  - `time_to_first_chunk_ms` 和 `goodput_kbps` 明显优于旧路径

#### T1-4 3A 锁定和半手动控制

- 优先级：`P1`
- 责任域：`Android`
- 前置依赖：`T1-2`
- 目标：减少曝光、白平衡、对焦在时间轴上的波动
- 涉及模块：
  - `OpencvCamera2View.java`
- 具体工作：
  - 实现“先自动收敛，再锁定”的默认流程
  - 支持 `AE lock`、`AWB lock`、固定 focus distance 或 AF lock
  - 在能力允许时试验 `AE_OFF + AWB_OFF`
  - 为不同 profile 配置曝光时间和帧间隔
- 交付物：
  - 3A 控制策略说明
  - profile 参数初版
- 验收标准：
  - 连续运行时曝光时间和亮度抖动明显下降
  - color decode 的稳定性优于自动模式

#### T1-5 兼容性矩阵与回退策略

- 优先级：`P1`
- 责任域：`Android` + `QA/Bench`
- 前置依赖：`T1-1`、`T1-2`、`T1-4`
- 目标：保证 Camera2 迁移不会因为个别设备兼容性阻塞主线
- 具体工作：
  - 记录各机型支持能力和异常现象
  - 明确哪些设备必须回退旧路径
  - 为 profile 选择增加设备黑白名单
- 交付物：
  - 简版设备兼容矩阵
  - 回退条件清单
- 验收标准：
  - 兼容性问题不需要现场人工猜测才能定位

### 5.4 阶段准出条件

- `Camera2` 成为默认主路径
- `latest-frame-only` 已稳定工作
- 至少一个稳定 profile 在多台设备上优于旧版本

## 6. Phase 2：接收端快路径与计算降本

### 6.1 阶段目标

- 把“每帧全图扫描”改成“锁定后优先局部处理”
- 让灰度和几何定位先走轻量路径
- 降低 CPU、内存带宽和无效颜色转换成本

### 6.2 准入条件

- `Phase 1` 完成
- 有离线样本可回放验证

### 6.3 任务列表

#### T2-1 Y 平面优先链路

- 优先级：`P0`
- 责任域：`Android` + `C++/Codec`
- 前置依赖：`T1-3`
- 目标：把扫描、定位、symbol 预处理尽量建立在 luma 上
- 涉及模块：
  - `OpencvCamera2View.java`
  - JNI 图像传递路径
  - `MultiThreadedDecoder`
- 具体工作：
  - 优先传递 `Y` 平面或灰度视图
  - 避免每帧先转完整 `RGBA`
  - 只在 color decode 需要时再读取 chroma 或 RGB 信息
- 交付物：
  - 新的图像输入接口
  - 颜色延迟读取策略
- 验收标准：
  - 单帧前处理耗时下降
  - 峰值内存带宽和复制次数减少

#### T2-2 ROI 快路径

- 优先级：`P0`
- 责任域：`C++/Codec`
- 前置依赖：`T2-1`
- 目标：在画面稳定时不再每帧全图 `Scanner.scan()`
- 涉及模块：
  - `cfc/app/src/cpp/cfc-cpp/MultiThreadedDecoder.h`
  - `/mnt/c/next/libcimbar` 中 `Scanner`、`Deskewer`、相关提取逻辑
- 具体工作：
  - 保存上一帧成功的角点、ROI、homography
  - 先在扩边 ROI 内局部重扫
  - 连续失败 `N` 次后回退全图扫描
  - 记录快路径命中率和回退次数
- 交付物：
  - ROI 状态机
  - 局部重扫实现
- 验收标准：
  - 锁定后全图扫描占比明显下降
  - `scan_success_ratio` 不下降，平均扫描耗时显著下降

#### T2-3 autodetect 锁定机制

- 优先级：`P0`
- 责任域：`C++/Codec`
- 前置依赖：`T2-2`
- 目标：把自动模式从“每帧轮询”改成“短探测后锁定”
- 涉及模块：
  - `MultiThreadedDecoder`
  - 配置管理逻辑
- 具体工作：
  - 会话开始前 `M` 帧执行 mode 探测
  - 一旦某个 mode 连续成功，进入锁定
  - 只有连续失败或 metadata 指明变化时才重新探测
- 交付物：
  - mode lock 状态机
  - lock/unlock 埋点
- 验收标准：
  - 稳定会话中 mode brute-force 尝试显著减少
  - 稳态 goodput 优于原有轮询方案

#### T2-4 ROI 快路径回归样本

- 优先级：`P1`
- 责任域：`QA/Bench`
- 前置依赖：`T2-2`、`T2-3`
- 目标：确保快路径不会引入隐藏回归
- 具体工作：
  - 增加轻微移动、突然失焦、快速回到稳定状态的录制样本
  - 比较快路径和纯全图扫描在成功率上的差异
  - 覆盖“频繁进出锁定状态”的会话
- 交付物：
  - 一组快路径专用回归样本
- 验收标准：
  - 快路径失败时能可靠回退，不出现长期锁死

### 6.4 阶段准出条件

- 锁定阶段的平均扫描开销显著下降
- autodetect 不再长期轮询
- 同等场景下吞吐和延迟优于 `Phase 1`

## 7. Phase 3：发送端节拍与协议显式信息

### 7.1 阶段目标

- 让 sender 帧切换更贴近显示节拍
- 给接收端提供明确的 mode、sync、pilot、版本信息
- 减少接收端靠猜测恢复上下文的成本

### 7.2 准入条件

- `Phase 2` 完成
- 团队确认协议可以引入兼容性变更

### 7.3 任务列表

#### T3-1 sender 节拍稳定化

- 优先级：`P0`
- 责任域：`Web/Sender`
- 前置依赖：`T0-4`
- 目标：降低浏览器定时器抖动导致的 mixed frame 概率
- 涉及模块：
  - `/mnt/c/next/libcimbar/web/*`
  - 发送端渲染与调度逻辑
- 具体工作：
  - 评估从 `setTimeout` 切到 `requestAnimationFrame`
  - 补充每帧实际显示间隔统计
  - 为同步帧和参考帧预留发送策略
- 交付物：
  - sender 节拍统计日志
  - 节拍驱动实现初版
- 验收标准：
  - 帧间隔抖动降低
  - 接收端 mixed frame 占比下降

#### T3-2 metadata / sync / pilot 头部设计

- 优先级：`P0`
- 责任域：`C++/Codec` + `Web/Sender`
- 前置依赖：`T2-3`
- 目标：把 mode、版本、校准信息显式写入帧结构
- 涉及模块：
  - `/mnt/c/next/libcimbar` 协议编码与解码逻辑
  - sender 帧布局
- 具体工作：
  - 设计最小 metadata 字段集：
    - `protocol version`
    - `mode id`
    - `ecc profile`
    - `frame id`
    - `generation id`
    - `pilot / palette info`
    - `sync bits`
  - 选择头部放置区域并评估对 payload 的影响
  - 明确接收端读 header 的顺序和失败回退策略
- 交付物：
  - 帧头设计文档
  - 编码器和解码器实现草案
- 验收标准：
  - 接收端稳定会话中不再依赖 brute-force mode 轮询
  - 帧同步和颜色校正时间缩短

#### T3-3 协议版本兼容与灰度发布

- 优先级：`P1`
- 责任域：`C++/Codec` + `Android` + `Web/Sender`
- 前置依赖：`T3-2`
- 目标：引入新 header 时不破坏老流程的可控性
- 具体工作：
  - 增加 `protocol version`
  - 明确旧版本 sender / receiver 的兼容行为
  - 支持 feature flag 或编译开关灰度切换
- 交付物：
  - 协议兼容矩阵
  - 升级与回退说明
- 验收标准：
  - 新旧版本边界清晰
  - 遇到不兼容数据时能快速失败并上报

#### T3-4 颜色校准与 pilot 区域强化

- 优先级：`P1`
- 责任域：`C++/Codec` + `Web/Sender`
- 前置依赖：`T3-2`
- 目标：把颜色校准从“后处理尝试”提升为协议组成部分
- 具体工作：
  - 固定 palette/pilot 区域
  - 记录颜色观测漂移
  - 评估背景亮度和白边强度对可读性的影响
- 交付物：
  - pilot 设计说明
  - 校准评估日志
- 验收标准：
  - 弱光或偏角场景下颜色稳定性提升

### 7.4 阶段准出条件

- sender 节拍有稳定统计和改进
- receiver 可以从帧头快速建立 mode/sync 上下文
- 新协议在 feature flag 下可灰度验证

## 8. Phase 4：多帧融合与会话级自适应

### 8.1 阶段目标

- 让 imperfect frame 提供部分有效信息
- 在 fountain 之前先做视觉层融合
- 做会话级而非实时双向闭环的自适应

### 8.2 重要约束

当前系统是“显示器到手机摄像头”的离线单向链路。  
因此第一版自适应不应假设存在可靠回传通道。

本阶段建议采用：

- 会话开始阶段选择 profile
- sender 预先配置或人工选择 profile
- receiver 记录推荐档位和链路质量

不要把“在线双向反馈切码率”作为本阶段的硬依赖。

### 8.3 准入条件

- `Phase 3` 完成
- 帧头里已经有 `frame id`、版本和必要 pilot 信息

### 8.4 任务列表

#### T4-1 多帧短窗口缓存

- 优先级：`P0`
- 责任域：`C++/Codec`
- 前置依赖：`T3-2`
- 目标：为同一候选帧保留最近几次观测
- 涉及模块：
  - `/mnt/c/next/libcimbar` 解码侧
  - `MultiThreadedDecoder`
- 具体工作：
  - 以 `frame id` 为键建立短窗口缓存
  - 控制缓存大小和超时淘汰
  - 记录窗口命中率和额外内存成本
- 交付物：
  - 多帧缓存管理器
- 验收标准：
  - 不引入不可控内存增长
  - 能按 `frame id` 聚合相邻观测

#### T4-2 cell 级软信息聚合

- 优先级：`P0`
- 责任域：`C++/Codec`
- 前置依赖：`T4-1`
- 目标：把单帧硬判决升级为“候选值 + 置信度”的聚合
- 具体工作：
  - 为 cell 保留最佳候选、次佳候选和置信度
  - 合并多帧的颜色观测和 symbol 候选
  - 支持把低置信度位置标成 erasure
- 交付物：
  - 软信息聚合器
  - 聚合日志字段
- 验收标准：
  - 单帧失败但多帧合并成功的案例可稳定复现

#### T4-3 融合后再进 RS / fountain

- 优先级：`P0`
- 责任域：`C++/Codec`
- 前置依赖：`T4-2`
- 目标：把视觉层融合真正接到解码主路径
- 具体工作：
  - 定义“单帧直通”和“多帧融合后解码”的切换阈值
  - 在融合后再调用 RS 和 fountain sink
  - 记录“坏帧转化为有效贡献”的比例
- 交付物：
  - 新的解码调度策略
- 验收标准：
  - `mixed_frame_utilization_ratio` 明显提高
  - 弱光、轻抖动场景下 goodput 提升

#### T4-4 会话级 bitrate ladder

- 优先级：`P1`
- 责任域：`C++/Codec` + `Web/Sender`
- 前置依赖：`T3-2`
- 目标：先做 preset/profile 级切换，不直接追求在线闭环
- 具体工作：
  - 定义 `R1 robust`、`R2 balanced`、`R3 high-throughput`
  - 为每档明确 mode、ecc、冗余和 sender 节拍
  - 制定“何时建议切换档位”的接收侧判据
- 交付物：
  - bitrate ladder 配置表
  - 接收侧推荐策略
- 验收标准：
  - 在不同环境下能稳定选择不同档位
  - 档位切换逻辑不依赖实时回传

#### T4-5 链路质量报告

- 优先级：`P1`
- 责任域：`QA/Bench` + `Android`
- 前置依赖：`T4-4`
- 目标：把链路质量总结成能指导 profile 选择的报告
- 具体工作：
  - 汇总不同机型、亮度和角度下各 profile 表现
  - 形成推荐表，例如“设备 A 在普通室内优先 `R2`”
  - 给后续 5x5 或高帧率专线提供准入依据
- 交付物：
  - profile 推荐矩阵
- 验收标准：
  - 新设备接入时可以依据矩阵快速给出默认档位

### 8.5 阶段准出条件

- 多帧融合在真实样本上带来稳定收益
- 会话级 preset/profile 选择有明确依据
- 不依赖回传链路也能完成自适应第一版

## 9. Phase 5：高密度码型、ECC 和专项加速

### 9.1 阶段目标

- 在前面几阶段已经稳定的基础上，追求更高理论上限
- 只在已有收益稳定兑现后进入此阶段

### 9.2 准入条件

- `Phase 4` 完成
- 基线对比显示瓶颈已经从采集稳定性转向码型密度或算力

### 9.3 任务列表

#### T5-1 `Conf5x5 / Conf5x5d` 产品化

- 优先级：`P0`
- 责任域：`C++/Codec`
- 前置依赖：`T4-5`
- 目标：把已有实验性配置变成可控的产品模式
- 涉及模块：
  - `/mnt/c/next/libcimbar/src/lib/cimb_translator/Config.h`
  - `/mnt/c/next/libcimbar/src/lib/cimb_translator/GridConf.h`
- 具体工作：
  - 明确 mode id、参数和测试集
  - 与 metadata/header 和 profile 配置打通
  - 补充 5x5 失效场景样本
- 交付物：
  - 可开关的 5x5 模式
  - 对应测试报告
- 验收标准：
  - 在目标设备上 5x5 的净 goodput 确实高于 8x8
  - 不满足条件的设备能自动回避

#### T5-2 软信息友好的 ECC 研究型分支

- 优先级：`P1`
- 责任域：`C++/Codec`
- 前置依赖：`T4-2`、`T4-3`
- 目标：探索比“硬判决 + RS”更适合视觉噪声的 frame 内编码
- 具体工作：
  - 评估 erasure-aware block decoding
  - 评估 LDPC、RaptorQ 或 soft-decision RS 的工程成本
  - 明确是否值得进入主线
- 交付物：
  - 一份 ECC 研究结论文档
  - 一个独立试验分支
- 验收标准：
  - 有清楚的数据说明收益和复杂度
  - 未证明收益前不进入主线

#### T5-3 SIMD / OpenCL / UMat 优化

- 优先级：`P1`
- 责任域：`C++/Codec`
- 前置依赖：`T2-2`
- 目标：在工作集已经缩小后，再做底层算力提速
- 具体工作：
  - 识别热点函数，如 `threshold`、`warpPerspective`、距离计算
  - 尝试 `cv::UMat` 或 OpenCL
  - Android ARM64 路径试验 NEON
- 交付物：
  - 热点分析结果
  - 至少一条有效加速路径
- 验收标准：
  - 加速收益来自真实热点，不是基准噪声
  - 不破坏现有图像结果一致性

#### T5-4 高帧率专用分支

- 优先级：`P2`
- 责任域：`Android` + `Web/Sender`
- 前置依赖：`T1-2`、`T5-1`
- 目标：面向指定设备做实验室上限突破
- 具体工作：
  - 检测 `CONSTRAINED_HIGH_SPEED_VIDEO`
  - 建立高帧率 session
  - 为高帧率分支设计专用 sender profile
- 交付物：
  - 高帧率专用实验分支
- 验收标准：
  - 只在指定机型启用
  - 明确与主线分支隔离

### 9.4 阶段准出条件

- 高密度模式或专项加速已证明具备净收益
- 高风险实验与主线版本有清晰边界

## 10. 推荐排期与并行关系

### 10.1 串行主路径

建议按以下主路径推进：

1. `T0-1` -> `T0-2` -> `T0-3` -> `T0-4`
2. `T1-1` -> `T1-2` -> `T1-3` -> `T1-4`
3. `T2-1` -> `T2-2` -> `T2-3`
4. `T3-1` 与 `T3-2` 并行，随后做 `T3-3`
5. `T4-1` -> `T4-2` -> `T4-3`
6. `T5-1` 和 `T5-3` 可在 `Phase 4` 收尾后并行探索

### 10.2 可并行任务

以下任务适合平行推进：

- `T0-3` 与 `T1-1` 可局部并行，但正式验收仍依赖埋点完成
- `T1-5` 可以从 `T1-2` 开始持续更新
- `T2-4` 可以伴随 `T2-2`、`T2-3` 持续扩样本
- `T3-4` 可以与 `T3-2` 一起设计
- `T4-5` 可以在 `T4-4` 开始后持续沉淀

## 11. 阶段验收 KPI

建议每个阶段至少检查以下指标：

- `goodput_kbps`
- `time_to_first_chunk_ms`
- `time_to_complete_file_ms`
- `scan_success_ratio`
- `decode_success_ratio`
- `avg_backlog`
- `perfect_frame_ratio`

建议附加专项指标：

- `Phase 1`：曝光时间抖动、帧率稳定性、丢帧率
- `Phase 2`：ROI 快路径命中率、全图扫描占比、扫描平均耗时
- `Phase 3`：sender 帧间隔抖动、header 解析成功率
- `Phase 4`：`mixed_frame_utilization_ratio`、多帧融合成功率
- `Phase 5`：单位耗时吞吐提升、单位功耗吞吐提升

## 12. 风险清单

### R1 Camera2 设备碎片化

- 风险：不同机型对手动传感器控制支持差异大
- 应对：保留旧 Camera 路径回退，建立兼容矩阵

### R2 双仓库逻辑漂移

- 风险：`/mnt/c/next/libcimbar` 与 `cfc` 内嵌副本长期不一致
- 应对：在 `Phase 0` 明确 source of truth 和同步流程

### R3 单向链路无法做实时闭环切码率

- 风险：过早设计需要回传的自适应方案，造成任务失焦
- 应对：第一版只做 session 级 preset/profile 选择

### R4 过早进入高密度码型优化

- 风险：采集链路未稳时，5x5 反而降低整体吞吐
- 应对：`Phase 5` 必须晚于 `Phase 4`

## 13. 建议的第一轮执行清单

如果只启动第一轮落地，建议先做以下 8 项：

1. `T0-1` 仓库同步策略梳理
2. `T0-2` 统一埋点与会话日志
3. `T0-3` 样本录制与离线回放基线
4. `T0-4` 基线报告
5. `T1-1` Camera2 主路径接入
6. `T1-2` 设备能力探测与 capture profile
7. `T1-3` latest-frame-only 背压策略
8. `T2-3` autodetect 锁定机制

这 8 项完成后，再决定是否优先进入 `ROI` 快路径还是 sender / protocol 方向，会更稳妥。

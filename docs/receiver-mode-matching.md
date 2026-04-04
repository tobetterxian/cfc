# Sender / Receiver 模式对应与自动识别说明

本文档说明当前项目里：
- sender 页面如何选择编码 mode
- Android APK 接收端如何选择 decode mode
- autodetect 的真实行为
- 哪些 sender / receiver 组合必须匹配
- 哪些组合更容易出问题

适用范围：
- `app/src/cpp/libcimbar/web/main.js`
- `app/src/cpp/libcimbar/src/lib/cimb_translator/Config.h`
- `app/src/cpp/cfc-cpp/MultiThreadedDecoder.h`
- `app/src/cpp/cfc-cpp/jni.cpp`
- `app/src/main/java/org/cimbar/camerafilecopy/MainActivity.java`
- `app/src/main/java/org/cimbar/camerafilecopy/ModeSelToggle.java`
- `app/src/cpp/libcimbar/src/lib/cimbar_js/cimbar_recv_js.cpp`

---

## 1. 先说结论

在当前项目里，**sender 和 receiver 必须在同一个 mode 上解读同一套编码参数**。

因为 mode 决定了：
- `color_bits`
- `symbol_bits`
- `ecc_bytes / ecc_block_size`
- `fountain_chunk_size`
- `fountain_chunks_per_frame`
- 图像几何布局

这些参数只要有一个档位不一致，接收端就会：
- 扫不到
- 解码不到有效 chunk
- 或者 fountain 层无法拼出完整文件

所以核心原则是：

> **sender 用什么 mode，receiver 最终也必须按同一个 mode 解。**

不同的是：
- sender 是主动选 mode
- receiver 可以手动锁定 mode，也可以先 autodetect 再锁定

---

## 2. 当前 sender / receiver 共享的 mode 编号

当前 sender 和 receiver 共用同一套底层 `Config::temp_conf(mode_val)` 映射。

| UI 名称 | mode 值 | 底层配置 |
| --- | ---: | --- |
| 4C | 4 | legacy 4-bit / 4-color |
| Bu | 66 | `Conf8x8_micro()` |
| Bm | 67 | `Conf8x8_mini()` |
| B | 68 | `Conf8x8()` |
| 5x5 | 69 | `Conf5x5()` |
| 5x5d | 70 | `Conf5x5d()` |

这意味着：
- sender 点 `B`，实际是在用 `mode = 68`
- receiver 如果要锁定同档，就必须也用 `68`

---

## 3. Android APK 接收端的 mode 选择机制

### 3.1 `modeVal = 0` 表示 autodetect
在 `MainActivity` 里：
- 默认启动时 `modeVal = 0`
- `mode switch` 关闭时，`modeVal = 0`

这表示：
- 接收端不预先锁死某个 mode
- 而是让 native 解码器自己轮询探测

### 3.2 `modeVal != 0` 表示锁定指定 mode
当 UI 上的 mode toggle 被打开时：
- `modeVal = detectedMode`

也就是：
- 一旦 autodetect 识别到某个 mode
- Java 层会把 toggle 打开
- 并把当前识别到的 `detectedMode` 作为锁定值继续喂给 native

所以 Android 接收端的实际状态有两种：

#### 状态 A：自动识别
- `modeVal = 0`
- native 会在多个 mode 之间轮询

#### 状态 B：锁定识别结果
- `modeVal = detectedMode`
- native 之后只按这一个 mode 解

---

## 4. Android autodetect 的真实逻辑

这部分关键逻辑在 `MultiThreadedDecoder::next_autodetect_mode()` 和 `update_autodetect()`。

### 4.1 初期只轮询常用 mode
autodetect 初期只在这四个 mode 里轮询：

- `68` -> B
- `67` -> Bm
- `66` -> Bu
- `4` -> 4C

代码常量：

```cpp
kCommonModes = {68, 67, 66, 4}
```

这意味着：
- 接收端一开始**不会立刻尝试 5x5 / 5x5d**
- 优先探测 B 系列和 legacy 4C

### 4.2 过一段时间后才把 5x5 / 5x5d 纳入探测
当 frame 计数超过阈值后，才切换到扩展轮询：

```cpp
kExtendedModes = {68, 67, 66, 4, 69, 70}
```

也就是之后会额外尝试：
- `69` -> 5x5
- `70` -> 5x5d

### 4.3 一旦某个 mode 解出了数据，会临时锁定
在 `update_autodetect(modeVal, decodeRes)` 里：
- 如果本帧 `decodeRes > 0`
- 就把 `_detectedMode = modeVal`
- 并把 `_autodetectLockedMode = modeVal`

含义是：
- 只要某个 mode 成功解出有效数据
- autodetect 就不再继续轮询其它 mode
- 后续优先沿着这个 mode 继续解

### 4.4 连续失败 5 次才会放弃这次锁定
如果已经锁到某个 mode，之后连续失败：
- `_autodetectFailureStreak >= 5`

才会清掉锁定，重新回到轮询状态。

所以 autodetect 不是“每帧都乱猜”，而是：
1. 先轮询探测
2. 有命中就锁住
3. 连续失败 5 次才解锁

---

## 5. Android 接收端“识别到 mode”后会发生什么

native 层如果已经识别到 mode：
- `jni.cpp` 会返回一个特殊字符串：`"/68"`、`"/67"` 之类

Java 层在 `onCameraFrame()` 里看到：
- 返回值以 `/` 开头

就会把它解释成：
- 这是 detected mode，不是文件完成

然后：
1. 更新 `detectedMode`
2. 在 UI 上把 `ModeSelToggle` 打开
3. 把 toggle 的显示状态切成对应 mode

### 5.1 toggle 显示支持哪些外观
当前 `ModeSelToggle` 只专门定义了：
- `4C`
- `Bm`
- `Bu`
- `B` 走默认外观

也就是说：
- `69 / 70` 目前没有专门的 toggle 外观区分
- 但底层仍然可以识别和解码

这是 **UI 表达能力不足**，不是协议不支持。

---

## 6. 为什么 sender / receiver mode 必须一致

因为 receiver 初始化解码器时，用的是当前 mode 对应的配置：

- `Config::update(modeVal)`
- `Decoder(ecc_bytes, color_bits)`
- `fountain_chunk_size(modeVal)`

这会直接影响：
- 每个 cell 应该按几 bit 读颜色
- 每个 cell 应该按几 bit 读符号
- RS block 怎么切
- 每帧有几个 chunk
- 每个 chunk 多大

### 6.1 典型不匹配后果

#### sender = B (68)，receiver 按 Bm (67) 解
问题：
- 两者都属于 8x8 / 6bit per cell，看起来很像
- 但 `ecc_bytes / block_size / chunk_size / image geometry` 都不一样

结果通常是：
- 可能偶尔扫到一点点
- 但很难形成稳定有效 decode
- fountain 重组也不成立

#### sender = B，receiver 按 5x5 解
问题更大：
- 几何布局、cell 尺寸、网格密度都不一样

结果通常是：
- 基本直接扫不出来

#### sender = 4C，receiver 按 B 解
问题：
- legacy 颜色解释逻辑不同
- `symbol_bits` 也不同

结果通常是：
- 颜色/符号解释都不对
- 几乎无法正确 decode

---

## 7. Android 接收端切 mode 时为什么有时会重建解码器

在 `jni.cpp` 中：
- Java 每帧把 `modeVal` 传给 native
- native 调 `proc->set_mode(modeVal)`

如果 `set_mode(modeVal)` 发现：
- 新 mode 的 `fountain_chunk_size` 与当前 writer 不一致

则会返回 `false`，然后 `jni.cpp` 会：
- 直接新建一个 `MultiThreadedDecoder`

### 含义
这表示：
- **并不是所有 mode 之间都能热切换并保留当前接收上下文**
- 如果 chunk size 不同，必须重建 decoder / writer 状态

这很重要，因为：
- 不同 mode 之间的 fountain 流结构本来就不兼容
- 强行沿用旧上下文会污染文件重组状态

---

## 8. 接收端的 progress / in-flight 到底对应什么

Android 侧有两个 native 指标：

- `files_in_flight`
- `files_decoded`

它们来自 `concurrent_fountain_decoder_sink`：
- `num_streams()`
- `num_done()`

可以这样理解：
- `files_in_flight`：当前正在重组中的文件数
- `files_decoded`：已经完整通过解码落盘的文件数

注意：
- 这不是“识别到 mode”
- 也不是“拍到了多少帧”
- 而是 fountain 重组层面的文件状态

---

## 9. sender / receiver 最常见的正确组合

### 9.1 最推荐的标准组合
- sender: `B (68)`
- Android receiver: 启动时 autodetect，识别后锁到 `68`

这是当前最标准的链路。

### 9.2 稳定优先组合
- sender: `Bm (67)`
- Android receiver: autodetect -> 锁 `67`

适合：
- 想比 B 更保守一些
- 优先稳，不优先吞吐

### 9.3 高密度实验组合
- sender: `5x5 (69)` 或 `5x5d (70)`
- Android receiver: 必须允许 autodetect 进入扩展 mode，或者直接手动锁定正确 mode

注意：
- 由于 Android autodetect 初期不探测 69/70
- 所以 sender 如果一开始就在发 5x5 / 5x5d
- 接收端需要更多时间才能探测到，或者最好手动指定

这也是 5x5 系列体验上常常“不如 B 系列直接稳”的一个原因。

---

## 10. 为什么 5x5 / 5x5d 更依赖正确匹配

因为它们不只是“更高容量”，而是：
- 更小 cell
- 更密网格
- 不同 chunk 拆分
- 不同纠错参数

尤其 `5x5d`：
- 每帧 16 chunk
- 几何布局也更激进

所以它对：
- 屏幕缩放
- 镜头锐度
- 自动对焦
- 正确 mode 匹配

都更敏感。

---

## 11. Web receiver 与 Android receiver 的差别

### Android receiver
- 支持 `modeVal = 0` autodetect
- native 会轮询 mode
- 成功后自动锁定
- 连续失败 5 次再解锁

### Web receiver (`cimbard_configure_decode`)
- `mode_val <= 0` 时会直接回退到 `68`
- **没有 Android 这套 autodetect 轮询逻辑**

也就是：
- Web receiver 默认更像“固定按 B 解”
- Android receiver 才有完整 autodetect 状态机

这点很关键：

> **Android 接收端和 web 接收端在 mode 选择策略上并不完全一样。**

---

## 12. 针对你当前场景的实用建议

你当前场景是：
- 电脑浏览器 sender
- 手机 APK receiver

建议这样理解：

### 推荐默认链路
- sender 用 `R2 / B (68)`
- 手机 APK 打开后先让它 autodetect
- 一旦识别到 `68`，后面就等于锁到 B 模式继续接收

### 如果 sender 改成 `Bm`
- 接收端一般也能比较快锁到 `67`
- 适合想求稳时使用

### 如果 sender 改成 `5x5 / 5x5d`
- 接收端可能不会第一时间识别出来
- 需要等待扩展轮询阶段，或者直接手动锁正确 mode
- 因此不适合作为“无脑默认档”

---

## 13. 排查 mode 相关问题时应该看什么

如果怀疑 sender / receiver mode 没对齐，优先看这些：

### sender 侧
- 当前页面选中的 mode
- 当前 profile 是否偷偷把 mode 改了
- framerate 是否过高导致虽然 mode 对了但仍然不稳定

### Android receiver 侧
- `events.log` 里有没有 `detected mode payload=/68` 之类的记录
- `summary.json` 里最后的 `detected_mode`
- 是否频繁出现检测到某个 mode 后又丢锁

### 典型症状
- 一直没有 detected mode：可能根本没扫到或 mode 太激进
- detected mode 在不同值之间跳：可能环境差或误识别多
- 已检测到 mode 但始终不出进度：可能 sender/receiver 没真正对齐，或帧质量不够

---

## 14. 一页式结论

如果只记最关键的几条：

- sender 和 receiver **必须最终使用同一个 mode**
- Android 的 `modeVal = 0` 表示 autodetect，不是某个固定协议
- Android autodetect 初期只探测：`68 / 67 / 66 / 4`
- `69 / 70` 要到后期才纳入探测，所以 5x5 系列更不适合作为默认档
- autodetect 一旦某个 mode 解出数据，就会先锁定这个 mode
- 连续失败 5 次才会放弃锁定
- Web receiver 默认更像固定 `68`，没有 Android 那套完整 autodetect 轮询
- 对“电脑发、手机收”来说，最推荐的标准组合仍然是：**sender = B (68)，receiver = Android autodetect -> 锁 68**

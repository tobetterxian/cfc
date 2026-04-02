# cfc / libcimbar 仓库同步策略

## 1. 当前事实

当前 Android App 的本地构建直接使用的是：

- `cfc/app/src/cpp/libcimbar`

不是外部工作区里的：

- `/mnt/c/next/libcimbar`

原因是 `cfc/app/src/cpp/CMakeLists.txt` 直接把 `libcimbar` 目录作为子工程加入 Android native build。

## 2. 当前建议的 source of truth

为了避免“改了上游但 App 不生效”或“App 内嵌副本和上游长期漂移”，建议按职责区分：

- Android 接收端实际发版所依赖的 native 代码：
  - 当前 source of truth 以 `cfc/app/src/cpp/libcimbar` 为准
- 脱离 Android 的通用库、CLI、Web sender、WASM 产物：
  - 当前 source of truth 以 `/mnt/c/next/libcimbar` 为准

这不是理想状态，但符合当前仓库结构的现实。

## 3. 同步规则

### 3.1 必须在 `cfc` 内嵌副本落地的改动

以下改动如果不落到 `cfc/app/src/cpp/libcimbar`，Android App 就不会受益：

- 接收链路相关的 extractor / scanner / decoder 改动
- JNI 直接依赖的 protocol/runtime 改动
- Android 上需要验证的 ROI / 多帧融合 / 软信息逻辑

### 3.2 必须在外部 `/mnt/c/next/libcimbar` 落地的改动

以下改动如果只落在 `cfc` 内嵌副本，会导致 Web/CLI 路径无法复用：

- `web/*` sender 节拍、metadata、pilot、sync 相关改动
- 通用 encoder / decoder / protocol 设计
- `Config.h`、`GridConf.h` 等共享配置
- 脱离 Android 的 benchmark、CLI、WASM 支撑代码

### 3.3 共享逻辑的执行规则

如果某项改动同时影响 Android 接收端和上游通用库，执行顺序建议为：

1. 先在 `cfc/app/src/cpp/libcimbar` 落地并验证 Android 构建
2. 再把同一逻辑同步到 `/mnt/c/next/libcimbar`
3. 在状态文档里记录“是否已完成双仓同步”

## 4. 提交与推送规则

建议采用以下提交策略：

- `cfc` 仓库：
  - 提交 Android App、JNI、内嵌 `libcimbar` 副本、App 文档
- `/mnt/c/next/libcimbar` 仓库：
  - 提交上游通用库、Web sender、CLI、WASM、上游文档

如果某个 Phase 同时修改了两个仓库，Phase 完成时应分别推送两个仓库。

## 5. 后续建议

长期建议不要继续手工维持两份 `libcimbar` 逻辑，至少应补其中一种机制：

- Git submodule/subtree
- vendor-sync 脚本
- “上游单一源码 + Android 构建时拉取”的方式

在这套机制建立之前，状态文档必须持续记录：

- 哪些任务只改了 `cfc`
- 哪些任务已经同步到 `/mnt/c/next/libcimbar`
- 哪些任务仍存在双仓漂移风险

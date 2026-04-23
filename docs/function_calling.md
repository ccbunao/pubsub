# Function calling 融入 pubsub：场景、需求与设计草案

本文讨论如何在 `tcp_pubsub` / `shm_pubsub` 之上引入“function calling”（也可称 tool calling / tool invocation），让消息从“任意字节流”升级为结构化的 **`ToolCall` / `ToolResult`**，并给出面向大模型（LLM）场景的典型需求与演进路线。

> 核心观点：pubsub 负责 **传输与解耦**；function calling 负责 **结构化、路由、幂等、权限与观测**。两者结合后可以形成一个轻量的“工具总线（tool bus）”。

## 1. 为什么需要 function calling？

当系统不只是传输数据，而是要驱动“动作”（例如抓图、控制设备、查询数据库、调用 HTTP API、执行推理）时，纯 blob 会很快遇到问题：

- 消息缺少语义：谁要执行什么？参数是什么？结果去哪儿？
- 无法可靠对齐请求/响应：缺少 `call_id` 之类的关联 id
- 安全难落地：没有稳定结构就难做 allowlist / 参数校验
- 观测难落地：难统计成功率、超时、重试、丢弃等指标

function calling 的价值是把“动作”表达成结构化对象，使得调度、审计、回放、限流、权限都能工程化。

## 2. LLM 场景的典型应用

### 2.1 多进程/多机工具执行

LLM 在一个进程（或云端）做规划，工具实际在另一台机器/边缘设备/沙箱中执行：

- 摄像头设备/采集卡只能在本机访问
- PLC / 工业设备在局域网
- GPU 推理服务单独部署

pubsub 做消息总线，LLM 通过发布 `ToolCall` 请求远端 worker 执行，worker 发布 `ToolResult` 返回。

### 2.2 多 agent 协作

不同 agent（或不同 worker）各自负责一类工具：

- `search.*`（检索）
- `vision.*`（视觉）
- `control.*`（设备控制）
- `storage.*`（文件/对象存储）

pubsub 可天然支持广播与解耦，便于扩展工具集合。

### 2.3 可回放与审计

结构化 `ToolCall` / `ToolResult` 事件流便于：

- 离线重放（replay）
- 回归测试（同一 call 输入、验证输出）
- 安全审计（谁调用了什么工具）

## 3. 需求梳理（建议先定语义）

在落地前建议明确下列问题，否则很容易“能跑但不可控”：

### 3.1 可靠性语义

pubsub（尤其 shm ring）在高负载下可能丢消息，因此需要选择语义：

- **At-most-once（至多一次）**：不重试，但可能丢；适合非关键、状态类（例如 UI 状态刷新）。
- **At-least-once（至少一次）**：允许重试/重复；需要幂等或去重；适合大多数工具调用。
- **Exactly-once（恰好一次）**：成本最高，通常需要事务日志/持久化队列，不建议在纯内存 pubsub 上直接承诺。

### 3.2 背压与限流

工具执行往往“慢”，发布端可能“快”。需要明确：

- call 队列满了怎么办：阻塞、丢弃、降级？
- 执行方并发上限：每个工具多少并发？

### 3.3 安全边界

LLM 不能直接拥有系统权限。需要：

- 允许的工具集合（allowlist）
- 参数校验（范围、类型、敏感字段）
- 调用方身份/权限（who can call what）
- 沙箱（文件/命令/网络访问限制）

## 4. 设计概览（建议最小可行版本）

### 4.1 组件分工

- **Orchestrator（编排/LLM 侧）**：决定何时发 `ToolCall`，等待 `ToolResult` 并回填到对话/状态机。
- **Tool Worker（执行侧）**：订阅某些 `ToolCall`，执行本地函数/外部系统，并发布 `ToolResult`。
- **Router（可选）**：统一接收 call 并分发到不同 worker（便于权限/限流/熔断）。

### 4.2 事件类型

最小集合：

- `ToolCall`：请求执行某个 tool
- `ToolResult`：执行结果（成功/失败/错误码/输出）
- `ToolEvent`（可选）：进度/心跳/日志片段

## 5. 协议建议（JSON 起步，后续可换 protobuf）

### 5.1 ToolCall（建议字段）

```json
{
  "type": "tool_call",
  "id": "uuid-or-ulid",
  "name": "camera.capture",
  "args": { "device": 0, "format": "jpeg" },
  "timeout_ms": 2000,
  "caller": "llm_orchestrator",
  "target": "edge_cam_worker_1",
  "trace_id": "trace-xxx",
  "ts_ms": 1710000000000
}
```

### 5.2 ToolResult（建议字段）

```json
{
  "type": "tool_result",
  "id": "uuid-or-ulid",
  "name": "camera.capture",
  "ok": true,
  "result": { "bytes_b64": "..." },
  "error": null,
  "duration_ms": 37,
  "caller": "llm_orchestrator",
  "worker": "edge_cam_worker_1",
  "trace_id": "trace-xxx",
  "ts_ms": 1710000000037
}
```

### 5.3 topic 命名建议

推荐按职责拆 topic：

- `tool.call.<service>`：call 请求
- `tool.result.<caller>`：结果返回（caller 专用，避免广播风暴）
- `tool.event.<caller>`：可选事件流（进度/日志）

如果需要更细粒度，可按工具名拆：

- `tool.call.camera.capture`
- `tool.call.storage.put_object`

## 6. 去重与幂等（至少一次语义的关键）

如果系统选择 **at-least-once**，必须处理重复执行：

- `ToolCall.id` 作为幂等 key
- worker 维护一个 `(caller,id) -> ToolResult` 的缓存（TTL，例如 5~30 分钟）
- 重复 call 直接返回缓存结果（或忽略）

> 注意：幂等与去重需要稳定的 `id`，因此 id 必须由 caller 生成并保证唯一性。

## 7. 安全与权限（强烈建议从第一天做）

建议在 worker（或 router）端做：

- `name` allowlist：只允许执行明确列出的工具
- 参数 schema 校验：类型/范围/必填字段
- 资源限制：timeout、并发、速率限制、最大 payload 大小
- 审计日志：记录 `caller/id/name/args摘要/结果摘要`

LLM 端只负责“提出调用意图”，**不直接拥有执行权限**。

## 8. 与本仓库的融合方式（建议不动核心传输层）

`tcp_pubsub` / `shm_pubsub` 目前都以 “payload = bytes” 为中心。建议采用“上层协议”的方式融合：

1. 保持底层库不变（仍传 bytes）
2. 新增一个 `toolbus`（或 `function_calling`）模块：
   - JSON 编解码（或 protobuf）
   - topic 规划与路由
   - 幂等/去重与超时
   - 日志与统计接口
3. 提供可替换 transport：
   - `toolbus_shm`：topic -> shm region（同机多进程）
   - `toolbus_tcp`：在单个 TCP publisher 上做 topic mux（跨机）
4. 先做 demo：一个 worker（例如 `echo`, `add`），一个 orchestrator

这样可以避免底层库被强绑定到某个序列化协议或 LLM 生态。

## 9. transport 兼容策略（TCP + SHM 都能跑）

目标：**同一份 ToolCall/ToolResult 协议**，可选择 `shm_pubsub`（同机）或 `tcp_pubsub`（跨机）承载。

### 9.1 SHM：topic 就是 region 名

- 发布：`publish(topic, payload_bytes)` -> `shm_pubsub::shm::Publisher(topic).send(...)`
- 订阅：`shm_pubsub::shm::Subscriber(topic)`，收到的 payload 直接当 JSON 解析

优点：实现简单、延迟低、吞吐高；缺点：只适用于同机。

### 9.2 TCP：单端口承载多个 topic（topic mux framing）

`tcp_pubsub` 本身不提供 topic 概念，因此 `toolbus_tcp` 在 payload 内加入 framing：

- `u32be topic_len`
- `u32be payload_len`
- `topic bytes`
- `payload bytes`

这样可以：

- 一个 TCP publisher（一个端口）承载多个 `tool.call.*` 或 `tool.result.*` topic
- subscriber 收到 bytes 后按 topic 分发到本地 handler

### 9.3 call/result 的双通道模式（符合 pub/sub 的单向特性）

由于 `tcp_pubsub` 是 pub/sub（publisher -> subscriber 单向传输），建议使用两条 TCP 通道：

- **Call bus**：orchestrator 开一个 TCP publisher，worker 作为 subscriber 连接并接收 `tool.call.<service>`
- **Result bus**：worker 开一个 TCP publisher，orchestrator 作为 subscriber 连接并接收 `tool.result.<caller>`

这种模式在跨机时最容易落地，也最清晰。

## 9. 演进路线（建议）

1) **MVP**
- JSON 协议
- call/result topic
- id 对齐
- worker allowlist + timeout + 基础日志

2) **可靠性增强**
- 去重缓存
- backpressure/限流
- 可选持久化（落文件/轻量 DB）用于重放与审计

3) **性能增强**
- protobuf/flatbuffers
- 二进制 payload 的零拷贝（在 shm 场景可探索）
- 更完善的统计（Prometheus/OTel）

---

如果你希望我下一步在仓库里实现一个最小可运行 demo，我需要你确认：

1) 你更偏向用 `tcp_pubsub`（跨机）还是 `shm_pubsub`（同机低延迟）承载 tool bus？  
2) 是否允许重复执行（至少一次语义）并通过 `id` 去重？  
3) 目标工具类型是什么（例如 HTTP / 文件 / 设备控制 / DB）？

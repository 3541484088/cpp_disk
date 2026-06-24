# 面试问答准备 — 基于微服务架构的 AI 云盘系统

## 一、项目整体介绍

**Q：请用 3 分钟介绍一下你的云盘项目**

A：这是一个基于 C++ 微服务架构的分布式云盘系统，整体分为服务端和客户端两部分。

服务端采用微服务架构，包含以下核心服务：
- **API 网关（xms_gateway）**：所有客户端请求的统一入口，负责路由转发、Token 鉴权
- **认证服务（xauth）**：处理用户登录、注册、Token 生成与验证
- **注册中心（register_server）**：服务注册与发现，所有微服务启动时向其注册
- **配置中心（config_server）**：集中管理所有服务的配置项
- **上传服务（xms_upload_service）**：处理文件分片上传、秒传、断点续传
- **下载服务（xms_download_service）**：处理文件下载请求
- **目录服务（xms_dir_service）**：管理文件和目录的元信息
- **共享服务（xms_share_service）**：实现文件夹共享和权限控制
- **日志服务（xlog）**：统一收集和存储各服务日志

客户端使用 Qt 6.8 开发，提供文件管理 GUI，并集成了 AI 智能助手，支持通过自然语言与云盘交互。

服务间通信使用 Protocol Buffers 序列化，基于 libevent 实现异步高性能网络 I/O。

---

## 二、架构设计

**Q：为什么要拆成这么多微服务？**

A：主要有三个原因：
1. **职责单一**：每个服务只做一件事，比如上传服务只处理上传，代码更简单，bug 更容易定位
2. **独立部署和扩展**：上传量大时可以单独扩展上传服务的实例，不影响其他服务
3. **故障隔离**：某个服务挂了不会拖垮整个系统，比如日志服务挂了不影响文件上传

**Q：服务间是怎么通信的？**

A：使用自定义的二进制协议，消息结构分两部分：
- **XMsgHead（消息头）**：包含消息类型（msg_type）、目标服务名（service_name）、用户名（username）、Token、消息 ID 等路由信息
- **消息体**：使用 Protocol Buffers 序列化的业务数据

底层网络基于 libevent 的事件驱动模型，每个连接对应一个 bufferevent，通过回调函数处理读/写/错误事件。

**Q：网关层具体做了什么？**

A：网关是所有客户端请求的唯一入口，主要做三件事：
1. **Token 鉴权**：除登录请求外，所有请求都要验证 Token 有效性（通过 XAuthProxy 本地缓存或转发到认证服务验证）
2. **路由转发**：根据消息头中的 service_name 字段，将请求转发到对应的后端服务
3. **负载均衡**：同一服务有多个实例时，使用轮询策略选择一个可用连接

具体流程：客户端请求 → 网关 XRouterHandle::ReadCB() → 检查 Token → XServiceProxy::SendMsg() 轮询找到可用连接 → 转发到目标服务 → 服务处理完通过回调链路返回给客户端。

**Q：服务注册与发现是怎么实现的？**

A：采用自研的注册中心方案：
1. **注册**：每个微服务启动时，通过 XRegisterClient 连接注册中心，发送 MSG_REGISTER_REQ 消息（包含服务名、IP、端口、是否可被发现）
2. **发现**：网关启动后通过 XServiceProxy::Main() 线程定时向注册中心拉取服务列表，获取所有可用服务的 IP 和端口
3. **心跳**：XRegisterClient 通过 TimerCB() 定时发送心跳消息保持连接活跃，注册中心设置了 5 秒读超时检测
4. **本地缓存**：服务列表会序列化为 .cache 文件保存到本地，注册中心不可用时可读取缓存继续服务

**Q：如果某个服务挂了，系统会怎样？**

A：目前的容错策略：
- 网关的 XServiceProxy 维护了服务连接池，轮询时会跳过已断开的连接
- Main 线程每 3 秒检查一次，对断开的连接尝试重连
- 如果同一服务有多个实例，一个挂了可以由其他实例继续服务（轮询负载均衡）
- 注册中心不可用时，网关和客户端可读取本地缓存文件维持基本功能

---

## 三、文件传输

**Q：文件上传的完整流程是什么？**

A：分三个阶段：
1. **UPLOAD_FILE_REQ**：客户端发送文件元信息（文件名、目录、大小、MD5、是否加密），服务端创建目录、检查秒传和断点续传条件，返回已接收大小（offset）
2. **SEND_SLICE_REQ（循环）**：客户端按 10MB 分片发送文件数据，每片附带 MD5 校验，服务端验证后写入文件
3. **UPLOAD_FILE_END_REQ**：客户端通知上传完成，服务端关闭文件句柄，保存 .info 元信息文件

**Q：秒传是怎么实现的？**

A：在 UPLOAD_FILE_REQ 阶段，服务端收到文件信息后：
1. 检查目标路径是否已存在同名文件
2. 读取该文件的 .info 元信息文件
3. 对比 MD5 和加密标志（is_enc）
4. 如果完全一致，直接返回 "SEC_UPLOAD"，跳过后续所有分片传输
5. 客户端收到秒传响应后直接标记上传成功

**Q：断点续传是怎么实现的？**

A：上传断点续传：
1. 服务端在 UPLOAD_FILE_REQ 时检查已存在的文件大小
2. 将已存在大小对齐到分片边界（FILE_SLICE_BYTE = 10MB），避免不完整分片
3. 用 filesystem::resize_file() 截断到对齐位置
4. 将 safe_offset 通过 head->set_offset() 返回给客户端
5. 客户端从 offset 位置开始发送剩余分片

下载断点续传：
1. 客户端在 DOWNLOAD_FILE_REQ 中设置 head->offset() 为已下载大小
2. 服务端在 DownloadFileBegin 时调用 ifs_.seekg(resume_offset_) 定位到该位置
3. 从断点位置开始发送分片

**Q：分片大小是怎么定的？为什么是 10MB？**

A：分片大小为 10MB（FILE_SLICE_BYTE = 10000000），这是一个权衡：
- 太小：每个分片都有协议头开销，频繁网络交互，传输效率低
- 太大：内存占用高，失败后重传代价大，断点续传粒度粗
- 10MB 在常见带宽下约 1-2 秒传完一个分片，失败重传成本可接受

---

## 四、认证与安全

**Q：Token 是怎么生成和验证的？**

A：Token 生成流程：
1. 客户端发送登录请求（用户名 + MD5(密码)）
2. 认证服务查询 xms_auth 表验证用户名密码
3. 验证通过后，在 xms_token 表插入一条记录，token 字段使用 MySQL UUID() 函数生成
4. 设置过期时间为当前时间 + 1800 秒（30分钟）
5. 返回 token 给客户端

Token 验证流程：
1. 网关 XRouterHandle::ReadCB() 收到请求后调用 XAuthProxy::CheckToken()
2. 先查本地缓存（token_cache map），命中则直接通过
3. 未命中则转发到认证服务，认证服务查询 xms_token 表验证 token 有效性和过期时间
4. 定时清理过期 token（delete from xms_token where expired_time < now）

**Q：密码是怎么存储的？**

A：密码不存明文，存储 MD5(密码) 的 base64 编码。客户端登录时先对密码做 MD5_base64 转换再发送，服务端直接对比数据库存储值。

---

## 五、AI 智能助手

**Q：AI 助手是怎么知道该调用哪个工具的？**

A：采用 Function Calling 机制，流程如下：
1. 系统将所有可用工具的描述（名称、功能、参数列表）嵌入 System Prompt
2. 用户输入 + 当前文件列表等上下文一起发送给 LLM
3. LLM 根据用户意图判断是否需要调用工具，如果需要则返回特定 JSON 格式：`{"tool_call":{"name":"工具名","params":{...}}}`
4. 客户端解析到 tool_call 后执行对应工具，将结果作为 "Tool" 消息追加到对话
5. 再次调用 LLM，LLM 根据工具返回结果生成最终回复

这是一个 Agent Loop，最多执行 5 轮工具调用（kMaxAgentRounds = 5），避免无限循环。

**Q：你注册了哪些工具？各自做什么？**

A：共注册了以下内置工具：
- **list_directory**：列出指定目录或当前目录的文件列表
- **search_files**：按文件名关键词、类型、大小范围搜索文件
- **get_file_info**：获取某个文件的详细信息（大小、时间、路径）
- **get_transfer_tasks**：查看上传/下载/已完成的传输任务
- **get_disk_info**：查看磁盘空间使用情况
- **get_shared_folders**：列出共享文件夹
- **tag_file**：为文件添加标签和描述
- **search_by_tag**：按标签搜索文件
- **create_folder**：创建新文件夹
- **move_file**：移动文件到指定目录

**Q：Prompt 是怎么构建的？**

A：使用 XAIPromptBuilder 类，最终 prompt 结构为：
```
[系统指令]  — 角色设定 + 工具描述 + 输出格式要求
[当前环境]  — 当前目录路径 + 文件列表快照
[对话历史]  — 多轮 user/assistant/tool 消息
请根据以上信息回复（只输出 JSON）：
```

还实现了历史裁剪（TrimHistory），保留最近 6 轮对话，工具调用链（assistant + tool）成对删除，避免上下文截断。

**Q：对接了哪些大模型 API？区别是什么？**

A：支持三种：
- **Ollama**（默认）：本地部署，免费，延迟低，适合开发测试，使用 /api/generate 接口
- **DashScope**（阿里通义）：云端 API，支持 qwen 系列，OpenAI 兼容接口
- **Claude**：Anthropic 的 API，格式不同（用 messages 数组），需要单独构建请求体

代码中通过 api_type 字段区分，分别调用 BuildOpenAIBody/BuildClaudeBody/BuildOllamaBody 构建请求。

---

## 六、C++ 技术深度

**Q：项目中哪里用到了多线程？怎么保证线程安全？**

A：多处使用多线程：
- **libevent 线程池**：XThreadPool 管理工作线程，处理网络 I/O 回调
- **AI 子线程**：XAIAssistant::SendUserMessage 创建 worker_thread_ 执行 LLM 调用，避免阻塞 GUI
- **网关代理线程**：XServiceProxy::Main() 独立线程定时刷新服务列表
- **心跳线程**：XRegisterClient 定时发送心跳

线程安全手段：
- std::mutex + lock_guard 保护共享数据（config_mutex_, history_mutex_, service_map_mutex 等）
- std::atomic<bool> alive_ 控制线程退出
- std::once_flag + call_once 保证 curl 全局只初始化一次
- QMetaObject::invokeMethod(Qt::QueuedConnection) 确保子线程回调在主线程执行

**Q：IFileContext 接口为什么要这样设计？**

A：这是依赖倒置原则的应用：
- xai_lib 是一个独立库，不应依赖 GUI 层的 XFileManager
- 通过定义 IFileContext 纯虚接口，xai_lib 只依赖抽象
- GUI 层（XAIPanel）实现该接口并注入到 XAIAssistant
- 好处：xai_lib 可独立编译测试，GUI 层可替换（比如换成命令行客户端）

**Q：智能指针在项目中怎么用的？**

A：`std::shared_ptr<std::atomic<bool>> alive_` 是一个典型例子：
- AI 请求在子线程中执行，但 XAIAssistant 对象可能在主线程被销毁
- 用 shared_ptr 让子线程持有 alive_ 的引用计数，即使对象析构 alive_ 也不会野指针
- 子线程通过 alive_->load() 检查对象是否还存在，避免访问已销毁对象

---

## 七、项目亮点总结

**Q：这个项目最大的技术挑战是什么？怎么解决的？**

A：（可以选择以下任一方向回答）

方向一：**网关的异步消息转发与回调匹配**
- 挑战：网关收到客户端请求后转发给后端服务，后端异步返回结果，需要准确回传给原始客户端
- 解决：用消息头的 msg_id 字段存储 XRouterHandle 指针地址，后端返回时通过 callback_task_ map 找到原始连接，回传响应

方向二：**AI Agent Loop 的可靠性**
- 挑战：LLM 返回格式不确定，可能返回非法 JSON、不存在的工具名、死循环调用
- 解决：设置最大轮次限制（5 轮），工具执行加 try-catch，参数校验，JSON 解析容错

方向三：**文件断点续传的数据一致性**
- 挑战：上传中断后，最后一个分片可能只写了一部分，直接追加会导致文件损坏
- 解决：重新上传时将已存在文件截断到分片边界对齐位置，丢弃可能不完整的最后一片

---

## 八、共享文件夹与权限

**Q：共享文件夹的权限控制是怎么做的？**

A：三级权限模型：
- **read（1）**：只能浏览和下载共享文件夹中的文件
- **write（2）**：可以上传文件到共享文件夹
- **admin（3）**：可以添加/移除成员、删除文件、删除整个共享文件夹

权限存储在 MySQL 表 shared_folder_permissions（folder_id, username, permission_level）中。

每次文件操作（上传、下载、删除）前都会调用 GetPermLevel() 验证权限：
- 上传需要 write(2) 以上
- 下载需要 read(1) 以上
- 删除文件需要 admin(3)
- 删除文件夹只有 owner 可以操作

**Q：共享文件夹的路径安全是怎么保证的？**

A：多层防护：
1. **路径遍历防护**：检查路径中每个组件，拒绝包含 ".." 的路径
2. **Sandbox 检查**：使用 filesystem::weakly_canonical() 解析真实路径，确认没有通过符号链接逃逸出沙箱目录
3. **文件名校验**：IsFilenameSafe() 检查文件名合法性
4. **服务端二次验证**：即使客户端绕过共享服务直接请求上传/下载服务，这些服务也会独立查询数据库验证权限

---

## 九、设计模式

**Q：项目中用到了哪些设计模式？**

A：
- **单例模式**：XRegisterClient::Get()、XServiceProxy::Get()、XAuthDao::Get()、XAIFileTags::Instance() 等，用于全局唯一的管理器
- **工厂方法**：XService::CreateServiceHandle() 虚函数，各服务子类（XRouterServer、XUploadServer 等）创建不同的 Handle 对象
- **策略模式**：XServiceProxyClient::Create() 根据 service_name 创建不同类型的代理（如 XAuthProxy）
- **观察者模式**：Qt 信号槽机制（OnAIReply、OnHighlightFiles 等信号）
- **模板方法**：XServiceHandle 基类定义 ReadCB/Close 等虚函数框架，子类实现具体逻辑
- **依赖注入**：IFileContext 接口注入到 XAIAssistant，解耦 AI 库和 GUI 层


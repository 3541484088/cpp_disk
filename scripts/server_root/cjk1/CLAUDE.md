# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 工作区结构

本目录包含两个独立的 C++ 项目，各自有独立的 git 仓库和 CLAUDE.md：

- [AI_YunCunChu/](AI_YunCunChu/) — 基于 Docker 的私有云存储系统，含 AI 语义检索
- [cpp_disk/](cpp_disk/) — 分布式云盘系统，8 个 C++17 微服务 + Qt6 桌面客户端

详细的构建命令、架构说明和开发约定请参阅各子项目的 CLAUDE.md。

---

## 项目一：AI_YunCunChu

### 项目简介

私有云文件管理平台，核心亮点是 **AI 智能语义检索**——用自然语言描述查找文件。整套系统通过 Docker Compose 部署，3 个容器协作运行。

### 技术栈

| 层级 | 技术 |
|------|------|
| 前端 | React 18 + Ant Design 5 + Emotion CSS-in-JS |
| 后端 | C/C++ FastCGI（13 个常驻进程，spawn-fcgi 管理） |
| Web 服务器 | Nginx 1.20.2 + fastdfs-nginx-module |
| 文件存储 | FastDFS v6.06（分布式文件系统） |
| 数据库 | MySQL 8.0 |
| 缓存 | Redis（应用容器内嵌） |
| AI 引擎 | 阿里百炼 DashScope API + FAISS v1.7.2 向量索引 |
| 容器化 | Docker Compose，自定义桥接网络 172.30.0.0/16 |

### 容器编排

| 容器 | IP | 对外端口 | 职责 |
|------|----|---------|------|
| tc_fcgi_mysql | 172.30.0.2 | 3307→3306 | MySQL 数据库 |
| tc_fcgi_nginx_fastdfs | 172.30.0.3 | 80, 443 | Nginx 反向代理 + FastDFS 文件存储 |
| tc_fcgi_app | 172.30.0.4 | 10000–10012 | 13 个 FastCGI 业务进程 + Redis |

### 请求路由

```
浏览器 HTTPS 443
  ├── 静态资源 (/, *.js, *.css)  →  React 构建产物 /app/front/
  ├── 文件下载 /group*/M**        →  FastDFS ngx_fastdfs_module
  └── API /api/*                  →  FastCGI 转发到 172.30.0.4:10000-10012
```

### 核心功能模块

- **登录/注册**：密码存储 `MD5(salt + MD5(password))`，Token 用 DES+Base64 生成并存入 Redis
- **普通上传**：前端先 POST `/api/md5` 做 MD5 秒传检测，命中则直接复用；未命中则 multipart 上传到 FastDFS
- **分片上传**：10 MB 一片，支持断点续传（`chunk_init` 返回已上传分片列表）
- **AI 语义搜索**：图片用 Qwen-VL 生成中文描述，文本文件直接提取内容，统一用 text-embedding-v3 转为 1024 维向量，存入 MySQL `file_ai_desc` 表并建 FAISS `IndexFlatIP` 索引；搜索时向量化查询词做余弦相似度检索（Top-10，阈值 0.45）
- **图床分享**：生成带提取码的图片分享链接

### 关键源文件

| 文件 | 职责 |
|------|------|
| [src_cgi/ai_cgi.cpp](AI_YunCunChu/src_cgi/ai_cgi.cpp) | AI 检索入口：describe / search / rebuild / apikey 管理 |
| [common/dashscope_api.cpp](AI_YunCunChu/common/dashscope_api.cpp) | DashScope REST 调用（libcurl）：Qwen-VL + text-embedding-v3 |
| [common/faiss_wrapper.cpp](AI_YunCunChu/common/faiss_wrapper.cpp) | FAISS 索引封装：创建/加载/添加/搜索/保存/重置 |
| [common/util_cgi.c](AI_YunCunChu/common/util_cgi.c) | multipart 解析、URL 解码、Token 验证 |
| [src_cgi/upload_cgi.c](AI_YunCunChu/src_cgi/upload_cgi.c) | 普通上传：multipart 解析 → FastDFS → MySQL |
| [src_cgi/login_cgi.c](AI_YunCunChu/src_cgi/login_cgi.c) | 登录：加盐 MD5 验证 + DES Token 生成 |

---

## 项目二：cpp_disk

### 项目简介

分布式云盘系统，由 **8 个 C++17 微服务**和一个 **Qt6 桌面客户端**组成。服务间通过自定义 Protobuf 协议 + libevent/TLS 加密通信，通过注册中心动态发现彼此。

### 技术栈

| 层级 | 技术 |
|------|------|
| 语言/标准 | C++17（MSVC 2022 / GCC） |
| 网络框架 | libevent（bufferevent + TLS） |
| 序列化 | Protobuf 3.21.12 |
| 数据库 | MySQL（LXMysql C++ 封装） |
| 桌面客户端 | Qt6 Widgets |
| 构建系统 | CMake 3.20+ with presets |

### 服务一览

| 服务 | 端口 | 职责 |
|------|------|------|
| register_server | 20018 | 服务注册与发现中心，**必须最先启动** |
| xauth_server | 20012 | 用户认证，Token 管理 |
| xlog_server | 20017 | 集中日志收集 |
| config_server | 20015 | 配置中心 |
| xms_gateway | 20010 | API 网关，客户端唯一入口，负责鉴权 + 负载均衡转发 |
| xms_dir_service | 20011 | 目录与文件元数据管理 |
| xms_upload_service | 20013 | 文件上传，AES 加密存储，MD5 秒传 |
| xms_download_service | 20014 | 文件下载 |

启动顺序有硬依赖，直接运行 `./scripts/start_services.sh` 或按上表顺序手动启动。

### 核心框架层（xplatform/ 静态库）

框架采用继承链分层：

```
XTask
  └── XComTask          libevent bufferevent 封装，TLS，自动重连，定时器
        └── XMsgEvent   Protobuf 消息帧解析，RegCB() 回调分发表
              └── XServiceHandle   每连接处理器基类
                    ├── XRouterHandle      (网关)
                    ├── XUploadHandle      (上传服务)
                    ├── XAuthHandle        (认证服务)
                    └── XRegisterHandle    (注册中心)

XService              TCP 监听基类，内置双线程池（监听池 + 客户端池）
```

### 通信协议

每条消息 = `XMsgHead`（Protobuf，含 `type` / `token` / `service_name` / `md5` / `offset`）+ 对应类型的 Protobuf body。消息类型枚举定义在 `xplatform/xmsg_type.pb.h`，客户端专用类型在 `xdisk_pb/`。单条消息上限 100 MB。

### API 网关工作原理

`XRouterHandle::ReadCB` 解析消息类型 → `XAuthProxy` 验证 Token → `XServiceProxy` 按服务名从连接池（`map<service_name, vector<XServiceProxyClient*>>`）轮询转发。连接池定期从 register_server 刷新。

### 文件上传协议

```
UPLOAD_FILE_REQ（文件名/大小/MD5）
  → 服务端返回 MD5 命中则秒传，否则进入分片流程
  → SEND_SLICE_REQ × N（每片二进制数据，AES 加密）
  → UPLOAD_FILE_END_REQ（结束，触发落盘）
```

### 桌面客户端（xms_disk_client_gui/）

Qt6 Widgets 应用，直连网关 20010 端口，链接 `xplatform`、`xauth_client`、`xdisk_pb`。功能包括：上传/下载/删除、目录导航、任务列表。

### 关键源文件

| 文件 | 职责 |
|------|------|
| [xplatform/xcom_task.h](cpp_disk/xplatform/xcom_task.h) | 核心：libevent bufferevent + TLS + 自动重连 |
| [xplatform/xmsg_event.h](cpp_disk/xplatform/xmsg_event.h) | Protobuf 帧解析 + `RegCB()` 分发 |
| [xplatform/xservice.h](cpp_disk/xplatform/xservice.h) | TCP 监听基类 + 双线程池 |
| [xplatform/xtools.h](cpp_disk/xplatform/xtools.h) | 工具集：MD5、Base64、AES、文件/目录操作 |
| [xms_gateway/xservice_proxy.h](cpp_disk/xms_gateway/xservice_proxy.h) | 负载均衡连接池 |
| [xms_upload_service/xupload_handle.h](cpp_disk/xms_upload_service/xupload_handle.h) | 上传协议实现 |
| [LXMysql/LXMysql.h](cpp_disk/LXMysql/LXMysql.h) | MySQL C++ 封装（事务、批量插入、结果集） |

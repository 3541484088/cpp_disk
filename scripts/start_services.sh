#!/bin/bash
# ============================================================
# 云盘服务启动脚本 (Ubuntu)
# 启动顺序: 注册中心 → 网关 → 目录服务 → 上传服务 → 下载服务
# ============================================================

set -e

BIN_DIR="$(cd "$(dirname "$0")/../build/linux_debug/bin" && pwd)"

# 创建存储目录
mkdir -p ./server_root

echo "========== 启动注册中心 (xms_register_server) =========="
"${BIN_DIR}/register_server" 20018 &
REG_PID=$!
sleep 2

echo "========== 启动鉴权服务 (xauth_server) =========="
"${BIN_DIR}/xauth_server" 20012 127.0.0.1 20018 &
AUTH_PID=$!
sleep 1

echo "========== 启动日志服务 (xlog_server) =========="
# xlog_server: argv[1]=register_ip, argv[2]=register_port, argv[3]=service_port
"${BIN_DIR}/xlog_server" 127.0.0.1 20018 20030 &
LOG_PID=$!
sleep 1

echo "========== 启动配置服务 (config_server) =========="
# config_server: argv[1]=register_ip, argv[2]=register_port, argv[3]=service_port
"${BIN_DIR}/config_server" 127.0.0.1 20018 20019 &
CFG_PID=$!
sleep 1

echo "========== 启动 API 网关 (xms_gateway) =========="
# xms_gateway: argv[1]=port, argv[2]=register_ip, argv[3]=register_port
"${BIN_DIR}/xms_gateway" 20010 127.0.0.1 20018 &
GW_PID=$!
sleep 2

echo "========== 启动目录服务 (xms_dir_service) =========="
# xms_dir_service: argv[1]=register_ip, argv[2]=register_port
"${BIN_DIR}/xms_dir_service" 127.0.0.1 20018 &
DIR_PID=$!
sleep 1

echo "========== 启动上传服务 (xms_upload_service) =========="
# xms_upload_service: argv[1]=register_ip, argv[2]=register_port, argv[3]=service_port
"${BIN_DIR}/xms_upload_service" 127.0.0.1 20018 20100 &
UP_PID=$!
sleep 1

echo "========== 启动下载服务 (xms_download_service) =========="
# xms_download_service: argv[1]=register_ip, argv[2]=register_port, argv[3]=service_port
"${BIN_DIR}/xms_download_service" 127.0.0.1 20018 20200 &
DL_PID=$!

echo ""
echo "所有服务已启动!"
echo "  注册中心 PID: ${REG_PID}"
echo "  鉴权服务 PID: ${AUTH_PID}"
echo "  日志服务 PID: ${LOG_PID}"
echo "  配置服务 PID: ${CFG_PID}"
echo "  网关 PID:     ${GW_PID}"
echo "  目录服务 PID:  ${DIR_PID}"
echo "  上传服务 PID:  ${UP_PID}"
echo "  下载服务 PID:  ${DL_PID}"
echo ""
echo "按 Ctrl+C 停止所有服务..."

# 捕获 Ctrl+C 信号，优雅退出
trap "echo '停止所有服务...'; kill ${REG_PID} ${AUTH_PID} ${LOG_PID} ${CFG_PID} ${GW_PID} ${DIR_PID} ${UP_PID} ${DL_PID} 2>/dev/null; exit 0" INT TERM

# 等待所有后台进程
wait
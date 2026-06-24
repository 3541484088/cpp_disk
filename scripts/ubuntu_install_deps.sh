#!/bin/bash
# ============================================================
# Ubuntu 依赖一键安装脚本 (Ubuntu 22.04 / 24.04 通用)
# 用法: chmod +x ubuntu_install_deps.sh && ./ubuntu_install_deps.sh
# ============================================================

set -e

ENV_ROOT="/opt/xdisk_env"
PROTOBUF_VERSION="3.21.12"
NPROC=$(nproc)

echo "========================================="
echo " 云盘项目 Ubuntu 依赖安装"
echo " 目标环境: ${ENV_ROOT}"
echo " protobuf:  ${PROTOBUF_VERSION} (从源码编译)"
echo "========================================="
echo ""

# ---------- 1. 系统包 ----------
echo "========== 更新 apt 源 =========="
sudo apt update

echo ""
echo "========== 安装编译工具链 =========="
sudo apt install -y build-essential cmake g++ pkg-config wget autoconf automake libtool

echo ""
echo "========== 安装 libevent =========="
# libevent-dev 会同时拉 libevent-openssl-2.1-7 (22.04) 或 libevent-openssl-2.1-7t64 (24.04)
sudo apt install -y libevent-dev

echo ""
echo "========== 安装 OpenSSL =========="
sudo apt install -y libssl-dev

echo ""
echo "========== 安装 MySQL 客户端库 =========="
# Ubuntu 24.04 用 default-libmysqlclient-dev 更稳妥
sudo apt install -y libmysqlclient-dev || sudo apt install -y default-libmysqlclient-dev

echo ""
echo "========== 安装 libcurl (xai_lib 需要) =========="
sudo apt install -y libcurl4-openssl-dev

echo ""
echo "========== 安装 jsoncpp (xai_lib 需要) =========="
sudo apt install -y libjsoncpp-dev

echo ""
echo "========== 安装 Qt6 (GUI客户端需要，可选) =========="
sudo apt install -y qt6-base-dev

# ---------- 2. 编译 protobuf 3.21.12 ----------
# 必须与 Windows 端用的版本完全一致，否则 .pb.cc/.pb.h 不兼容
echo ""
echo "========== 编译 Protobuf ${PROTOBUF_VERSION} =========="

PROTOBUF_SRC="/tmp/protobuf-${PROTOBUF_VERSION}"
PROTOBUF_INSTALL="${ENV_ROOT}/protobuf"

if [ -f "${PROTOBUF_INSTALL}/lib/libprotobuf.a" ] || \
   [ -f "${PROTOBUF_INSTALL}/lib/libprotobuf.so" ]; then
    echo "Protobuf ${PROTOBUF_VERSION} 已安装到 ${PROTOBUF_INSTALL}，跳过编译"
else
    if [ ! -d "${PROTOBUF_SRC}" ]; then
        echo "下载 protobuf-cpp-${PROTOBUF_VERSION}.tar.gz ..."
        wget -q --show-progress \
            "https://github.com/protocolbuffers/protobuf/releases/download/v${PROTOBUF_VERSION}/protobuf-cpp-${PROTOBUF_VERSION}.tar.gz" \
            -O /tmp/protobuf.tar.gz
        mkdir -p "${PROTOBUF_SRC}"
        tar -xzf /tmp/protobuf.tar.gz -C /tmp/
    fi

    cd "${PROTOBUF_SRC}"
    echo "配置..."
    ./configure --prefix="${PROTOBUF_INSTALL}" --disable-shared 2>&1 | tail -5

    echo "编译 (使用 ${NPROC} 个核心)..."
    make -j${NPROC} 2>&1 | tail -5

    echo "安装到 ${PROTOBUF_INSTALL} ..."
    sudo mkdir -p "${PROTOBUF_INSTALL}"
    sudo make install 2>&1 | tail -3

    # 让 CMake find_package 能找到
    sudo ldconfig
    echo "Protobuf ${PROTOBUF_VERSION} 安装完成"
fi

# ---------- 3. 创建环境目录结构 (对标 Windows E:/environment/) ----------
echo ""
echo "========== 创建环境目录结构 =========="
sudo mkdir -p "${ENV_ROOT}/include"
sudo mkdir -p "${ENV_ROOT}/lib"
sudo mkdir -p "${ENV_ROOT}/bin"

# 把系统库的 include 符号链接到统一环境目录
# (实际上 CMake 会用明确路径，这里为了和 Windows 结构对齐)
echo "环境目录: ${ENV_ROOT}"

echo ""
echo "========================================="
echo " 安装完成！"
echo ""
echo "构建命令:"
echo "  cmake --preset linux-debug"
echo "  cmake --build --preset linux-debug-build -j${NPROC}"
echo ""
echo "启动服务:"
echo "  ./scripts/start_services.sh"
echo "========================================="
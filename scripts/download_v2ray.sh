#!/bin/bash
# V2Ray Core 下载和安装脚本

set -e

# 配置
V2RAY_VERSION="latest"
V2RAY_DIR="data/v2ray"
ARCH=$(uname -m)
OS=$(uname -s | tr '[:upper:]' '[:lower:]')

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

log_step() {
    echo -e "${BLUE}[STEP]${NC} $1"
}

# 检测架构
detect_arch() {
    case "$ARCH" in
        x86_64)
            ARCH="64"
            ;;
        aarch64|arm64)
            ARCH="arm64-v8a"
            ;;
        armv7l)
            ARCH="arm32-v7a"
            ;;
        *)
            log_error "Unsupported architecture: $ARCH"
            exit 1
            ;;
    esac
}

# 获取最新版本号
get_latest_version() {
    log_step "Fetching latest V2Ray version..."
    
    if command -v curl >/dev/null 2>&1; then
        VERSION=$(curl -s https://api.github.com/repos/v2fly/v2ray-core/releases/latest | grep '"tag_name"' | sed -E 's/.*"([^"]+)".*/\1/')
    elif command -v wget >/dev/null 2>&1; then
        VERSION=$(wget -qO- https://api.github.com/repos/v2fly/v2ray-core/releases/latest | grep '"tag_name"' | sed -E 's/.*"([^"]+)".*/\1/')
    else
        log_error "Neither curl nor wget is available. Please install one of them."
        exit 1
    fi
    
    if [ -z "$VERSION" ]; then
        log_error "Failed to fetch latest version"
        exit 1
    fi
    
    log_info "Latest version: $VERSION"
}

# 下载 V2Ray
download_v2ray() {
    log_step "Downloading V2Ray Core..."
    
    DOWNLOAD_URL="https://github.com/v2fly/v2ray-core/releases/download/${VERSION}/v2ray-${OS}-${ARCH}.zip"
    TEMP_FILE="/tmp/v2ray.zip"
    
    log_info "Download URL: $DOWNLOAD_URL"
    
    if command -v curl >/dev/null 2>&1; then
        curl -L -o "$TEMP_FILE" "$DOWNLOAD_URL"
    elif command -v wget >/dev/null 2>&1; then
        wget -O "$TEMP_FILE" "$DOWNLOAD_URL"
    fi
    
    if [ ! -f "$TEMP_FILE" ]; then
        log_error "Download failed"
        exit 1
    fi
    
    log_info "Download completed: $TEMP_FILE"
}

# 解压和安装
install_v2ray() {
    log_step "Installing V2Ray Core..."
    
    # 创建目录
    mkdir -p "$V2RAY_DIR"
    
    # 解压
    if command -v unzip >/dev/null 2>&1; then
        unzip -o "$TEMP_FILE" -d "$V2RAY_DIR"
    else
        log_error "unzip is not installed. Please install it first."
        exit 1
    fi
    
    # 设置执行权限
    chmod +x "$V2RAY_DIR/v2ray"
    
    # 下载 geoip 和 geosite 数据
    log_step "Downloading GeoIP and GeoSite data..."
    
    GEOIP_URL="https://github.com/v2fly/geoip/releases/latest/download/geoip.dat"
    GEOSITE_URL="https://github.com/v2fly/domain-list-community/releases/latest/download/dlc.dat"
    
    if command -v curl >/dev/null 2>&1; then
        curl -L -o "$V2RAY_DIR/geoip.dat" "$GEOIP_URL"
        curl -L -o "$V2RAY_DIR/geosite.dat" "$GEOSITE_URL"
    elif command -v wget >/dev/null 2>&1; then
        wget -O "$V2RAY_DIR/geoip.dat" "$GEOIP_URL"
        wget -O "$V2RAY_DIR/geosite.dat" "$GEOSITE_URL"
    fi
    
    # 清理临时文件
    rm -f "$TEMP_FILE"
    
    log_info "V2Ray installed to: $V2RAY_DIR"
}

# 验证安装
verify_installation() {
    log_step "Verifying installation..."
    
    if [ ! -f "$V2RAY_DIR/v2ray" ]; then
        log_error "V2Ray binary not found"
        exit 1
    fi
    
    VERSION_OUTPUT=$("$V2RAY_DIR/v2ray" version 2>&1 | head -n 1)
    log_info "Installed: $VERSION_OUTPUT"
    
    # 检查 geoip 和 geosite
    if [ -f "$V2RAY_DIR/geoip.dat" ]; then
        GEOIP_SIZE=$(du -h "$V2RAY_DIR/geoip.dat" | cut -f1)
        log_info "GeoIP data: $GEOIP_SIZE"
    else
        log_warn "GeoIP data not found"
    fi
    
    if [ -f "$V2RAY_DIR/geosite.dat" ]; then
        GEOSITE_SIZE=$(du -h "$V2RAY_DIR/geosite.dat" | cut -f1)
        log_info "GeoSite data: $GEOSITE_SIZE"
    else
        log_warn "GeoSite data not found"
    fi
}

# 主函数
main() {
    echo "========================================"
    echo "  V2Ray Core Installation Script"
    echo "========================================"
    echo ""
    
    # 检查是否已安装
    if [ -f "$V2RAY_DIR/v2ray" ]; then
        log_warn "V2Ray is already installed at $V2RAY_DIR"
        read -p "Do you want to reinstall? (y/N): " -n 1 -r
        echo
        if [[ ! $REPLY =~ ^[Yy]$ ]]; then
            log_info "Installation cancelled"
            exit 0
        fi
    fi
    
    detect_arch
    get_latest_version
    download_v2ray
    install_v2ray
    verify_installation
    
    echo ""
    echo "========================================"
    log_info "V2Ray Core installation completed!"
    echo "========================================"
    echo ""
    echo "Next steps:"
    echo "1. Build the project: ./build.sh"
    echo "2. Run the application: ./build/ovpn-client"
    echo "3. Configure V2Ray proxy in the GUI"
    echo ""
}

# 运行主函数
main
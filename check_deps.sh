#!/bin/bash
# VPN客户端依赖自动检测与安装脚本（Debian/Ubuntu专用）
set -e

# 需要的包列表
REQUIRED_PKGS=(
    libgtk-3-0
    libgtk-3-dev
    libnm-dev
    libayatana-appindicator3-1
    libayatana-appindicator3-dev
    uuid-dev
    libglib2.0-dev
    pkg-config
    build-essential
    dbus-x11
    policykit-1-gnome
)

MISSING_PKGS=()

# 检查每个包是否已安装
for PKG in "${REQUIRED_PKGS[@]}"; do
    dpkg-query -W -f='${Status}' "$PKG" 2>/dev/null | grep -q "install ok installed" || MISSING_PKGS+=("$PKG")
done

if [ ${#MISSING_PKGS[@]} -eq 0 ]; then
    echo "✅ All required dependencies are already installed."
    exit 0
else
    echo "❗ Missing packages detected: ${MISSING_PKGS[*]}"
    echo "Installing missing dependencies. You may be prompted for your password."
    sudo apt update
    sudo apt install -y "${MISSING_PKGS[@]}"
    if [ $? -eq 0 ]; then
        echo "✅ Dependencies installed successfully."
        exit 0
    else
        echo "❌ Failed to install some packages. Please check your network and package sources."
        exit 1
    fi
fi

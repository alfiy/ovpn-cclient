#!/bin/bash

set -e

echo "Building OVPN Client (C + GTK3 + AppIndicator)..."

# 检查系统依赖
echo "Checking system dependencies..."
MISSING_PACKAGES=()

# 检查开发工具
if ! command -v gcc &> /dev/null; then
    MISSING_PACKAGES+=("build-essential")
fi

if ! command -v pkg-config &> /dev/null; then
    MISSING_PACKAGES+=("pkg-config")
fi

# 检查GTK3开发包
if ! pkg-config --exists gtk+-3.0; then
    MISSING_PACKAGES+=("libgtk-3-dev")
fi

# 检查NetworkManager开发包
if ! pkg-config --exists libnm; then
    MISSING_PACKAGES+=("libnm-dev")
fi

# 检查AppIndicator开发包
if ! pkg-config --exists appindicator3-0.1; then
    MISSING_PACKAGES+=("libappindicator3-dev")
fi

# 检查UUID开发包
if ! pkg-config --exists uuid; then
    MISSING_PACKAGES+=("uuid-dev")
fi

# 安装缺失的包
if [ ${#MISSING_PACKAGES[@]} -ne 0 ]; then
    echo "Missing required packages: ${MISSING_PACKAGES[*]}"
    echo "Installing missing packages..."
    sudo apt update
    sudo apt install -y "${MISSING_PACKAGES[@]}"
fi

# 编译程序
echo "Compiling OVPN Client..."
make clean || true
make

# 创建桌面文件
echo "Creating desktop entry..."
cat > ovpn-client.desktop << 'EOF'
[Desktop Entry]
Version=1.0
Type=Application
Name=OVPN Client
Comment=OpenVPN Configuration Client and Manager (C + GTK3)
Exec=/usr/local/bin/ovpn-client
Icon=network-vpn
Terminal=false
Categories=Network;Security;
Keywords=VPN;OpenVPN;Network;Security;
StartupNotify=true
EOF

# 创建安装脚本
cat > install_c.sh << 'EOF'
#!/bin/bash
echo "Installing OVPN Client (C version)..."

# 复制可执行文件到系统路径
sudo cp ovpn-client /usr/local/bin/
sudo chmod +x /usr/local/bin/ovpn-client

# 安装桌面条目
mkdir -p ~/.local/share/applications
cp ovpn-client.desktop ~/.local/share/applications/

# 更新桌面数据库
update-desktop-database ~/.local/share/applications/ || true

echo "Installation complete!"
echo "You can now run 'ovpn-client' from terminal or find it in your applications menu."
echo "The application will also appear in the system tray when running."
EOF

chmod +x install_c.sh

echo "C version build complete!"
echo "Files created:"
echo "  - ovpn-client (executable)"
echo "  - ovpn-client.desktop (desktop entry)"
echo "  - install_c.sh (installation script)"
echo ""
echo "To install system-wide, run: ./install_c.sh"
echo ""
echo "Features of C version:"
echo "  ✓ No Python dependencies"
echo "  ✓ Small binary size (~100KB vs ~50MB Python)"
echo "  ✓ Fast startup time"
echo "  ✓ System tray integration with AppIndicator"
echo "  ✓ Native GTK3 interface"
echo "  ✓ Direct NetworkManager integration"
echo "  ✓ Easy distribution (single binary)"
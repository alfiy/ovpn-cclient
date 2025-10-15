#!/usr/bin/env bash
# ============================================================
# setup_sysroot.sh — 自动创建 ARM64 Ubuntu rootfs 供交叉编译使用
# 作者: Alfiy
# 适用于 Ubuntu 20.04 / 22.04 主机
# ============================================================

set -e

SYSROOT_DIR="/opt/arm64-rootfs"
ARCH="arm64"
UBUNTU_CODENAME="jammy"  # Ubuntu 22.04
MIRROR_URL="http://ports.ubuntu.com/"

# ============================================================
# 🧩 Step 1. 检查依赖
# ============================================================
echo "[INFO] Checking required tools..."
for cmd in debootstrap qemu-aarch64-static chroot; do
    if ! command -v $cmd >/dev/null 2>&1; then
        echo "[ERROR] Missing command: $cmd"
        echo "        Please install it first: sudo apt install -y debootstrap qemu-user-static"
        exit 1
    fi
done

# ============================================================
# 🧩 Step 2. 创建 sysroot 目录
# ============================================================
echo "[INFO] Creating rootfs directory at $SYSROOT_DIR ..."
sudo mkdir -p "$SYSROOT_DIR"

# ============================================================
# 🧩 Step 3. 使用 debootstrap 构建 ARM64 Ubuntu
# ============================================================
if [ ! -d "$SYSROOT_DIR/usr/bin" ]; then
    echo "[INFO] Bootstrapping ARM64 Ubuntu ($UBUNTU_CODENAME)..."
    sudo debootstrap --arch=$ARCH $UBUNTU_CODENAME "$SYSROOT_DIR" "$MIRROR_URL"
else
    echo "[INFO] Rootfs already exists — skipping debootstrap."
fi

# ============================================================
# 🧩 Step 4. 复制 QEMU 模拟器以便 chroot
# ============================================================
if [ ! -f "$SYSROOT_DIR/usr/bin/qemu-aarch64-static" ]; then
    echo "[INFO] Copying QEMU emulator into sysroot..."
    sudo cp /usr/bin/qemu-aarch64-static "$SYSROOT_DIR/usr/bin/"
fi

# ============================================================
# 🧩 Step 5. 在 chroot 环境中安装交叉编译所需的库
# ============================================================
echo "[INFO] Installing required development packages inside sysroot..."
sudo chroot "$SYSROOT_DIR" /bin/bash -c "
    apt-get update &&
    DEBIAN_FRONTEND=noninteractive apt-get install -y \
        libgtk-3-dev libnm-dev libayatana-appindicator3-dev \
        libglib2.0-dev uuid-dev build-essential pkg-config
"

# ============================================================
# 🧩 Step 6. 检查关键文件是否存在
# ============================================================
if [ ! -d "$SYSROOT_DIR/usr/lib/aarch64-linux-gnu/pkgconfig" ]; then
    echo "[ERROR] pkgconfig directory missing — installation may have failed."
    exit 1
fi

echo "[SUCCESS] ARM64 sysroot setup completed successfully."
echo

# ============================================================
# 🧩 Step 7. 打印使用提示
# ============================================================
echo "✅ Next steps:"
echo "------------------------------------------------------------"
echo "export CROSS_COMPILE=aarch64-linux-gnu-"
echo "export SYSROOT=$SYSROOT_DIR"
echo "export PKG_CONFIG_SYSROOT_DIR=\$SYSROOT"
echo "export PKG_CONFIG_PATH=\$SYSROOT/usr/lib/aarch64-linux-gnu/pkgconfig:\$SYSROOT/usr/share/pkgconfig"
echo
echo "Then you can build your project with:"
echo "./build.sh"
echo "------------------------------------------------------------"
echo "[INFO] Example test:"
echo "pkg-config --cflags gtk+-3.0"
echo
echo "[DONE] Sysroot for ARM64 is ready at: $SYSROOT_DIR"


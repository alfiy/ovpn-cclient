#!/bin/bash
set -e

# 确保 D-Bus session 存在
if [ -z "$DBUS_SESSION_BUS_ADDRESS" ]; then
    export $(dbus-launch)
fi

# 确保有 DISPLAY
if [ -z "$DISPLAY" ]; then
    export DISPLAY=:0
fi

# 启动 polkit-gnome 认证代理（后台运行，避免重复启动）
if ! pgrep -x polkit-gnome-authentication-agent-1 >/dev/null; then
    /usr/lib/policykit-1-gnome/polkit-gnome-authentication-agent-1 &
    sleep 1
fi

# 启动你的 VPN 客户端
exec ./build/ovpn-client

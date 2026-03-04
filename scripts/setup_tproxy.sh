#!/bin/bash
# V2Ray 透明代理 iptables 配置脚本
# 需要 root 权限运行

set -e

# 默认配置
V2RAY_PORT=${2:-12345}
OPENVPN_SUBNET="10.8.0.0/24"

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
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

# 检查 root 权限
if [ "$EUID" -ne 0 ]; then
    log_error "This script must be run as root"
    exit 1
fi

# 启动透明代理
start_tproxy() {
    log_info "Starting V2Ray transparent proxy..."
    
    # 检查是否已经配置
    if iptables -t mangle -L V2RAY_MASK >/dev/null 2>&1; then
        log_warn "TProxy rules already exist, cleaning up first..."
        stop_tproxy
    fi
    
    # 创建自定义链
    iptables -t mangle -N V2RAY_MASK
    iptables -t mangle -N V2RAY
    
    # 跳过内网和保留地址
    iptables -t mangle -A V2RAY -d 0.0.0.0/8 -j RETURN
    iptables -t mangle -A V2RAY -d 10.0.0.0/8 -j RETURN
    iptables -t mangle -A V2RAY -d 127.0.0.0/8 -j RETURN
    iptables -t mangle -A V2RAY -d 169.254.0.0/16 -j RETURN
    iptables -t mangle -A V2RAY -d 172.16.0.0/12 -j RETURN
    iptables -t mangle -A V2RAY -d 192.168.0.0/16 -j RETURN
    iptables -t mangle -A V2RAY -d 224.0.0.0/4 -j RETURN
    iptables -t mangle -A V2RAY -d 240.0.0.0/4 -j RETURN
    
    # 跳过 OpenVPN 内网段（确保内网流量走 OpenVPN）
    iptables -t mangle -A V2RAY -d ${OPENVPN_SUBNET} -j RETURN
    
    # 其他流量转发到 V2Ray
    iptables -t mangle -A V2RAY -p tcp -j TPROXY --on-port ${V2RAY_PORT} --tproxy-mark 1
    iptables -t mangle -A V2RAY -p udp -j TPROXY --on-port ${V2RAY_PORT} --tproxy-mark 1
    
    # 应用到 PREROUTING 链
    iptables -t mangle -A PREROUTING -j V2RAY
    
    # 本机流量处理
    iptables -t mangle -A V2RAY_MASK -d 0.0.0.0/8 -j RETURN
    iptables -t mangle -A V2RAY_MASK -d 10.0.0.0/8 -j RETURN
    iptables -t mangle -A V2RAY_MASK -d 127.0.0.0/8 -j RETURN
    iptables -t mangle -A V2RAY_MASK -d 169.254.0.0/16 -j RETURN
    iptables -t mangle -A V2RAY_MASK -d 172.16.0.0/12 -j RETURN
    iptables -t mangle -A V2RAY_MASK -d 192.168.0.0/16 -j RETURN
    iptables -t mangle -A V2RAY_MASK -d 224.0.0.0/4 -j RETURN
    iptables -t mangle -A V2RAY_MASK -d 240.0.0.0/4 -j RETURN
    iptables -t mangle -A V2RAY_MASK -d ${OPENVPN_SUBNET} -j RETURN
    iptables -t mangle -A V2RAY_MASK -j MARK --set-mark 1
    
    iptables -t mangle -A OUTPUT -j V2RAY_MASK
    
    # 配置路由
    ip rule add fwmark 1 table 100 2>/dev/null || true
    ip route add local 0.0.0.0/0 dev lo table 100 2>/dev/null || true
    
    log_info "TProxy started successfully on port ${V2RAY_PORT}"
    log_info "OpenVPN subnet ${OPENVPN_SUBNET} will bypass V2Ray"
}

# 停止透明代理
stop_tproxy() {
    log_info "Stopping V2Ray transparent proxy..."
    
    # 删除 iptables 规则
    iptables -t mangle -D PREROUTING -j V2RAY 2>/dev/null || true
    iptables -t mangle -D OUTPUT -j V2RAY_MASK 2>/dev/null || true
    
    # 删除自定义链
    iptables -t mangle -F V2RAY 2>/dev/null || true
    iptables -t mangle -X V2RAY 2>/dev/null || true
    iptables -t mangle -F V2RAY_MASK 2>/dev/null || true
    iptables -t mangle -X V2RAY_MASK 2>/dev/null || true
    
    # 删除路由规则
    ip rule del fwmark 1 table 100 2>/dev/null || true
    ip route del local 0.0.0.0/0 dev lo table 100 2>/dev/null || true
    
    log_info "TProxy stopped successfully"
}

# 显示状态
status_tproxy() {
    echo "=== V2Ray TProxy Status ==="
    echo ""
    echo "Mangle table chains:"
    iptables -t mangle -L V2RAY -n -v 2>/dev/null || echo "V2RAY chain not found"
    echo ""
    iptables -t mangle -L V2RAY_MASK -n -v 2>/dev/null || echo "V2RAY_MASK chain not found"
    echo ""
    echo "IP rules:"
    ip rule show | grep "fwmark 0x1" || echo "No fwmark rule found"
    echo ""
    echo "IP routes (table 100):"
    ip route show table 100 2>/dev/null || echo "Table 100 not found"
}

# 主逻辑
case "$1" in
    start)
        start_tproxy
        ;;
    stop)
        stop_tproxy
        ;;
    restart)
        stop_tproxy
        sleep 1
        start_tproxy
        ;;
    status)
        status_tproxy
        ;;
    *)
        echo "Usage: $0 {start|stop|restart|status} [v2ray_port]"
        echo "Example: $0 start 12345"
        exit 1
        ;;
esac

exit 0
# V2Ray 集成使用指南

## 概述

本项目已集成 V2Ray Core，实现智能分流功能：
- **内网流量** (10.8.0.0/24) → OpenVPN 隧道
- **中国大陆网站** → 直连
- **国外网站** → V2Ray 代理
- **广告/恶意网站** → 自动阻断

## 快速开始

### 1. 安装 V2Ray Core

运行安装脚本：

```bash
cd ~/ovpn-cclient
./scripts/download_v2ray.sh
```

脚本会自动：
- 检测系统架构（x86_64/arm64/armv7）
- 下载最新版 V2Ray Core
- 下载 GeoIP 和 GeoSite 数据
- 安装到 `data/v2ray/` 目录

### 2. 编译项目

```bash
./build.sh
```

### 3. 运行应用

```bash
./build/ovpn-client
```

### 4. 配置 V2Ray

在主界面点击 **"V2Ray 配置"** 按钮，然后：

1. **粘贴节点链接**
   - 支持格式：`ss://`, `vmess://`, `vless://`, `trojan://`
   - 示例：`ss://YWVzLTI1Ni1nY206bllpek93SzZlM0xqT0JiTQ@91.208.104.215:1394/`

2. **启动 V2Ray**
   - 点击 "启动 V2Ray" 按钮
   - 等待状态变为 "运行中"

3. **启用透明代理**（可选）
   - 打开 "启用透明代理" 开关
   - 输入 sudo 密码
   - 所有流量将自动分流

## 支持的协议

### Shadowsocks (SS)

```
ss://YWVzLTI1Ni1nY206cGFzc3dvcmQ@server.com:8388/
```

- 加密方法：aes-256-gcm, chacha20-ietf-poly1305 等
- 自动解析 Base64 编码的配置

### VMess

```
vmess://base64(json_config)
```

JSON 配置示例：
```json
{
  "v": "2",
  "ps": "节点名称",
  "add": "server.com",
  "port": "443",
  "id": "uuid",
  "aid": "0",
  "net": "ws",
  "type": "none",
  "host": "server.com",
  "path": "/path",
  "tls": "tls"
}
```

### VLess

```
vless://uuid@server.com:443?encryption=none&security=tls&type=ws&host=server.com&path=/path#节点名称
```

### Trojan

```
trojan://password@server.com:443?security=tls&type=ws&host=server.com&path=/path#节点名称
```

## 路由规则详解

### 优先级 1: OpenVPN 内网

```
目标: 10.8.0.0/24
动作: 直连（走 OpenVPN 隧道）
说明: 确保访问内网资源时不经过 V2Ray
```

### 优先级 2: 中国大陆 IP

```
目标: geoip:cn
动作: 直连
说明: 国内网站不走代理，提高速度
```

### 优先级 3: 中国大陆域名

```
目标: geosite:cn
动作: 直连
说明: 基于域名的国内网站识别
```

### 优先级 4: 广告域名

```
目标: geosite:category-ads-all
动作: 阻断
说明: 自动拦截广告和跟踪器
```

### 默认规则: 其他流量

```
目标: 所有其他流量
动作: V2Ray 代理
说明: 国外网站走代理
```

## 透明代理配置

### 工作原理

透明代理使用 `iptables` 的 `TPROXY` 模块，将所有 TCP/UDP 流量重定向到 V2Ray。

### 前置要求

1. **安装 pkexec**（用于权限提升）
   ```bash
   sudo apt install policykit-1
   ```

2. **内核支持 TPROXY**
   ```bash
   # 检查内核模块
   lsmod | grep xt_TPROXY
   
   # 如果没有，加载模块
   sudo modprobe xt_TPROXY
   ```

### 手动配置

如果 GUI 配置失败，可以手动运行：

```bash
# 启动透明代理
sudo ./scripts/setup_tproxy.sh start 12345

# 停止透明代理
sudo ./scripts/setup_tproxy.sh stop

# 查看状态
sudo ./scripts/setup_tproxy.sh status
```

### iptables 规则说明

启用透明代理后，会创建以下规则：

```bash
# 创建自定义链
iptables -t mangle -N V2RAY
iptables -t mangle -N V2RAY_MASK

# 跳过内网和保留地址
iptables -t mangle -A V2RAY -d 10.8.0.0/24 -j RETURN  # OpenVPN 内网
iptables -t mangle -A V2RAY -d 192.168.0.0/16 -j RETURN
iptables -t mangle -A V2RAY -d 127.0.0.0/8 -j RETURN

# 其他流量转发到 V2Ray
iptables -t mangle -A V2RAY -p tcp -j TPROXY --on-port 12345 --tproxy-mark 1
iptables -t mangle -A V2RAY -p udp -j TPROXY --on-port 12345 --tproxy-mark 1

# 应用规则
iptables -t mangle -A PREROUTING -j V2RAY
iptables -t mangle -A OUTPUT -j V2RAY_MASK
```

## 故障排查

### 1. V2Ray 无法启动

**症状**：点击启动按钮后状态显示 "错误"

**解决方案**：
```bash
# 检查 V2Ray 二进制是否存在
ls -lh data/v2ray/v2ray

# 检查配置文件
cat ~/.config/ovpn-client/v2ray/config.json

# 手动运行 V2Ray 查看错误
./data/v2ray/v2ray run -c ~/.config/ovpn-client/v2ray/config.json
```

### 2. 节点链接解析失败

**症状**：提示 "无法解析代理链接"

**解决方案**：
- 检查链接格式是否正确
- 确保链接完整（包括协议前缀）
- 尝试使用其他节点

### 3. 透明代理配置失败

**症状**：提示 "配置透明代理失败"

**解决方案**：
```bash
# 检查 pkexec 是否安装
which pkexec

# 检查脚本权限
ls -l scripts/setup_tproxy.sh

# 检查内核模块
lsmod | grep xt_TPROXY

# 手动运行脚本
sudo ./scripts/setup_tproxy.sh start 12345
```

### 4. 国外网站无法访问

**症状**：启用代理后国外网站仍无法访问

**解决方案**：
```bash
# 检查 V2Ray 是否运行
ps aux | grep v2ray

# 检查 iptables 规则
sudo iptables -t mangle -L V2RAY -n -v

# 检查路由规则
ip rule show | grep fwmark

# 查看 V2Ray 日志
tail -f /tmp/v2ray.log
```

### 5. 内网资源无法访问

**症状**：启用透明代理后无法访问 OpenVPN 内网

**解决方案**：
```bash
# 检查 OpenVPN 是否连接
nmcli connection show --active | grep vpn

# 检查路由规则是否正确
ip route show table main | grep 10.8.0.0

# 验证 iptables 规则
sudo iptables -t mangle -L V2RAY -n -v | grep 10.8.0.0
```

## 性能优化

### 1. 减少延迟

在 V2Ray 配置中启用 Mux（多路复用）：

```json
{
  "mux": {
    "enabled": true,
    "concurrency": 8
  }
}
```

### 2. 提高吞吐量

调整 TCP 缓冲区大小：

```bash
# 临时设置
sudo sysctl -w net.core.rmem_max=16777216
sudo sysctl -w net.core.wmem_max=16777216

# 永久设置
echo "net.core.rmem_max=16777216" | sudo tee -a /etc/sysctl.conf
echo "net.core.wmem_max=16777216" | sudo tee -a /etc/sysctl.conf
sudo sysctl -p
```

### 3. DNS 优化

使用 DoH (DNS over HTTPS) 避免 DNS 污染：

```json
{
  "dns": {
    "servers": [
      "https://1.1.1.1/dns-query",
      "https://8.8.8.8/dns-query"
    ]
  }
}
```

## 高级配置

### 自定义路由规则

编辑 `~/.config/ovpn-client/v2ray/config.json`，在 `routing.rules` 中添加规则：

```json
{
  "routing": {
    "rules": [
      {
        "type": "field",
        "domain": ["example.com"],
        "outboundTag": "direct"
      },
      {
        "type": "field",
        "ip": ["1.2.3.4/32"],
        "outboundTag": "proxy"
      }
    ]
  }
}
```

### 多节点负载均衡

配置多个出站代理实现负载均衡：

```json
{
  "outbounds": [
    {
      "tag": "proxy-1",
      "protocol": "shadowsocks",
      "settings": { ... }
    },
    {
      "tag": "proxy-2",
      "protocol": "shadowsocks",
      "settings": { ... }
    }
  ],
  "routing": {
    "balancers": [
      {
        "tag": "balancer",
        "selector": ["proxy-1", "proxy-2"]
      }
    ]
  }
}
```

## 安全建议

1. **定期更新 V2Ray**
   ```bash
   ./scripts/download_v2ray.sh
   ```

2. **使用强加密**
   - Shadowsocks: aes-256-gcm, chacha20-ietf-poly1305
   - VMess: aes-128-gcm, chacha20-poly1305

3. **启用 TLS**
   - 确保节点支持 TLS/XTLS
   - 验证证书有效性

4. **防止 DNS 泄漏**
   - 使用 V2Ray 内置 DNS
   - 启用 DoH/DoT

5. **定期更换节点**
   - 避免长期使用同一节点
   - 使用订阅功能自动更新

## 卸载

如果需要移除 V2Ray 集成：

```bash
# 停止 V2Ray
# 在 GUI 中点击 "停止 V2Ray"

# 关闭透明代理
sudo ./scripts/setup_tproxy.sh stop

# 删除 V2Ray 文件
rm -rf data/v2ray
rm -rf ~/.config/ovpn-client/v2ray

# 删除相关代码（可选）
# 移除 src/v2ray_*.c 和 include/v2ray_*.h
```

## 常见问题

### Q: V2Ray 和 OpenVPN 会冲突吗？

A: 不会。路由规则确保内网流量 (10.8.0.0/24) 优先走 OpenVPN，不会经过 V2Ray。

### Q: 透明代理需要一直 root 权限吗？

A: 只在启用/禁用透明代理时需要 root 权限（通过 pkexec 提示输入密码）。V2Ray 本身以普通用户权限运行。

### Q: 支持订阅链接吗？

A: 当前版本支持单个节点链接。订阅功能计划在未来版本中添加。

### Q: 如何验证分流是否生效？

A: 
```bash
# 访问国内网站，应该直连
curl -I https://www.baidu.com

# 访问国外网站，应该走代理
curl -I https://www.google.com

# 访问内网资源，应该走 OpenVPN
curl http://10.8.0.1
```

### Q: V2Ray 占用多少系统资源？

A: 
- 内存：约 50-100 MB
- CPU：空闲时 < 1%，高负载时 5-15%
- 磁盘：约 30 MB（包括 GeoIP/GeoSite 数据）

## 技术支持

如遇到问题，请：

1. 查看运行日志（GUI 中的 "运行日志" 标签页）
2. 检查 `/tmp/v2ray.log`
3. 运行 `sudo ./scripts/setup_tproxy.sh status` 查看透明代理状态
4. 提交 Issue 时附上详细的错误信息和系统环境

## 参考资料

- [V2Ray 官方文档](https://www.v2fly.org/)
- [V2Ray 配置指南](https://guide.v2fly.org/)
- [GeoIP 数据源](https://github.com/v2fly/geoip)
- [GeoSite 数据源](https://github.com/v2fly/domain-list-community)
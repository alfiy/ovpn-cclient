#ifndef PROXY_PARSER_H
#define PROXY_PARSER_H

#include <glib.h>

// 代理协议类型
typedef enum {
    PROXY_TYPE_SHADOWSOCKS,
    PROXY_TYPE_VMESS,
    PROXY_TYPE_VLESS,
    PROXY_TYPE_TROJAN,
    PROXY_TYPE_UNKNOWN
} ProxyType;

// Shadowsocks 配置
typedef struct {
    char *server;
    int port;
    char *method;
    char *password;
    char *plugin;
    char *plugin_opts;
} SSConfig;

// VMess 配置
typedef struct {
    char *address;
    int port;
    char *id;
    int alter_id;
    char *security;
    char *network;
    char *type;
    char *host;
    char *path;
    char *tls;
    char *sni;
} VMessConfig;

// VLess 配置
typedef struct {
    char *address;
    int port;
    char *id;
    char *encryption;
    char *flow;
    char *network;
    char *type;
    char *host;
    char *path;
    char *security;
    char *sni;
} VLessConfig;

// Trojan 配置
typedef struct {
    char *address;
    int port;
    char *password;
    char *sni;
    char *network;
    char *type;
    char *host;
    char *path;
} TrojanConfig;

// 通用代理配置
typedef struct {
    ProxyType type;
    char *name;
    union {
        SSConfig ss;
        VMessConfig vmess;
        VLessConfig vless;
        TrojanConfig trojan;
    } config;
} ProxyConfig;

/**
 * 解析代理链接
 * @param url 代理链接 (ss://, vmess://, vless://, trojan://)
 * @return ProxyConfig* 成功返回配置，失败返回 NULL
 */
ProxyConfig* proxy_parser_parse(const char *url);

/**
 * 释放代理配置
 * @param config 要释放的配置
 */
void proxy_parser_free(ProxyConfig *config);

/**
 * 获取代理类型字符串
 * @param type 代理类型
 * @return 类型字符串
 */
const char* proxy_parser_get_type_string(ProxyType type);

/**
 * 生成 V2Ray 配置 JSON
 * @param config 代理配置
 * @param local_port 本地监听端口
 * @return JSON 字符串，需要用 g_free 释放
 */
char* proxy_parser_generate_v2ray_config(ProxyConfig *config, int local_port);

#endif // PROXY_PARSER_H
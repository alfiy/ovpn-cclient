
#include "../include/proxy_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <json-c/json.h>

// Base64 解码（使用 GLib）
static gchar* base64_decode_str(const char *encoded) {
    gsize out_len;
    guchar *decoded = g_base64_decode(encoded, &out_len);
    if (!decoded) return NULL;
    
    gchar *result = g_strndup((const gchar*)decoded, out_len);
    g_free(decoded);
    return result;
}

// 解析 Shadowsocks 链接
static ProxyConfig* parse_shadowsocks(const char *url) {
    // ss://base64(method:password)@server:port
    if (strncmp(url, "ss://", 5) != 0) return NULL;
    
    ProxyConfig *config = g_new0(ProxyConfig, 1);
    config->type = PROXY_TYPE_SHADOWSOCKS;
    config->name = g_strdup("Shadowsocks");
    
    const char *data = url + 5;
    char *at_pos = strchr(data, '@');
    
    if (at_pos) {
        // 新格式: ss://base64@server:port
        gchar *decoded = base64_decode_str(data);
        if (!decoded) {
            g_free(config);
            return NULL;
        }
        
        // 解析 method:password
        char *colon = strchr(decoded, ':');
        if (colon) {
            *colon = '\0';
            config->config.ss.method = g_strdup(decoded);
            config->config.ss.password = g_strdup(colon + 1);
        }
        g_free(decoded);
        
        // 解析 server:port
        at_pos++; // 跳过 @
        char *slash = strchr(at_pos, '/');
        if (slash) *slash = '\0';
        
        char *port_pos = strrchr(at_pos, ':');
        if (port_pos) {
            *port_pos = '\0';
            config->config.ss.server = g_strdup(at_pos);
            config->config.ss.port = atoi(port_pos + 1);
        }
    } else {
        // 旧格式: ss://base64(method:password@server:port)
        gchar *decoded = base64_decode_str(data);
        if (!decoded) {
            g_free(config);
            return NULL;
        }
        
        // 解析完整字符串
        char *at = strchr(decoded, '@');
        if (at) {
            *at = '\0';
            char *colon1 = strchr(decoded, ':');
            if (colon1) {
                *colon1 = '\0';
                config->config.ss.method = g_strdup(decoded);
                config->config.ss.password = g_strdup(colon1 + 1);
            }
            
            char *server_port = at + 1;
            char *slash = strchr(server_port, '/');
            if (slash) *slash = '\0';
            
            char *colon2 = strrchr(server_port, ':');
            if (colon2) {
                *colon2 = '\0';
                config->config.ss.server = g_strdup(server_port);
                config->config.ss.port = atoi(colon2 + 1);
            }
        }
        g_free(decoded);
    }
    
    return config;
}

// 解析 VMess 链接
static ProxyConfig* parse_vmess(const char *url) {
    // vmess://base64(json)
    if (strncmp(url, "vmess://", 8) != 0) return NULL;
    
    gchar *decoded = base64_decode_str(url + 8);
    if (!decoded) return NULL;
    
    ProxyConfig *config = g_new0(ProxyConfig, 1);
    config->type = PROXY_TYPE_VMESS;
    
    // 解析 JSON (使用 json-c)
    struct json_object *parsed_json = json_tokener_parse(decoded);
    if (parsed_json) {
        struct json_object *tmp;
        
        if (json_object_object_get_ex(parsed_json, "ps", &tmp)) {
            config->name = g_strdup(json_object_get_string(tmp));
        }
        if (json_object_object_get_ex(parsed_json, "add", &tmp)) {
            config->config.vmess.address = g_strdup(json_object_get_string(tmp));
        }
        if (json_object_object_get_ex(parsed_json, "port", &tmp)) {
            config->config.vmess.port = json_object_get_int(tmp);
        }
        if (json_object_object_get_ex(parsed_json, "id", &tmp)) {
            config->config.vmess.id = g_strdup(json_object_get_string(tmp));
        }
        if (json_object_object_get_ex(parsed_json, "aid", &tmp)) {
            config->config.vmess.alter_id = json_object_get_int(tmp);
        }
        if (json_object_object_get_ex(parsed_json, "net", &tmp)) {
            config->config.vmess.network = g_strdup(json_object_get_string(tmp));
        }
        if (json_object_object_get_ex(parsed_json, "type", &tmp)) {
            config->config.vmess.type = g_strdup(json_object_get_string(tmp));
        }
        if (json_object_object_get_ex(parsed_json, "host", &tmp)) {
            config->config.vmess.host = g_strdup(json_object_get_string(tmp));
        }
        if (json_object_object_get_ex(parsed_json, "path", &tmp)) {
            config->config.vmess.path = g_strdup(json_object_get_string(tmp));
        }
        if (json_object_object_get_ex(parsed_json, "tls", &tmp)) {
            config->config.vmess.tls = g_strdup(json_object_get_string(tmp));
        }
        if (json_object_object_get_ex(parsed_json, "sni", &tmp)) {
            config->config.vmess.sni = g_strdup(json_object_get_string(tmp));
        }
        
        json_object_put(parsed_json);
    }
    
    g_free(decoded);
    return config;
}

// 解析 VLess 链接
static ProxyConfig* parse_vless(const char *url) {
    // vless://uuid@server:port?params#name
    if (strncmp(url, "vless://", 8) != 0) return NULL;
    
    ProxyConfig *config = g_new0(ProxyConfig, 1);
    config->type = PROXY_TYPE_VLESS;
    
    const char *data = url + 8;
    
    // 解析名称
    char *hash = strchr(data, '#');
    if (hash) {
        config->name = g_uri_unescape_string(hash + 1, NULL);
    } else {
        config->name = g_strdup("VLess");
    }
    
    // 解析 UUID@server:port
    char *at = strchr(data, '@');
    if (at) {
        config->config.vless.id = g_strndup(data, at - data);
        
        char *question = strchr(at, '?');
        char *server_port = at + 1;
        if (question) {
            char *colon = strchr(server_port, ':');
            if (colon && colon < question) {
                config->config.vless.address = g_strndup(server_port, colon - server_port);
                config->config.vless.port = atoi(colon + 1);
            }
            
            // 解析参数
            GHashTable *params = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
            char *params_str = g_strndup(question + 1, hash ? hash - question - 1 : strlen(question + 1));
            
            char **pairs = g_strsplit(params_str, "&", -1);
            for (int i = 0; pairs[i]; i++) {
                char **kv = g_strsplit(pairs[i], "=", 2);
                if (kv[0] && kv[1]) {
                    g_hash_table_insert(params, g_strdup(kv[0]), g_uri_unescape_string(kv[1], NULL));
                }
                g_strfreev(kv);
            }
            g_strfreev(pairs);
            g_free(params_str);
            
            config->config.vless.encryption = g_strdup(g_hash_table_lookup(params, "encryption"));
            config->config.vless.flow = g_strdup(g_hash_table_lookup(params, "flow"));
            config->config.vless.network = g_strdup(g_hash_table_lookup(params, "type"));
            config->config.vless.security = g_strdup(g_hash_table_lookup(params, "security"));
            config->config.vless.sni = g_strdup(g_hash_table_lookup(params, "sni"));
            
            g_hash_table_destroy(params);
        }
    }
    
    return config;
}

// 主解析函数
ProxyConfig* proxy_parser_parse(const char *url) {
    if (!url) return NULL;
    
    if (strncmp(url, "ss://", 5) == 0) {
        return parse_shadowsocks(url);
    } else if (strncmp(url, "vmess://", 8) == 0) {
        return parse_vmess(url);
    } else if (strncmp(url, "vless://", 8) == 0) {
        return parse_vless(url);
    }
    
    return NULL;
}

// 释放配置
void proxy_parser_free(ProxyConfig *config) {
    if (!config) return;
    
    g_free(config->name);
    
    switch (config->type) {
        case PROXY_TYPE_SHADOWSOCKS:
            g_free(config->config.ss.server);
            g_free(config->config.ss.method);
            g_free(config->config.ss.password);
            g_free(config->config.ss.plugin);
            g_free(config->config.ss.plugin_opts);
            break;
        case PROXY_TYPE_VMESS:
            g_free(config->config.vmess.address);
            g_free(config->config.vmess.id);
            g_free(config->config.vmess.security);
            g_free(config->config.vmess.network);
            g_free(config->config.vmess.type);
            g_free(config->config.vmess.host);
            g_free(config->config.vmess.path);
            g_free(config->config.vmess.tls);
            g_free(config->config.vmess.sni);
            break;
        case PROXY_TYPE_VLESS:
            g_free(config->config.vless.address);
            g_free(config->config.vless.id);
            g_free(config->config.vless.encryption);
            g_free(config->config.vless.flow);
            g_free(config->config.vless.network);
            g_free(config->config.vless.type);
            g_free(config->config.vless.host);
            g_free(config->config.vless.path);
            g_free(config->config.vless.security);
            g_free(config->config.vless.sni);
            break;
        default:
            break;
    }
    
    g_free(config);
}

// 获取类型字符串
const char* proxy_parser_get_type_string(ProxyType type) {
    switch (type) {
        case PROXY_TYPE_SHADOWSOCKS: return "Shadowsocks";
        case PROXY_TYPE_VMESS: return "VMess";
        case PROXY_TYPE_VLESS: return "VLess";
        case PROXY_TYPE_TROJAN: return "Trojan";
        default: return "Unknown";
    }
}

// 生成 V2Ray 配置
char* proxy_parser_generate_v2ray_config(ProxyConfig *config, int local_port) {
    if (!config) return NULL;
    
    struct json_object *root = json_object_new_object();
    
    // Log 配置
    struct json_object *log_obj = json_object_new_object();
    json_object_object_add(log_obj, "loglevel", json_object_new_string("warning"));
    json_object_object_add(root, "log", log_obj);
    
    // Inbounds - 透明代理
    struct json_object *inbounds = json_object_new_array();
    struct json_object *inbound = json_object_new_object();
    json_object_object_add(inbound, "tag", json_object_new_string("transparent"));
    json_object_object_add(inbound, "port", json_object_new_int(local_port));
    json_object_object_add(inbound, "protocol", json_object_new_string("dokodemo-door"));
    
    struct json_object *inbound_settings = json_object_new_object();
    json_object_object_add(inbound_settings, "network", json_object_new_string("tcp,udp"));
    json_object_object_add(inbound_settings, "followRedirect", json_object_new_boolean(1));
    json_object_object_add(inbound, "settings", inbound_settings);
    
    struct json_object *sniffing = json_object_new_object();
    json_object_object_add(sniffing, "enabled", json_object_new_boolean(1));
    struct json_object *dest_override = json_object_new_array();
    json_object_array_add(dest_override, json_object_new_string("http"));
    json_object_array_add(dest_override, json_object_new_string("tls"));
    json_object_object_add(sniffing, "destOverride", dest_override);
    json_object_object_add(inbound, "sniffing", sniffing);
    
    json_object_array_add(inbounds, inbound);
    json_object_object_add(root, "inbounds", inbounds);
    
    // Outbounds
    struct json_object *outbounds = json_object_new_array();
    
    // 主代理出站
    struct json_object *proxy_outbound = json_object_new_object();
    json_object_object_add(proxy_outbound, "tag", json_object_new_string("proxy"));
    
    if (config->type == PROXY_TYPE_SHADOWSOCKS) {
        json_object_object_add(proxy_outbound, "protocol", json_object_new_string("shadowsocks"));
        struct json_object *settings = json_object_new_object();
        struct json_object *servers = json_object_new_array();
        struct json_object *server = json_object_new_object();
        json_object_object_add(server, "address", json_object_new_string(config->config.ss.server));
        json_object_object_add(server, "port", json_object_new_int(config->config.ss.port));
        json_object_object_add(server, "method", json_object_new_string(config->config.ss.method));
        json_object_object_add(server, "password", json_object_new_string(config->config.ss.password));
        json_object_array_add(servers, server);
        json_object_object_add(settings, "servers", servers);
        json_object_object_add(proxy_outbound, "settings", settings);
    }
    
    json_object_array_add(outbounds, proxy_outbound);
    
    // 直连出站
    struct json_object *direct_outbound = json_object_new_object();
    json_object_object_add(direct_outbound, "tag", json_object_new_string("direct"));
    json_object_object_add(direct_outbound, "protocol", json_object_new_string("freedom"));
    json_object_array_add(outbounds, direct_outbound);
    
    // 阻断出站
    struct json_object *block_outbound = json_object_new_object();
    json_object_object_add(block_outbound, "tag", json_object_new_string("block"));
    json_object_object_add(block_outbound, "protocol", json_object_new_string("blackhole"));
    json_object_array_add(outbounds, block_outbound);
    
    json_object_object_add(root, "outbounds", outbounds);
    
    // 路由规则
    struct json_object *routing = json_object_new_object();
    json_object_object_add(routing, "domainStrategy", json_object_new_string("IPIfNonMatch"));
    struct json_object *rules = json_object_new_array();
    
    // 规则1: 内网 IP 直连
    struct json_object *rule1 = json_object_new_object();
    json_object_object_add(rule1, "type", json_object_new_string("field"));
    struct json_object *ip1 = json_object_new_array();
    json_object_array_add(ip1, json_object_new_string("10.8.0.0/24"));
    json_object_object_add(rule1, "ip", ip1);
    json_object_object_add(rule1, "outboundTag", json_object_new_string("direct"));
    json_object_array_add(rules, rule1);
    
    // 规则2: 中国大陆 IP 直连
    struct json_object *rule2 = json_object_new_object();
    json_object_object_add(rule2, "type", json_object_new_string("field"));
    struct json_object *ip2 = json_object_new_array();
    json_object_array_add(ip2, json_object_new_string("geoip:cn"));
    json_object_object_add(rule2, "ip", ip2);
    json_object_object_add(rule2, "outboundTag", json_object_new_string("direct"));
    json_object_array_add(rules, rule2);
    
    // 规则3: 中国大陆域名直连
    struct json_object *rule3 = json_object_new_object();
    json_object_object_add(rule3, "type", json_object_new_string("field"));
    struct json_object *domain3 = json_object_new_array();
    json_object_array_add(domain3, json_object_new_string("geosite:cn"));
    json_object_object_add(rule3, "domain", domain3);
    json_object_object_add(rule3, "outboundTag", json_object_new_string("direct"));
    json_object_array_add(rules, rule3);
    
    // 规则4: 广告域名阻断
    struct json_object *rule4 = json_object_new_object();
    json_object_object_add(rule4, "type", json_object_new_string("field"));
    struct json_object *domain4 = json_object_new_array();
    json_object_array_add(domain4, json_object_new_string("geosite:category-ads-all"));
    json_object_object_add(rule4, "domain", domain4);
    json_object_object_add(rule4, "outboundTag", json_object_new_string("block"));
    json_object_array_add(rules, rule4);
    
    json_object_object_add(routing, "rules", rules);
    json_object_object_add(root, "routing", routing);
    
    // 生成 JSON 字符串
    const char *json_str = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PRETTY);
    char *result = g_strdup(json_str);
    
    json_object_put(root);
    
    return result;
}
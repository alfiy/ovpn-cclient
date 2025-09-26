#include "../include/structs.h"
#include "../include/parser.h"
#include "../include/log_util.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>


// 解析OVPN文件

// read_inline_block 函数实现
char* read_inline_block(FILE *file, const char *end_tag) {
    char line[2048];
    GString *content = g_string_new("");
    
    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\r\n")] = 0;
        
        if (strcmp(line, end_tag) == 0) {
            break;
        }
        
        g_string_append(content, line);
        g_string_append(content, "\n");
    }
    
    char *result = g_strdup(content->str);
    g_string_free(content, TRUE);
    return result;
}

// 证书文件写入函数
gboolean write_cert_file(const char *content, const char *filepath) {
    FILE *file;
    if (!content || strlen(content) == 0) {
        return FALSE;
    }
    
    char *dir = g_path_get_dirname(filepath);
    if (g_mkdir_with_parents(dir, 0700) != 0) {
        g_free(dir);
        log_message("ERROR", "Failed to create directory for: %s", filepath);
        return FALSE;
    }
    g_free(dir);
    
    file = fopen(filepath, "w");
    if (!file) {
        log_message("ERROR", "Failed to create cert file: %s", filepath);
        return FALSE;
    }
    
    fprintf(file, "%s", content);
    fclose(file);
    chmod(filepath, 0600);
    
    log_message("INFO", "Created cert file: %s", filepath);
    return TRUE;
}

// 生成证书路径函数
char* generate_cert_path(const char *config_name, const char *cert_type) {
    const char *user = g_get_user_name();
    return g_strdup_printf("/home/%s/.cert/nm-openvpn/%s-%s.pem", 
                          user, config_name, cert_type);
}

// ovpn_file 解析函数
OVPNConfig* parse_ovpn_file(const char *filepath) {
    FILE *file;
    char line[2048];
    OVPNConfig *config;
    
    log_message("INFO", "Parsing OVPN file: %s", filepath);
    
    file = fopen(filepath, "r");
    if (!file) {
        log_message("ERROR", "Failed to open file: %s", filepath);
        return NULL;
    }
    
    config = g_malloc0(sizeof(OVPNConfig));
    
    // 初始化默认值
    strcpy(config->proto, "udp");
    strcpy(config->port, "1194");
    strcpy(config->dev, "tun");
    
    // 初始化关键的新字段默认值
    strcpy(config->remote_cert_tls, "server");
    strcpy(config->tls_version_min, "1.2");
    config->cert_pass_flags_zero = TRUE;
    
    // 从文件路径提取配置名称
    char *basename = g_path_get_basename(filepath);
    char *config_name = g_strndup(basename, strlen(basename) - 5);
    g_free(basename);
    
    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\r\n")] = 0;
        
        if (line[0] == '#' || line[0] == '\0') continue;
        
        // remote 解析
        if (strncmp(line, "remote ", 7) == 0 && config->remote_count < MAX_REMOTES) {
            char server[256], port[16] = "1194", proto[16] = "udp";
            int n = sscanf(line + 7, "%255s %15s %15s", server, port, proto);
            
            strcpy(config->remote[config->remote_count].server, server);
            if (n >= 2) strcpy(config->remote[config->remote_count].port, port);
            else strcpy(config->remote[config->remote_count].port, "1194");
            if (n == 3) strcpy(config->remote[config->remote_count].proto, proto);
            else strcpy(config->remote[config->remote_count].proto, "udp");
            
            config->remote_count++;
            continue;
        }
        
        // 基本配置解析
        if (strncmp(line, "proto ", 6) == 0) {
            sscanf(line + 6, "%15s", config->proto);
        } else if (strncmp(line, "port ", 5) == 0) {
            sscanf(line + 5, "%15s", config->port);
        } else if (strncmp(line, "dev ", 4) == 0) {
            sscanf(line + 4, "%15s", config->dev);
        }
        // 证书文件路径
        else if (strncmp(line, "ca ", 3) == 0) {
            sscanf(line + 3, "%1023s", config->ca);
        } else if (strncmp(line, "cert ", 5) == 0) {
            sscanf(line + 5, "%1023s", config->cert);
        } else if (strncmp(line, "key ", 4) == 0) {
            sscanf(line + 4, "%1023s", config->key);
        } else if (strncmp(line, "tls-auth ", 9) == 0) {
            char direction[8] = "";
            sscanf(line + 9, "%1023s %7s", config->tls_auth, direction);
            if (strlen(direction) > 0) {
                strcpy(config->key_direction, direction);
            }
        } else if (strncmp(line, "tls-crypt ", 10) == 0) {
            sscanf(line + 10, "%1023s", config->tls_crypt);
        }
        // 关键新增：解析缺失的配置项
        else if (strncmp(line, "remote-cert-tls ", 16) == 0) {
            sscanf(line + 16, "%63s", config->remote_cert_tls);
        } else if (strncmp(line, "tls-version-min ", 16) == 0) {
            sscanf(line + 16, "%15s", config->tls_version_min);
        }
        // inline 证书块解析
        else if (strcmp(line, "<ca>") == 0) {
            config->ca_inline = read_inline_block(file, "</ca>");
        } else if (strcmp(line, "<cert>") == 0) {
            config->cert_inline = read_inline_block(file, "</cert>");
        } else if (strcmp(line, "<key>") == 0) {
            config->key_inline = read_inline_block(file, "</key>");
        } else if (strcmp(line, "<tls-crypt>") == 0) {
            config->tls_crypt_inline = read_inline_block(file, "</tls-crypt>");
        } else if (strcmp(line, "<tls-auth>") == 0) {
            char *temp_content = read_inline_block(file, "</tls-auth>");
            if (temp_content) {
                char *tls_auth_path = generate_cert_path(config_name, "tls-auth");
                if (write_cert_file(temp_content, tls_auth_path)) {
                    strcpy(config->tls_auth, tls_auth_path);
                }
                g_free(tls_auth_path);
                g_free(temp_content);
            }
        }
        // 其他配置项
        else if (strncmp(line, "auth-user-pass", 14) == 0) {
            config->auth_user_pass = TRUE;
        } else if (strncmp(line, "cipher ", 7) == 0) {
            sscanf(line + 7, "%63s", config->cipher);
        } else if (strncmp(line, "auth ", 5) == 0) {
            sscanf(line + 5, "%63s", config->auth);
        } else if (strncmp(line, "comp-lzo", 8) == 0) {
            config->comp_lzo = TRUE;
        } else if (strncmp(line, "redirect-gateway", 16) == 0) {
            config->redirect_gateway = TRUE;
        }
        
        // 保存原始配置
        if (strlen(config->raw_config) + strlen(line) + 2 < sizeof(config->raw_config)) {
            strcat(config->raw_config, line);
            strcat(config->raw_config, "\n");
        }
    }
    
    fclose(file);
    
    // 处理 inline 证书
    if (config->ca_inline) {
        char *ca_path = generate_cert_path(config_name, "ca");
        if (write_cert_file(config->ca_inline, ca_path)) {
            strcpy(config->ca, ca_path);
        }
        g_free(ca_path);
    }
    
    if (config->cert_inline) {
        char *cert_path = generate_cert_path(config_name, "cert");
        if (write_cert_file(config->cert_inline, cert_path)) {
            strcpy(config->cert, cert_path);
        }
        g_free(cert_path);
    }
    
    if (config->key_inline) {
        char *key_path = generate_cert_path(config_name, "key");
        if (write_cert_file(config->key_inline, key_path)) {
            strcpy(config->key, key_path);
        }
        g_free(key_path);
    }
    
    if (config->tls_crypt_inline) {
        char *tls_crypt_path = generate_cert_path(config_name, "tls-crypt");
        if (write_cert_file(config->tls_crypt_inline, tls_crypt_path)) {
            strcpy(config->tls_crypt, tls_crypt_path);
        }
        g_free(tls_crypt_path);
    }
    
    // 确保关键默认值
    if (strlen(config->remote_cert_tls) == 0) {
        strcpy(config->remote_cert_tls, "server");
    }
    
    if (strlen(config->tls_version_min) == 0) {
        strcpy(config->tls_version_min, "1.2");
    }
    
    g_free(config_name);
    
    log_message("INFO", "End Parsing OVPN file: %s", filepath);
    return config;
}

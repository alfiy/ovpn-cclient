#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "../include/core_logic.h"
#include "../include/log_util.h"
#include "../include/structs.h"
#include "../include/ui_callbacks.h"




// 验证证书文件
gboolean validate_certificates(OVPNConfig *config) {
    gboolean valid = TRUE;

    // CA
    if (strlen(config->ca) == 0 && !config->ca_inline) {
        log_message("ERROR", "CA certificate is required");
        valid = FALSE;
    } else if (strlen(config->ca) > 0 && access(config->ca, R_OK) != 0) {
        log_message("ERROR", "CA certificate file not found: %s", config->ca);
        valid = FALSE;
    }

    // client cert/key
    if (!config->auth_user_pass) {
        if (strlen(config->cert) == 0 && !config->cert_inline) {
            log_message("ERROR", "Client certificate is required");
            valid = FALSE;
        } else if (strlen(config->cert) > 0 && access(config->cert, R_OK) != 0) {
            log_message("ERROR", "Client certificate file not found: %s", config->cert);
            valid = FALSE;
        }

        if (strlen(config->key) == 0 && !config->key_inline) {
            log_message("ERROR", "Private key is required");
            valid = FALSE;
        } else if (strlen(config->key) > 0 && access(config->key, R_OK) != 0) {
            log_message("ERROR", "Private key file not found: %s", config->key);
            valid = FALSE;
        }
    }

    // TLS auth/crypt
    if (strlen(config->tls_auth) > 0 && access(config->tls_auth, R_OK) != 0) {
        log_message("ERROR", "TLS auth key file not found: %s", config->tls_auth);
        valid = FALSE;
    }
    if (strlen(config->tls_crypt) == 0 && !config->tls_crypt_inline) {
        // not required
    } else if (strlen(config->tls_crypt) > 0 && access(config->tls_crypt, R_OK) != 0) {
        log_message("ERROR", "TLS crypt key file not found: %s", config->tls_crypt);
        valid = FALSE;
    }

    return valid;
}


// 执行底层网络连接测试
gboolean test_connection(const char *server, int port, const char *proto) {
    int sock;
    struct sockaddr_in server_addr;
    gboolean success = FALSE;

    log_message("INFO", "Testing connection to %s:%d (%s)", server, port, proto);

    // 创建套接字
    if (strcmp(proto, "tcp") == 0) {
        sock = socket(AF_INET, SOCK_STREAM, 0);
    } else {
        sock = socket(AF_INET, SOCK_DGRAM, 0);
    }

    if (sock < 0) {
        log_message("ERROR", "Failed to create socket");
        return FALSE;
    }

    // 设置服务器地址信息
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, server, &server_addr.sin_addr);

    if (strcmp(proto, "tcp") == 0) {
        // TCP 测试
        if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == 0) {
            log_message("INFO", "TCP connection successful");
            success = TRUE;
        } else {
            log_message("ERROR", "TCP connection failed");
        }
    } else {
        // UDP 测试（简单发送数据包）
        char test_data[] = "\x38\x01\x00\x00\x00\x00\x00\x00\x00";
        if (sendto(sock, test_data, sizeof(test_data), 0,
                   (struct sockaddr*)&server_addr, sizeof(server_addr)) > 0) {
            log_message("INFO", "UDP connection test sent successfully");
            success = TRUE;
        } else {
            log_message("ERROR", "UDP connection test failed");
        }
    }

    close(sock);
    return success;
}
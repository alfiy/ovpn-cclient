#ifndef PARSER_H
#define PARSER_H
#include <stdio.h>
#include <glib-2.0/glib.h>
#include "../include/structs.h"

char* read_inline_block(FILE *file, const char *end_tag);
gboolean write_cert_file(const char *content, const char *filepath);
char* generate_cert_path(const char *config_name, const char *cert_type);
OVPNConfig* parse_ovpn_file(const char *filename);

#endif


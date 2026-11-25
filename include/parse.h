/* parse.h - CLI command parser (OWNER: Shubham) */
#ifndef PARSE_H
#define PARSE_H
#include <stddef.h>
#include "protocol.h"

typedef enum { P_ERROR = -1, P_EMPTY = 0, P_SERVER = 1, P_HELP = 2, P_EXIT = 3 } parse_result_t;

#define PARSE_MAX_TEXT 4096

typedef struct {
    cmd_t    cmd;
    char     path[PROTO_MAX_PATH];
    int      fd;
    int      mode;                    /* open_mode_t */
    uint32_t offset;
    uint32_t length;                  /* bytes to read */
    char     text[PARSE_MAX_TEXT];    /* payload for write (client sends it in 512-byte chunks) */
    size_t   text_len;
} parsed_cmd_t;

/* P_SERVER: *out is filled, send it to the server. P_ERROR: err has a message. */
parse_result_t parse_command(const char *line, parsed_cmd_t *out, char *err, size_t errlen);
void print_help(void);

#endif

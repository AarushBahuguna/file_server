#ifndef SERVER_H
#define SERVER_H
#include "common.h"
#include "protocol.h"
#define MAX_WORKERS 4
int server_start(void);
int tcp_server_main(void);
void server_stop(void);
#endif

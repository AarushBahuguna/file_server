/* ipc.h - FIFO helpers (OWNER: Shubham) */
#ifndef IPC_H
#define IPC_H
#include "protocol.h"

/* ---- server side ---- */
int ipc_server_init(void);                       /* creates+opens server FIFO, returns fd or -1 */
int ipc_recv_request(int server_fd, request_t *req);   /* blocking; 1 = ok, 0 = EOF, -1 = error */
int ipc_send_response(pid_t client_pid, const response_t *resp); /* 0 ok, -1 error */
void ipc_server_close(int server_fd);

/* ---- client side ---- */
int ipc_client_init(void);                       /* creates own reply FIFO; returns 0 or -1 */
int ipc_call(request_t *req, response_t *resp);  /* send + wait for reply; 0 ok, -1 error */
void ipc_client_close(void);

#endif

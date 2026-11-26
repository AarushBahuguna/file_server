/* ipc.h - FIFO and TCP IPC helpers (OWNER: Shubham) */
#ifndef IPC_H
#define IPC_H
#include "protocol.h"

#define TCP_DEFAULT_PORT 9000

/* ---- server side ---- */
int ipc_server_init(void);                       /* creates+opens server FIFO, returns fd or -1 */
int ipc_recv_request(int server_fd, request_t *req);   /* blocking; 1 = ok, 0 = EOF, -1 = error */
int ipc_send_response(pid_t client_pid, const response_t *resp); /* 0 ok, -1 error */
void ipc_server_close(int server_fd);

/* ---- client side ---- */
int ipc_client_init(void);                       /* creates own reply FIFO; returns 0 or -1 */
void ipc_set_timeout(int ms);                    /* how long ipc_call waits for a reply; <=0 = forever (default 30000) */
/* send + wait for the reply with the same req_id (stale replies are skipped).
 * 0 ok, -1 error with errno: ETIMEDOUT (no reply), EINTR (signal), ENOENT/ENXIO (server not running) */
int ipc_call(request_t *req, response_t *resp);
void ipc_client_close(void);

/* TCP transport: for clients running on another laptop. */
int tcp_server_init(int port);
int tcp_server_accept(int listen_fd);
int tcp_recv_request(int client_fd, request_t *req);
int tcp_send_response(int client_fd, const response_t *resp);
void tcp_server_close(int listen_fd);

int tcp_client_init(const char *host, int port);
int tcp_client_call(request_t *req, response_t *resp);
void tcp_client_close(void);

#endif

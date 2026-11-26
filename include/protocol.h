/* protocol.h - client <-> server message format (OWNER: Shubham)
 * FROZEN on Day 1. Change only with agreement from the whole team.
 *
 * IPC design:
 *   - one well-known server FIFO (SERVER_FIFO), all clients write requests to it
 *   - one reply FIFO per client (CLIENT_FIFO_FMT with the client's pid)
 *   - every message is a fixed-size struct <= PIPE_BUF (4096) so a single
 *     write() is atomic and requests from different clients never interleave.
 */
#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <limits.h>
#include <sys/types.h>

#define SERVER_FIFO      "/tmp/fs_server_fifo"
#define CLIENT_FIFO_FMT  "/tmp/fs_client_%d"
#define CLIENT_FIFO_MAX  64

#define PROTO_MAX_USER   32
#define PROTO_MAX_PATH   128
#define PROTO_MAX_REQ_DATA   512    /* payload for write */
#define PROTO_MAX_RESP_DATA  3072   /* payload for read / ls / stats */

typedef enum {
    CMD_NONE = 0,
    CMD_LOGIN,      /* register user session            */
    CMD_LOGOUT,
    CMD_MKDIR,      /* path                             */
    CMD_CD,         /* path                             */
    CMD_LS,         /* path (empty = current dir)       */
    CMD_CREATE,     /* path                             */
    CMD_OPEN,       /* path, mode (OPEN_READ/OPEN_WRITE) -> returns fd */
    CMD_CLOSE,      /* fd                               */
    CMD_READ,       /* fd, offset, length               */
    CMD_WRITE,      /* fd, offset, length, data         */
    CMD_DELETE,     /* path                             */
    CMD_STATS,      /* fragmentation, free space, cache, scheduler */
    CMD_COUNT
} cmd_t;

typedef enum { OPEN_READ = 0, OPEN_WRITE = 1 } open_mode_t;

typedef enum {
    ST_OK = 0,
    ST_ERR_NOENT,       /* no such file or directory */
    ST_ERR_EXIST,       /* already exists            */
    ST_ERR_NOSPC,       /* disk full                 */
    ST_ERR_BADFD,
    ST_ERR_PERM,
    ST_ERR_INVAL,       /* bad arguments             */
    ST_ERR_DEADLOCK,    /* request aborted: deadlock victim */
    ST_ERR_BUSY,
    ST_ERR_INTERNAL
} status_t;

typedef struct {
    uint32_t req_id;                    /* client-chosen, echoed in response */
    pid_t    client_pid;                /* used to find reply FIFO           */
    int32_t  cmd;                       /* cmd_t                             */
    int32_t  fd;
    int32_t  mode;                      /* open_mode_t                       */
    uint32_t offset;
    uint32_t length;                    /* bytes to read, or bytes in data[] */
    char     user[PROTO_MAX_USER];
    char     path[PROTO_MAX_PATH];
    char     data[PROTO_MAX_REQ_DATA];
} request_t;

typedef struct {
    uint32_t req_id;
    int32_t  status;                    /* status_t                          */
    int32_t  fd;                        /* for CMD_OPEN                      */
    uint32_t length;                    /* valid bytes in data[]             */
    char     data[PROTO_MAX_RESP_DATA]; /* file bytes or text output         */
} response_t;

_Static_assert(sizeof(request_t)  <= PIPE_BUF, "request_t must fit in PIPE_BUF");
_Static_assert(sizeof(response_t) <= PIPE_BUF, "response_t must fit in PIPE_BUF");

const char *cmd_name(int cmd);
const char *status_str(int status);

#endif

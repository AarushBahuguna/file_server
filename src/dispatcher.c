#include <errno.h>
#include <string.h>
#include <time.h>
#include "dispatcher.h"
#include "protocol.h"
#include "ipc.h"
#include "session.h"
#include "stats.h"
#include "log.h"
#include "rwlock.h"
#include "cache.h"
#include "fs.h"
#include "sched.h"

static unsigned long g_requests;
static time_t g_start;
static int g_real_fds[SESS_MAX][SESS_MAX_FDS];

static int status_from_errno(int rc) {
    if (rc >= 0) return ST_OK;
    switch (rc) {
        case -ENOENT: return ST_ERR_NOENT;
        case -EEXIST: return ST_ERR_EXIST;
        case -ENOSPC: return ST_ERR_NOSPC;
        case -EBADF: return ST_ERR_BADFD;
        case -EPERM: case -EACCES: return ST_ERR_PERM;
        case -EINVAL: case -ENAMETOOLONG: case -ENOTDIR: case -EISDIR: return ST_ERR_INVAL;
        case -EDEADLK: return ST_ERR_DEADLOCK;
        case -EBUSY: case -ENOTEMPTY: return ST_ERR_BUSY;
        default: return ST_ERR_INTERNAL;
    }
}

static void do_stats(response_t *rs) {
    char fsout[2048]; size_t len = 0;
    fs_report(fsout, sizeof fsout);
    stats_put_u64(rs->data, sizeof rs->data, &len, "disk.total_blocks", NUM_BLOCKS);
    stats_put_u64(rs->data, sizeof rs->data, &len, "disk.block_size", BLOCK_SIZE);
    cache_stats_t cs; cache_get_stats(&cs);
    stats_put_u64(rs->data, sizeof rs->data, &len, "cache.capacity", CACHE_ENTRIES);
    stats_put_u64(rs->data, sizeof rs->data, &len, "cache.hits", cs.hits);
    stats_put_u64(rs->data, sizeof rs->data, &len, "cache.misses", cs.misses);
    stats_put_u64(rs->data, sizeof rs->data, &len, "cache.evictions", cs.evictions);
    stats_put_u64(rs->data, sizeof rs->data, &len, "cache.writebacks", cs.writebacks);
    struct sched_stats ss; sched_stats(&ss);
    stats_put_u64(rs->data, sizeof rs->data, &len, "sched.requests", (unsigned long long)ss.requests_served);
    stats_put_u64(rs->data, sizeof rs->data, &len, "sched.head_movement", (unsigned long long)ss.total_head_movement);
    stats_put_u64(rs->data, sizeof rs->data, &len, "server.requests", g_requests);
    stats_put_u64(rs->data, sizeof rs->data, &len, "server.uptime_s", (unsigned long long)(time(NULL) - g_start));
    stats_put_u64(rs->data, sizeof rs->data, &len, "sessions.active", (unsigned long long)session_count());
    rs->length = (uint32_t)len;
    rs->status = ST_OK;
}

static unsigned long hash_path(const char *path) {
    unsigned long hash = 5381;
    if (!path) return 0;
    for (const unsigned char *p = (const unsigned char *)path; *p; p++) {
        hash = ((hash << 5) + hash) + *p;
    }
    return hash ? hash : 1;
}

static void close_real_fd(pid_t pid, int logical_fd) {
    int slot = logical_fd - SESS_FIRST_FD;
    if (slot < 0 || slot >= SESS_MAX_FDS) return;
    int real_fd = g_real_fds[pid % SESS_MAX][slot];
    if (real_fd >= 0) {
        fs_close(real_fd);
        g_real_fds[pid % SESS_MAX][slot] = -1;
    }
}

static int graph_session(pid_t pid) {
    unsigned long id = (unsigned long)(unsigned int)pid;
    return (int)(id % SESS_MAX);
}

int dispatch_request_sync(request_t *req, response_t *response) {
    if (!req || !response) return -EINVAL;
    response_t rs = {0}; rs.req_id = req->req_id;
    struct timespec start, end; clock_gettime(CLOCK_MONOTONIC, &start); g_requests++;

    pid_t pid = req->client_pid;
    char abs[PROTO_MAX_PATH]; sess_file_t opened;
    int rc = ST_OK;

    if (req->cmd == CMD_LOGIN) { rs.status = session_login(pid, req->user); goto finish; }
    if (req->cmd == CMD_LOGOUT) {
        sess_file_t files[SESS_MAX_FDS]; int n = 0;
        rc = session_logout(pid, files, SESS_MAX_FDS, &n);
        for (int i = 0; i < n; i++) {
            close_real_fd(pid, files[i].fd);
            lock_release((int)hash_path(files[i].path), graph_session(pid));
        }
        rs.status = rc; goto finish;
    }

    if (session_user(pid, abs, sizeof abs) != ST_OK) { rs.status = ST_ERR_PERM; goto finish; }

    switch (req->cmd) {
        case CMD_MKDIR:
            rc = session_resolve(pid, req->path, abs, sizeof abs);
            if (rc == ST_OK) {
                int file_id = (int)hash_path(abs);
                int lock_rc = lock_acquire(file_id, LOCK_WRITE, graph_session(pid));
                rc = lock_rc < 0 ? lock_rc : fs_mkdir(abs, fs_root_inode());
                if (lock_rc >= 0) lock_release(file_id, graph_session(pid));
            }
            rs.status = status_from_errno(rc); break;
        case CMD_CREATE:
            rc = session_resolve(pid, req->path, abs, sizeof abs);
            if (rc == ST_OK) {
                int file_id = (int)hash_path(abs);
                int lock_rc = lock_acquire(file_id, LOCK_WRITE, graph_session(pid));
                rc = lock_rc < 0 ? lock_rc : fs_create(abs, (uint16_t)pid, fs_root_inode());
                if (lock_rc >= 0) lock_release(file_id, graph_session(pid));
            }
            rs.status = status_from_errno(rc); break;
        case CMD_CD:
            rc = session_resolve(pid, req->path, abs, sizeof abs);
            if (rc == ST_OK) {
                int cwd = fs_cd(abs, fs_root_inode());
                if (cwd >= 0) {
                    rc = session_chdir(pid, abs);
                    snprintf(rs.data, sizeof rs.data, "%s", abs);
                    rs.length = (uint32_t)strlen(abs);
                } else rc = cwd;
            }
            rs.status = status_from_errno(rc); break;
        case CMD_LS:
            rc = session_resolve(pid, req->path[0] ? req->path : ".", abs, sizeof abs);
            if (rc == ST_OK) {
                int file_id = (int)hash_path(abs);
                int lock_rc = lock_acquire(file_id, LOCK_READ, graph_session(pid));
                rc = lock_rc < 0 ? lock_rc : fs_ls(abs, fs_root_inode(), rs.data, sizeof rs.data);
                if (lock_rc >= 0) lock_release(file_id, graph_session(pid));
            }
            if (rc >= 0) rs.length = (uint32_t)rc;
            rs.status = status_from_errno(rc); break;
        case CMD_DELETE:
            rc = session_resolve(pid, req->path, abs, sizeof abs);
            if (rc == ST_OK) {
                int file_id = (int)hash_path(abs);
                int lock_rc = lock_acquire(file_id, LOCK_WRITE, graph_session(pid));
                rc = lock_rc < 0 ? lock_rc : fs_delete(abs, fs_root_inode());
                if (lock_rc >= 0) lock_release(file_id, graph_session(pid));
            }
            rs.status = status_from_errno(rc); break;
        case CMD_OPEN:
            rc = session_resolve(pid, req->path, abs, sizeof abs);
            if (rc == ST_OK) {
                int file_id = (int)hash_path(abs);
                int mode = req->mode == OPEN_WRITE ? LOCK_WRITE : LOCK_READ;
                int lock_rc = lock_acquire(file_id, mode, graph_session(pid));
                rc = lock_rc < 0 ? lock_rc : fs_open(abs, req->mode, fs_root_inode());
                if (lock_rc >= 0) {
                    if (rc >= 0) {
                        int real_fd = rc;
                        int logical_fd = real_fd;
                        rc = session_fd_open(pid, abs, req->mode, &logical_fd);
                        if (rc == ST_OK) {
                            int slot = logical_fd - SESS_FIRST_FD;
                            g_real_fds[pid % SESS_MAX][slot] = real_fd;
                            rs.fd = logical_fd;
                        } else fs_close(real_fd);
                    }
                }
            }
            rs.status = status_from_errno(rc); break;
        case CMD_CLOSE:
            rc = session_fd_get(pid, req->fd, &opened);
            if (rc == ST_OK) {
                close_real_fd(pid, req->fd);
                int file_id = (int)hash_path(opened.path);
                lock_release(file_id, graph_session(pid));
                rc = session_fd_close(pid, req->fd);
            }
            rs.status = status_from_errno(rc); break;
        case CMD_READ:
            rc = session_fd_get(pid, req->fd, &opened);
            if (rc == ST_OK) {
                int slot = req->fd - SESS_FIRST_FD;
                int real_fd = g_real_fds[pid % SESS_MAX][slot];
                rc = real_fd >= 0
                    ? (int)fs_read(real_fd, rs.data, req->length, req->offset)
                    : -EBADF;
            }
            if (rc >= 0) rs.length = (uint32_t)rc;
            rs.status = status_from_errno(rc); break;
        case CMD_WRITE:
            rc = session_fd_get(pid, req->fd, &opened);
            if (rc == ST_OK && opened.mode != OPEN_WRITE) rc = -EPERM;
            if (rc == ST_OK) {
                int slot = req->fd - SESS_FIRST_FD;
                int real_fd = g_real_fds[pid % SESS_MAX][slot];
                rc = real_fd >= 0
                    ? (int)fs_write(real_fd, req->data, req->length, req->offset)
                    : -EBADF;
            }
            if (rc >= 0) rs.length = (uint32_t)rc;
            rs.status = status_from_errno(rc); break;
        case CMD_STATS:
            do_stats(&rs); break;
        default:
            rs.status = ST_ERR_INVAL;
    }

finish:
    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed = (double)(end.tv_sec - start.tv_sec) * 1000.0 + (double)(end.tv_nsec - start.tv_nsec) / 1e6;
    log_request(req, &rs, elapsed);
    *response = rs;
    return 0;
}

void dispatch_request(request_t *req) {
    if (!req) return;
    response_t rs;
    if (dispatch_request_sync(req, &rs) == 0)
        ipc_send_response(req->client_pid, &rs);
}

void dispatcher_init(void) {
    g_start = time(NULL);
    g_requests = 0;
    memset(g_real_fds, -1, sizeof(g_real_fds));
}

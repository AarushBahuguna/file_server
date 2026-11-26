#include "session.h"
#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>
#include <errno.h>
#include <time.h>

typedef struct {
    int in_use;
    pid_t pid;
    char user[PROTO_MAX_USER];
    char cwd[PROTO_MAX_PATH];
    sess_file_t files[SESS_MAX_FDS];
    int file_used[SESS_MAX_FDS];
} session_t;

static session_t g_s[SESS_MAX];
static pthread_mutex_t g_mu = PTHREAD_MUTEX_INITIALIZER;

static session_t *find(pid_t pid) {
    for (int i = 0; i < SESS_MAX; i++)
        if (g_s[i].in_use && g_s[i].pid == pid) return &g_s[i];
    return NULL;
}

int path_normalize(const char *cwd, const char *path, char *out, size_t n) {
    char buf[2 * PROTO_MAX_PATH + 4];
    int w;
    if (path[0] == '/') w = snprintf(buf, sizeof buf, "%s", path);
    else w = snprintf(buf, sizeof buf, "%s/%s", (cwd && *cwd) ? cwd : "/", path);
    if (w < 0 || (size_t)w >= sizeof buf) return ST_ERR_INVAL;

    const char *comp[64];
    int nc = 0;
    char *save = NULL;
    for (char *t = strtok_r(buf, "/", &save); t; t = strtok_r(NULL, "/", &save)) {
        if (!strcmp(t, ".")) continue;
        if (!strcmp(t, "..")) { if (nc > 0) nc--; continue; }
        if (nc >= 64) return ST_ERR_INVAL;
        comp[nc++] = t;
    }
    if (nc == 0) {
        if (n < 2) return ST_ERR_INVAL;
        strcpy(out, "/");
        return ST_OK;
    }
    size_t len = 0;
    for (int i = 0; i < nc; i++) {
        size_t cl = strlen(comp[i]);
        if (len + 1 + cl + 1 > n) return ST_ERR_INVAL;
        out[len++] = '/';
        memcpy(out + len, comp[i], cl);
        len += cl;
    }
    out[len] = '\0';
    return ST_OK;
}

int session_login(pid_t pid, const char *user) {
    if (!user || !*user) return ST_ERR_INVAL;
    pthread_mutex_lock(&g_mu);
    session_t *s = find(pid);
    if (!s) {
        for (int i = 0; i < SESS_MAX; i++) if (!g_s[i].in_use) { s = &g_s[i]; break; }
        if (!s) { pthread_mutex_unlock(&g_mu); return ST_ERR_BUSY; }
    }
    memset(s, 0, sizeof *s);
    s->in_use = 1;
    s->pid = pid;
    snprintf(s->user, sizeof s->user, "%s", user);
    strcpy(s->cwd, "/");
    pthread_mutex_unlock(&g_mu);
    return ST_OK;
}

int session_logout(pid_t pid, sess_file_t *open_files, int max, int *n_open) {
    pthread_mutex_lock(&g_mu);
    session_t *s = find(pid);
    if (!s) { pthread_mutex_unlock(&g_mu); return ST_ERR_PERM; }
    int n = 0;
    for (int i = 0; i < SESS_MAX_FDS; i++)
        if (s->file_used[i] && open_files && n < max) open_files[n++] = s->files[i];
    if (n_open) *n_open = n;
    memset(s, 0, sizeof *s);
    pthread_mutex_unlock(&g_mu);
    return ST_OK;
}

int session_user(pid_t pid, char *out, size_t n) {
    pthread_mutex_lock(&g_mu);
    session_t *s = find(pid);
    if (s) snprintf(out, n, "%s", s->user);
    pthread_mutex_unlock(&g_mu);
    return s ? ST_OK : ST_ERR_PERM;
}

int session_cwd(pid_t pid, char *out, size_t n) {
    pthread_mutex_lock(&g_mu);
    session_t *s = find(pid);
    if (s) snprintf(out, n, "%s", s->cwd);
    pthread_mutex_unlock(&g_mu);
    return s ? ST_OK : ST_ERR_PERM;
}

int session_chdir(pid_t pid, const char *abs_path) {
    if (!abs_path || abs_path[0] != '/' || strlen(abs_path) >= PROTO_MAX_PATH) return ST_ERR_INVAL;
    pthread_mutex_lock(&g_mu);
    session_t *s = find(pid);
    if (s) snprintf(s->cwd, sizeof s->cwd, "%s", abs_path);
    pthread_mutex_unlock(&g_mu);
    return s ? ST_OK : ST_ERR_PERM;
}

int session_resolve(pid_t pid, const char *path, char *out, size_t n) {
    char cwd[PROTO_MAX_PATH];
    int rc = session_cwd(pid, cwd, sizeof cwd);
    if (rc != ST_OK) return rc;
    return path_normalize(cwd, path, out, n);
}

int session_fd_open(pid_t pid, const char *abs_path, int mode, int *fd) {
    if (!abs_path || strlen(abs_path) >= PROTO_MAX_PATH) return ST_ERR_INVAL;
    pthread_mutex_lock(&g_mu);
    session_t *s = find(pid);
    int rc = ST_ERR_PERM;
    if (s) {
        rc = ST_ERR_BUSY;                                  /* no free fd slot */
        for (int i = 0; i < SESS_MAX_FDS; i++) {
            if (s->file_used[i]) continue;
            s->file_used[i] = 1;
            s->files[i].fd = i + SESS_FIRST_FD;
            s->files[i].mode = mode;
            snprintf(s->files[i].path, sizeof s->files[i].path, "%s", abs_path);
            *fd = s->files[i].fd;
            rc = ST_OK;
            break;
        }
    }
    pthread_mutex_unlock(&g_mu);
    return rc;
}

int session_fd_get(pid_t pid, int fd, sess_file_t *out) {
    int i = fd - SESS_FIRST_FD;
    pthread_mutex_lock(&g_mu);
    session_t *s = find(pid);
    int rc = s ? ST_ERR_BADFD : ST_ERR_PERM;
    if (s && i >= 0 && i < SESS_MAX_FDS && s->file_used[i]) {
        if (out) *out = s->files[i];
        rc = ST_OK;
    }
    pthread_mutex_unlock(&g_mu);
    return rc;
}

int session_fd_close(pid_t pid, int fd) {
    int i = fd - SESS_FIRST_FD;
    pthread_mutex_lock(&g_mu);
    session_t *s = find(pid);
    int rc = s ? ST_ERR_BADFD : ST_ERR_PERM;
    if (s && i >= 0 && i < SESS_MAX_FDS && s->file_used[i]) {
        s->file_used[i] = 0;
        memset(&s->files[i], 0, sizeof s->files[i]);
        rc = ST_OK;
    }
    pthread_mutex_unlock(&g_mu);
    return rc;
}

int session_find_dead(pid_t *pids, int max) {
    int n = 0;
    pthread_mutex_lock(&g_mu);
    for (int i = 0; i < SESS_MAX && n < max; i++)
        if (g_s[i].in_use && kill(g_s[i].pid, 0) < 0 && errno == ESRCH) pids[n++] = g_s[i].pid;
    pthread_mutex_unlock(&g_mu);
    return n;
}

int session_count(void) {
    int n = 0;
    pthread_mutex_lock(&g_mu);
    for (int i = 0; i < SESS_MAX; i++) if (g_s[i].in_use) n++;
    pthread_mutex_unlock(&g_mu);
    return n;
}

void session_summary(char *buf, size_t n) {
    size_t len = 0;
    int count = 0;
    pthread_mutex_lock(&g_mu);
    for (int i = 0; i < SESS_MAX; i++) if (g_s[i].in_use) count++;
    len += (size_t)snprintf(buf + len, n - len, "sessions: %d active\n", count);
    for (int i = 0; i < SESS_MAX && len < n; i++) {
        if (!g_s[i].in_use) continue;
        int open_fds = 0;
        for (int j = 0; j < SESS_MAX_FDS; j++) open_fds += g_s[i].file_used[j];
        len += (size_t)snprintf(buf + len, n - len, "  %s (pid %d) cwd=%s open_fds=%d\n",
                                g_s[i].user, (int)g_s[i].pid, g_s[i].cwd, open_fds);
    }
    pthread_mutex_unlock(&g_mu);
    if (len >= n) buf[n - 1] = '\0';
}

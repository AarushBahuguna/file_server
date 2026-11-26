#include "log.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>

static pthread_mutex_t g_mu = PTHREAD_MUTEX_INITIALIZER;
static FILE *g_fp = NULL;
static volatile int g_ready = 0;
static int g_stderr = 0;
static log_level_t g_min = LOG_INFO;
static const char *LV[] = { "DEBUG", "INFO ", "WARN ", "ERROR" };

int log_init(const char *path, log_level_t min_level, int also_stderr) {
    pthread_mutex_lock(&g_mu);
    if (g_fp) { fclose(g_fp); g_fp = NULL; }
    if (path) {
        g_fp = fopen(path, "a");
        if (!g_fp) { pthread_mutex_unlock(&g_mu); return -1; }
        setvbuf(g_fp, NULL, _IOLBF, 0);
    }
    g_min = min_level;
    g_stderr = also_stderr;
    g_ready = 1;
    pthread_mutex_unlock(&g_mu);
    return 0;
}

void log_close(void) {
    pthread_mutex_lock(&g_mu);
    if (g_fp) fclose(g_fp);
    g_fp = NULL;
    g_ready = 0;
    pthread_mutex_unlock(&g_mu);
}

void log_write(log_level_t lvl, const char *comp, const char *fmt, ...) {
    if (!g_ready || lvl < g_min) return;
    char msg[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof msg, fmt, ap);
    va_end(ap);

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    struct tm tm;
    localtime_r(&ts.tv_sec, &tm);
    char when[32];
    strftime(when, sizeof when, "%Y-%m-%d %H:%M:%S", &tm);

    pthread_mutex_lock(&g_mu);
    if (g_ready) {
        if (g_fp)
            fprintf(g_fp, "%s.%03ld [%s] [%d/%lx] %s: %s\n", when, ts.tv_nsec / 1000000, LV[lvl],
                    (int)getpid(), (unsigned long)pthread_self(), comp, msg);
        if (g_stderr)
            fprintf(stderr, "%s.%03ld [%s] %s: %s\n", when, ts.tv_nsec / 1000000, LV[lvl], comp, msg);
    }
    pthread_mutex_unlock(&g_mu);
}

void log_request(const request_t *req, const response_t *resp, double elapsed_ms) {
    log_write(LOG_INFO, "req", "id=%u user=%s cmd=%s path=%s fd=%d off=%u len=%u -> %s (%.2f ms)",
              req->req_id, req->user, cmd_name(req->cmd), req->path, req->fd, req->offset, req->length,
              resp ? status_str(resp->status) : "no reply", elapsed_ms);
}

/* log.h - thread-safe logging (OWNER: Shubham). Everyone can use it.
 *   log_init("server.log", LOG_INFO, 0);   once at program start
 *   LOG_I("cache", "evicted block %d", blk);
 * If log_init() was never called, logging is silently disabled. */
#ifndef LOG_H
#define LOG_H
#include "protocol.h"

typedef enum { LOG_DEBUG = 0, LOG_INFO, LOG_WARN, LOG_ERROR } log_level_t;

int  log_init(const char *path, log_level_t min_level, int also_stderr);  /* path NULL = no file; 0 ok */
void log_close(void);
void log_write(log_level_t lvl, const char *component, const char *fmt, ...)
     __attribute__((format(printf, 3, 4)));
/* one line per request: user, command, path, fd, offset, length, status, latency */
void log_request(const request_t *req, const response_t *resp, double elapsed_ms);

#define LOG_D(comp, ...) log_write(LOG_DEBUG, comp, __VA_ARGS__)
#define LOG_I(comp, ...) log_write(LOG_INFO,  comp, __VA_ARGS__)
#define LOG_W(comp, ...) log_write(LOG_WARN,  comp, __VA_ARGS__)
#define LOG_E(comp, ...) log_write(LOG_ERROR, comp, __VA_ARGS__)
#endif

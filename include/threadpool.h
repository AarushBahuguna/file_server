#ifndef THREADPOOL_H
#define THREADPOOL_H
#include "protocol.h"
#define QUEUE_SIZE 64
#define MAX_WORKERS 4
int threadpool_init(int workers);
int threadpool_submit(request_t *request);
void threadpool_shutdown(void);
#endif

#ifndef DISPATCHER_H
#define DISPATCHER_H
#include "protocol.h"
void dispatch_request(request_t *request);
int dispatch_request_sync(request_t *request, response_t *response);
void dispatcher_init(void);
#endif

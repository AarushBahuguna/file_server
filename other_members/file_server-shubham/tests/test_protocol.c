#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "protocol.h"

int main(void) {
    assert(sizeof(request_t)  <= PIPE_BUF);
    assert(sizeof(response_t) <= PIPE_BUF);
    assert(strcmp(cmd_name(CMD_WRITE), "WRITE") == 0);
    assert(strcmp(status_str(ST_OK), "OK") == 0);
    printf("test_protocol: PASS\n");
    return 0;
}

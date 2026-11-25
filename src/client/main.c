/* CLI client - Day 1 stub; parser comes on Day 3 */
#include <stdio.h>
#include <string.h>
#include "ipc.h"

int main(void) {
    printf("client stub: protocol OK (request=%zu bytes, response=%zu bytes)\n",
           sizeof(request_t), sizeof(response_t));
    return 0;
}

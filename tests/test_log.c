#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include <unistd.h>
#include "log.h"

#define THREADS 4
#define LINES 200

static void *worker(void *arg) {
    for (int i = 0; i < LINES; i++) LOG_I("test", "thread %ld line %d", (long)arg, i);
    return NULL;
}

int main(void) {
    const char *path = "/tmp/test_log_output.txt";
    unlink(path);
    LOG_I("test", "before init: must be ignored");
    assert(log_init(path, LOG_INFO, 0) == 0);
    LOG_D("test", "debug: filtered out by level");
    LOG_W("test", "warn line");

    pthread_t t[THREADS];
    for (long i = 0; i < THREADS; i++) pthread_create(&t[i], NULL, worker, (void *)i);
    for (int i = 0; i < THREADS; i++) pthread_join(t[i], NULL);

    request_t rq; response_t rs;
    memset(&rq, 0, sizeof rq); memset(&rs, 0, sizeof rs);
    rq.cmd = CMD_MKDIR; strcpy(rq.user, "alice"); strcpy(rq.path, "/docs");
    rs.status = ST_ERR_EXIST;
    log_request(&rq, &rs, 1.5);
    log_close();

    FILE *fp = fopen(path, "r");
    assert(fp);
    char line[1024];
    int lines = 0, warn = 0, req = 0;
    while (fgets(line, sizeof line, fp)) {
        lines++;
        assert(line[0] == '2' && strstr(line, "] "));          /* every line starts with a timestamp: no interleaving */
        assert(!strstr(line, "before init") && !strstr(line, "debug:"));
        if (strstr(line, "[WARN ]")) warn++;
        if (strstr(line, "cmd=MKDIR") && strstr(line, "already exists")) req++;
    }
    fclose(fp);
    assert(lines == THREADS * LINES + 2);
    assert(warn == 1 && req == 1);
    printf("test_log: PASS\n");
    return 0;
}

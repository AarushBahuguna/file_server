#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/wait.h>
#include "session.h"

static char out[PROTO_MAX_PATH];
#define N(cwd, p) (path_normalize(cwd, p, out, sizeof out) == ST_OK)

static void *worker(void *arg) {
    pid_t pid = 50000 + (int)(long)arg;
    for (int i = 0; i < 300; i++) {
        assert(session_login(pid, "w") == ST_OK);
        int fd;
        assert(session_fd_open(pid, "/f", OPEN_READ, &fd) == ST_OK && fd == SESS_FIRST_FD);
        assert(session_fd_close(pid, fd) == ST_OK);
        assert(session_logout(pid, NULL, 0, NULL) == ST_OK);
    }
    return NULL;
}

int main(void) {
    /* path_normalize */
    assert(N("/", "a/b") && !strcmp(out, "/a/b"));
    assert(N("/x/y", "z") && !strcmp(out, "/x/y/z"));
    assert(N("/x/y", "../z") && !strcmp(out, "/x/z"));
    assert(N("/x/y", "../../..") && !strcmp(out, "/"));
    assert(N("/x", "/a//b/./c/") && !strcmp(out, "/a/b/c"));
    assert(N("/x", "") && !strcmp(out, "/x"));
    assert(N("/", "/") && !strcmp(out, "/"));
    char longp[300]; memset(longp, 'a', 299); longp[299] = 0;
    assert(path_normalize("/", longp, out, sizeof out) == ST_ERR_INVAL);

    /* login / cwd / resolve */
    assert(session_login(100, "") == ST_ERR_INVAL);
    assert(session_cwd(100, out, sizeof out) == ST_ERR_PERM);          /* not logged in */
    assert(session_login(100, "alice") == ST_OK);
    assert(session_login(200, "bob") == ST_OK);
    assert(session_count() == 2);
    assert(session_cwd(100, out, sizeof out) == ST_OK && !strcmp(out, "/"));
    assert(session_chdir(100, "/docs") == ST_OK);
    assert(session_resolve(100, "x.txt", out, sizeof out) == ST_OK && !strcmp(out, "/docs/x.txt"));
    assert(session_resolve(200, "x.txt", out, sizeof out) == ST_OK && !strcmp(out, "/x.txt"));   /* separate cwd */
    assert(session_chdir(100, "relative") == ST_ERR_INVAL);

    /* fd table */
    int fd1, fd2;
    sess_file_t f;
    assert(session_fd_open(100, "/docs/a", OPEN_WRITE, &fd1) == ST_OK && fd1 == 3);
    assert(session_fd_open(100, "/docs/b", OPEN_READ, &fd2) == ST_OK && fd2 == 4);
    assert(session_fd_get(100, 3, &f) == ST_OK && f.mode == OPEN_WRITE && !strcmp(f.path, "/docs/a"));
    assert(session_fd_get(200, 3, &f) == ST_ERR_BADFD);                 /* fds are per session */
    assert(session_fd_get(100, 9, &f) == ST_ERR_BADFD);
    assert(session_fd_close(100, 3) == ST_OK);
    assert(session_fd_close(100, 3) == ST_ERR_BADFD);
    int fd3;
    assert(session_fd_open(100, "/c", OPEN_READ, &fd3) == ST_OK && fd3 == 3);   /* lowest free fd reused */
    for (int i = 0; i < SESS_MAX_FDS - 2; i++) assert(session_fd_open(100, "/m", OPEN_READ, &fd3) == ST_OK);
    assert(session_fd_open(100, "/m", OPEN_READ, &fd3) == ST_ERR_BUSY); /* table full */

    /* logout returns what was still open so the server can release locks */
    sess_file_t left[SESS_MAX_FDS];
    int n = -1;
    assert(session_logout(100, left, SESS_MAX_FDS, &n) == ST_OK && n == SESS_MAX_FDS);
    assert(session_logout(100, left, SESS_MAX_FDS, &n) == ST_ERR_PERM);
    assert(session_count() == 1);

    /* dead client detection */
    pid_t child = fork();
    if (child == 0) _exit(0);
    waitpid(child, NULL, 0);
    assert(session_login(child, "ghost") == ST_OK);
    pid_t dead[8];
    int nd = session_find_dead(dead, 8);
    int found = 0;
    for (int i = 0; i < nd; i++) if (dead[i] == child) found = 1;
    assert(found);
    assert(session_logout(child, NULL, 0, NULL) == ST_OK);

    /* summary */
    char sum[512];
    session_summary(sum, sizeof sum);
    assert(strstr(sum, "sessions: 1 active") && strstr(sum, "bob"));
    session_logout(200, NULL, 0, NULL);

    /* thread safety */
    pthread_t t[8];
    for (long i = 0; i < 8; i++) pthread_create(&t[i], NULL, worker, (void *)i);
    for (int i = 0; i < 8; i++) pthread_join(t[i], NULL);
    assert(session_count() == 0);

    printf("test_session: PASS\n");
    return 0;
}

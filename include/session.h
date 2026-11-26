/* session.h - per-user sessions for the SERVER (OWNER: Shubham, used by Shwetank's dispatcher).
 * A session is created at CMD_LOGIN and keyed by the client's pid. It stores the user name,
 * current directory and the table of open fds, so the server can turn relative paths into
 * absolute ones, validate fds, and release everything when a client logs out or dies.
 * All functions are thread-safe and return status_t codes (ST_OK = 0).
 *
 * Typical dispatcher flow:
 *   CMD_LOGIN : session_login(req.client_pid, req.user)
 *   CMD_CD    : session_resolve(pid, req.path, abs, sizeof abs); fs check dir; session_chdir(pid, abs);
 *               respond with data = abs
 *   CMD_OPEN  : session_resolve(...); lock_acquire(...); session_fd_open(pid, abs, mode, &fd)
 *   CMD_READ  : session_fd_get(pid, req.fd, &f)  -> f.path, f.mode
 *   CMD_CLOSE : session_fd_get(...); lock_release(...); session_fd_close(pid, req.fd)
 *   CMD_LOGOUT: session_logout(pid, files, N, &n); lock_release() every returned file
 *   periodically: session_find_dead(pids, N) -> session_logout() each (releases locks of crashed clients)
 */
#ifndef SESSION_H
#define SESSION_H
#include <stddef.h>
#include <sys/types.h>
#include "protocol.h"

#define SESS_MAX      64
#define SESS_MAX_FDS  32
#define SESS_FIRST_FD 3

typedef struct {
    int  fd;
    int  mode;                     /* open_mode_t */
    char path[PROTO_MAX_PATH];     /* absolute path */
} sess_file_t;

/* Makes an absolute, normalised path ('.', '..', '//' removed). Relative paths start at cwd.
 * An empty path means cwd. '..' at the root stays at the root. */
int path_normalize(const char *cwd, const char *path, char *out, size_t n);

int  session_login(pid_t pid, const char *user);       /* logging in twice with one pid resets it */
int  session_logout(pid_t pid, sess_file_t *open_files, int max, int *n_open);
int  session_user(pid_t pid, char *out, size_t n);
int  session_cwd(pid_t pid, char *out, size_t n);
int  session_chdir(pid_t pid, const char *abs_path);
int  session_resolve(pid_t pid, const char *path, char *out, size_t n);

int  session_fd_open(pid_t pid, const char *abs_path, int mode, int *fd);
int  session_fd_get(pid_t pid, int fd, sess_file_t *out);
int  session_fd_close(pid_t pid, int fd);

int  session_find_dead(pid_t *pids, int max);          /* sessions whose client process is gone */
int  session_count(void);
void session_summary(char *buf, size_t n);             /* text for the 'stats' command */
#endif

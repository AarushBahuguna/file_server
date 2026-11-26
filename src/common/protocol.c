#include "protocol.h"

static const char *CMD_NAMES[CMD_COUNT] = {
    "NONE","LOGIN","LOGOUT","MKDIR","CD","LS","CREATE","OPEN",
    "CLOSE","READ","WRITE","DELETE","STATS"
};

const char *cmd_name(int cmd) {
    return (cmd >= 0 && cmd < CMD_COUNT) ? CMD_NAMES[cmd] : "?";
}

const char *status_str(int s) {
    switch (s) {
    case ST_OK:           return "OK";
    case ST_ERR_NOENT:    return "no such file or directory";
    case ST_ERR_EXIST:    return "already exists";
    case ST_ERR_NOSPC:    return "no space left on disk";
    case ST_ERR_BADFD:    return "bad file descriptor";
    case ST_ERR_PERM:     return "permission denied";
    case ST_ERR_INVAL:    return "invalid argument";
    case ST_ERR_DEADLOCK: return "aborted: deadlock victim";
    case ST_ERR_BUSY:     return "resource busy";
    default:              return "internal error";
    }
}

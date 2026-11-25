#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "parse.h"

static parsed_cmd_t pc;
static char err[160];
#define P(line) parse_command(line, &pc, err, sizeof err)

int main(void) {
    assert(P("") == P_EMPTY);
    assert(P("   \n") == P_EMPTY);
    assert(P("# comment") == P_EMPTY);
    assert(P("help") == P_HELP);
    assert(P("exit") == P_EXIT);

    assert(P("mkdir docs") == P_SERVER && pc.cmd == CMD_MKDIR && !strcmp(pc.path, "docs"));
    assert(P("mkdir") == P_ERROR);
    assert(P("mkdir a b") == P_ERROR);
    assert(P("cd") == P_SERVER && pc.cmd == CMD_CD && !strcmp(pc.path, "/"));
    assert(P("cd /home/x") == P_SERVER && !strcmp(pc.path, "/home/x"));
    assert(P("ls") == P_SERVER && pc.cmd == CMD_LS && pc.path[0] == '\0');
    assert(P("touch f.txt") == P_SERVER && pc.cmd == CMD_CREATE);
    assert(P("rm f.txt") == P_SERVER && pc.cmd == CMD_DELETE);

    assert(P("open f.txt") == P_SERVER && pc.cmd == CMD_OPEN && pc.mode == OPEN_READ);
    assert(P("open f.txt w") == P_SERVER && pc.mode == OPEN_WRITE);
    assert(P("open f.txt x") == P_ERROR);

    assert(P("close 3") == P_SERVER && pc.cmd == CMD_CLOSE && pc.fd == 3);
    assert(P("close abc") == P_ERROR);
    assert(P("close -1") == P_ERROR);

    assert(P("read 3 10 100") == P_SERVER && pc.cmd == CMD_READ && pc.fd == 3 && pc.offset == 10 && pc.length == 100);
    assert(P("read 3 10") == P_ERROR);

    assert(P("write 3 0 hello  big world\n") == P_SERVER && pc.cmd == CMD_WRITE);
    assert(!strcmp(pc.text, "hello  big world") && pc.text_len == 16);
    assert(P("write 3 0") == P_ERROR);

    assert(P("stats") == P_SERVER && pc.cmd == CMD_STATS);
    assert(P("stats now") == P_ERROR);
    assert(P("frobnicate") == P_ERROR);

    char longpath[300];
    memset(longpath, 'a', sizeof longpath - 1);
    longpath[sizeof longpath - 1] = '\0';
    char line[400];
    snprintf(line, sizeof line, "mkdir %s", longpath);
    assert(P(line) == P_ERROR);

    printf("test_parse: PASS\n");
    return 0;
}

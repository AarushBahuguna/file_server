#include <errno.h>
#include <stdio.h>

#include "waitfor.h"

static int fail(const char *message)
{
    fprintf(stderr, "FAIL: %s\n", message);
    return 1;
}

static int test_empty_graph(void)
{
    int cycle[MAX_SESSIONS];
    int cycle_len = -1;

    wfg_init();
    if (wfg_detect(-1, cycle, &cycle_len) != -1) {
        return fail("empty graph detected a deadlock");
    }
    if (cycle_len != 0) {
        return fail("empty graph returned a cycle length");
    }
    wfg_destroy();
    return 0;
}

static int test_two_cycle(void)
{
    int cycle[MAX_SESSIONS];
    int cycle_len = 0;
    int holders[1];
    int victim;

    wfg_init();

    holders[0] = 1;
    wfg_on_wait(0, 100, holders, 1);
    holders[0] = 0;
    wfg_on_wait(1, 101, holders, 1);

    victim = wfg_detect(1, cycle, &cycle_len);
    if (victim != 1) {
        return fail("two-session cycle selected the wrong victim");
    }
    if (cycle_len < 2) {
        return fail("two-session cycle returned a short path");
    }

    if (wfg_recover_victim(victim) != -EDEADLK) {
        return fail("victim recovery did not return -EDEADLK");
    }
    if (!wfg_is_aborted(victim)) {
        return fail("victim was not marked aborted");
    }

    wfg_reset_abort(victim);
    if (wfg_is_aborted(victim)) {
        return fail("abort state was not reset");
    }

    wfg_destroy();
    return 0;
}

static int test_three_cycle(void)
{
    int cycle[MAX_SESSIONS];
    int cycle_len = 0;
    int holders[1];

    wfg_init();

    holders[0] = 1;
    wfg_on_wait(0, 100, holders, 1);
    holders[0] = 2;
    wfg_on_wait(1, 101, holders, 1);
    holders[0] = 0;
    wfg_on_wait(2, 102, holders, 1);

    if (wfg_detect(-1, cycle, &cycle_len) != 2) {
        return fail("three-session cycle selected the wrong youngest victim");
    }
    if (cycle_len != 3) {
        return fail("three-session cycle path has the wrong length");
    }

    wfg_destroy();
    return 0;
}

static int test_chain_no_cycle(void)
{
    int cycle[MAX_SESSIONS];
    int cycle_len = -1;
    int holders[1];

    wfg_init();

    holders[0] = 1;
    wfg_on_wait(0, 100, holders, 1);
    holders[0] = 2;
    wfg_on_wait(1, 101, holders, 1);

    if (wfg_detect(-1, cycle, &cycle_len) != -1) {
        return fail("long wait chain was reported as a cycle");
    }
    if (cycle_len != 0) {
        return fail("chain returned a cycle path");
    }

    wfg_destroy();
    return 0;
}

static int test_grant_and_release(void)
{
    int cycle[MAX_SESSIONS];
    int cycle_len = -1;
    int holders[1];

    wfg_init();

    holders[0] = 1;
    wfg_on_wait(0, 100, holders, 1);
    wfg_on_grant(0);

    if (wfg_detect(-1, cycle, &cycle_len) != -1) {
        return fail("stale outgoing edge remained after grant");
    }

    holders[0] = 2;
    wfg_on_wait(1, 101, holders, 1);
    wfg_on_release(2);

    if (wfg_detect(-1, cycle, &cycle_len) != -1) {
        return fail("stale incoming edge remained after release");
    }

    wfg_destroy();
    return 0;
}

static int test_self_edge_and_multiple_holders(void)
{
    int cycle[MAX_SESSIONS];
    int cycle_len = -1;
    int holders[3] = {4, 5, 4};

    wfg_init();

    wfg_on_wait(4, 100, holders, 3);
    if (wfg_detect(-1, cycle, &cycle_len) != -1) {
        return fail("self-edge or duplicate holder created a false cycle");
    }

    wfg_on_release(5);
    wfg_destroy();
    return 0;
}

static int test_stats(void)
{
    unsigned int detections = 0;
    unsigned int deadlocks = 0;

    wfg_init();
    wfg_detect(-1, NULL, NULL);
    wfg_stats(&detections, &deadlocks);

    if (detections != 1) {
        return fail("detection counter did not increment");
    }
    if (deadlocks != 0) {
        return fail("empty graph incremented deadlock counter");
    }

    wfg_destroy();
    return 0;
}

int main(void)
{
    if (test_empty_graph() != 0) {
        return 1;
    }
    if (test_two_cycle() != 0) {
        return 1;
    }
    if (test_three_cycle() != 0) {
        return 1;
    }
    if (test_chain_no_cycle() != 0) {
        return 1;
    }
    if (test_grant_and_release() != 0) {
        return 1;
    }
    if (test_self_edge_and_multiple_holders() != 0) {
        return 1;
    }
    if (test_stats() != 0) {
        return 1;
    }

    puts("deadlock graph tests passed");
    puts("2-cycle, 3-cycle, chain, grant/release, self-edge, and stats checks passed");
    return 0;
}

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "waitfor.h"

#define WFG_COLOR_WHITE 0
#define WFG_COLOR_GRAY 1
#define WFG_COLOR_BLACK 2

static unsigned char g_edges[MAX_SESSIONS][MAX_SESSIONS];
static unsigned char g_aborted[MAX_SESSIONS];
static unsigned int g_detection_count;
static unsigned int g_deadlock_count;
static pthread_mutex_t g_graph_mutex = PTHREAD_MUTEX_INITIALIZER;
static int g_initialized;

static int session_valid(int session)
{
    return session >= 0 && session < MAX_SESSIONS;
}

static int dfs_cycle(int start, int *color, int *parent,
                     int *path, int *path_len)
{
    int i;

    color[start] = WFG_COLOR_GRAY;

    for (i = 0; i < MAX_SESSIONS; i++) {
        if (!g_edges[start][i]) {
            continue;
        }

        if (color[i] == WFG_COLOR_WHITE) {
            parent[i] = start;
            if (dfs_cycle(i, color, parent, path, path_len)) {
                return 1;
            }
        } else if (color[i] == WFG_COLOR_GRAY) {
            int cycle_start;
            int pos;

            *path_len = 0;
            cycle_start = i;
            pos = start;

            while (pos != cycle_start && pos >= 0) {
                path[(*path_len)++] = pos;
                pos = parent[pos];
            }
            path[(*path_len)++] = cycle_start;

            if (*path_len > 1) {
                int left = 0;
                int right = *path_len - 1;
                while (left < right) {
                    int tmp = path[left];
                    path[left] = path[right];
                    path[right] = tmp;
                    left++;
                    right--;
                }
            }

            return 1;
        }
    }

    color[start] = WFG_COLOR_BLACK;
    return 0;
}

int wfg_init(void)
{
    if (g_initialized) {
        return 0;
    }

    pthread_mutex_lock(&g_graph_mutex);
    memset(g_edges, 0, sizeof(g_edges));
    memset(g_aborted, 0, sizeof(g_aborted));
    g_detection_count = 0;
    g_deadlock_count = 0;
    g_initialized = 1;
    pthread_mutex_unlock(&g_graph_mutex);
    return 0;
}

void wfg_destroy(void)
{
    if (!g_initialized) {
        return;
    }

    pthread_mutex_lock(&g_graph_mutex);
    memset(g_edges, 0, sizeof(g_edges));
    memset(g_aborted, 0, sizeof(g_aborted));
    g_initialized = 0;
    pthread_mutex_unlock(&g_graph_mutex);
}

void wfg_on_wait(int waiter, int file_id,
                 const int *holders, int n_holders)
{
    int i;

    (void)file_id;
    if (!g_initialized || !session_valid(waiter) || holders == NULL) {
        return;
    }

    pthread_mutex_lock(&g_graph_mutex);
    for (i = 0; i < n_holders; i++) {
        if (session_valid(holders[i]) && holders[i] != waiter) {
            g_edges[waiter][holders[i]] = 1;
        }
    }
    pthread_mutex_unlock(&g_graph_mutex);
}

void wfg_on_grant(int session)
{
    int i;

    if (!g_initialized || !session_valid(session)) {
        return;
    }

    pthread_mutex_lock(&g_graph_mutex);
    for (i = 0; i < MAX_SESSIONS; i++) {
        g_edges[session][i] = 0;
    }
    pthread_mutex_unlock(&g_graph_mutex);
}

void wfg_on_release(int session)
{
    int i;

    if (!g_initialized || !session_valid(session)) {
        return;
    }

    pthread_mutex_lock(&g_graph_mutex);
    for (i = 0; i < MAX_SESSIONS; i++) {
        g_edges[i][session] = 0;
    }
    pthread_mutex_unlock(&g_graph_mutex);
}

int wfg_detect(int start_session, int *cycle_path, int *cycle_len)
{
    int color[MAX_SESSIONS];
    int parent[MAX_SESSIONS];
    int path[MAX_SESSIONS];
    int path_len = 0;
    int victim = -1;
    int i;

    if (!g_initialized) {
        if (cycle_len != NULL) {
            *cycle_len = 0;
        }
        return -1;
    }

    pthread_mutex_lock(&g_graph_mutex);
    g_detection_count++;
    memset(color, 0, sizeof(color));
    for (i = 0; i < MAX_SESSIONS; i++) {
        parent[i] = -1;
    }

    if (session_valid(start_session)) {
        if (dfs_cycle(start_session, color, parent, path, &path_len)) {
            victim = 0;
        }
    } else {
        for (i = 0; i < MAX_SESSIONS; i++) {
            if (color[i] == WFG_COLOR_WHITE && dfs_cycle(i, color, parent, path, &path_len)) {
                victim = 0;
                break;
            }
        }
    }

    if (victim == 0) {
        int youngest = -1;
        g_deadlock_count++;

        for (i = 0; i < path_len; i++) {
            if (path[i] > youngest) {
                youngest = path[i];
            }
        }
        victim = youngest;

        if (cycle_path != NULL) {
            for (i = 0; i < path_len && i < MAX_SESSIONS; i++) {
                cycle_path[i] = path[i];
            }
        }
        if (cycle_len != NULL) {
            *cycle_len = path_len;
        }
    } else {
        if (cycle_len != NULL) {
            *cycle_len = 0;
        }
    }

    pthread_mutex_unlock(&g_graph_mutex);
    return victim;
}

void wfg_dump(void)
{
    int i;
    int j;
    int printed = 0;

    if (!g_initialized) {
        puts("wait-for graph: not initialized");
        return;
    }

    pthread_mutex_lock(&g_graph_mutex);
    for (i = 0; i < MAX_SESSIONS; i++) {
        for (j = 0; j < MAX_SESSIONS; j++) {
            if (g_edges[i][j]) {
                printf("session %d -> session %d\n", i, j);
                printed = 1;
            }
        }
    }
    pthread_mutex_unlock(&g_graph_mutex);

    if (!printed) {
        puts("wait-for graph: empty");
    }
}

int wfg_recover_victim(int victim)
{
    int i;

    if (!g_initialized || !session_valid(victim)) {
        return -EINVAL;
    }

    pthread_mutex_lock(&g_graph_mutex);
    g_aborted[victim] = 1;

    for (i = 0; i < MAX_SESSIONS; i++) {
        g_edges[victim][i] = 0;
        g_edges[i][victim] = 0;
    }
    pthread_mutex_unlock(&g_graph_mutex);

    return -EDEADLK;
}

int wfg_is_aborted(int session)
{
    int aborted;

    if (!session_valid(session)) {
        return 0;
    }

    pthread_mutex_lock(&g_graph_mutex);
    aborted = g_aborted[session] ? 1 : 0;
    pthread_mutex_unlock(&g_graph_mutex);
    return aborted;
}

void wfg_reset_abort(int session)
{
    if (!session_valid(session)) {
        return;
    }

    pthread_mutex_lock(&g_graph_mutex);
    g_aborted[session] = 0;
    pthread_mutex_unlock(&g_graph_mutex);
}

void wfg_stats(unsigned int *detections, unsigned int *deadlocks)
{
    pthread_mutex_lock(&g_graph_mutex);
    if (detections != NULL) {
        *detections = g_detection_count;
    }
    if (deadlocks != NULL) {
        *deadlocks = g_deadlock_count;
    }
    pthread_mutex_unlock(&g_graph_mutex);
}

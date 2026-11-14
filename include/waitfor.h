/*
 * waitfor.h - Wait-For Graph API (Deadlock Detection)
 * ----------------------------------------------------
 * This header defines the interface for the wait-for graph module used
 * to detect deadlocks among sessions (client threads) that hold and
 * request file locks.
 *
 * How it works (big picture):
 *   - The graph has one node per session (client thread).
 *   - An edge  A -> B  means "session A is waiting for a lock that
 *     session B currently holds."
 *   - A cycle in this graph means a deadlock: every session in the
 *     cycle is waiting for another, and none can make progress.
 *   - The lock manager (rwlock.c, written by Shwetank) calls four
 *     hook functions at the right moments so the graph stays up to
 *     date:
 *       wfg_on_wait    — a session starts waiting for a lock
 *       wfg_on_grant   — a session receives the lock it was waiting for
 *       wfg_on_release — a session releases a lock
 *   - wfg_detect() runs a DFS to find cycles.
 *   - wfg_dump()   prints the current graph (for debugging / reports).
 *
 * Data representation:
 *   Internally the graph is a MAX_SESSIONS x MAX_SESSIONS adjacency
 *   matrix (small, fixed-size, simple).  A single mutex protects it.
 *
 * Integration notes for Shwetank (rwlock.c):
 *   - Call wfg_on_wait()    BEFORE the session blocks on the lock.
 *   - Call wfg_on_grant()   AFTER  the lock is granted.
 *   - Call wfg_on_release() AFTER  the lock is released.
 *   - The wait must be abortable: use a timed wait or a condition
 *     variable with an abort flag, so the deadlock detector can wake
 *     and abort the victim session.
 *
 * Graph-node convention:
 *   Each node is identified by a session ID (an int, 0-based).
 *   An edge goes from the waiter to the holder.
 */

#ifndef WAITFOR_H   /* include guard */
#define WAITFOR_H

/* maximum number of concurrent sessions (graph size) */
#define MAX_SESSIONS 64

/* ---------------------------------------------------------------
 * wfg_init — initialise the wait-for graph
 *
 * Call once at program start.  Clears the adjacency matrix and
 * creates the internal mutex.
 *
 * Returns 0 on success, -1 on error.
 * --------------------------------------------------------------- */
int wfg_init(void);

/* ---------------------------------------------------------------
 * wfg_on_wait — a session starts waiting for a file lock
 *
 * Called by the lock manager just before a session blocks.
 *
 *   waiter   : session ID of the thread that wants the lock
 *   file_id  : which file the lock is on (used to look up holders)
 *   holders  : array of session IDs that currently hold the lock
 *   n_holders: number of entries in the holders array
 *
 * For each holder, an edge  waiter -> holder  is added to the graph
 * (self-edges are skipped — a session cannot deadlock with itself).
 * --------------------------------------------------------------- */
void wfg_on_wait(int waiter, int file_id,
                 const int *holders, int n_holders);

/* ---------------------------------------------------------------
 * wfg_on_grant — a session receives the lock it was waiting for
 *
 * Called by the lock manager right after a blocked session is woken
 * and given the lock.
 *
 *   session  : the session that just received the lock
 *
 * All edges FROM this session are removed, because it is no longer
 * waiting for anything.
 * --------------------------------------------------------------- */
void wfg_on_grant(int session);

/* ---------------------------------------------------------------
 * wfg_on_release — a session releases a lock
 *
 * Called by the lock manager after a session releases its lock.
 *
 *   session  : the session that released the lock
 *
 * All edges TO this session are removed, because nobody can be
 * waiting on a lock this session no longer holds.
 * --------------------------------------------------------------- */
void wfg_on_release(int session);

/* ---------------------------------------------------------------
 * wfg_detect — run deadlock detection (cycle detection)
 *
 * Performs a depth-first search on the graph.  If a cycle is found,
 * the function fills in the cycle path and returns the victim.
 *
 *   start_session : if >= 0, only check for cycles involving this
 *                   session (on-request mode — cheaper, used right
 *                   after a new wait edge is added).
 *                   if < 0, check all nodes (periodic mode).
 *
 *   cycle_path    : output array that receives the session IDs in the
 *                   detected cycle (caller allocates, at least
 *                   MAX_SESSIONS entries).
 *   cycle_len     : output — how many entries were written to cycle_path.
 *
 * Returns:
 *    >= 0 : session ID of the chosen victim (youngest session in the
 *           cycle — least work lost).
 *    -1   : no deadlock found.
 * --------------------------------------------------------------- */
int wfg_detect(int start_session, int *cycle_path, int *cycle_len);

/* ---------------------------------------------------------------
 * wfg_dump — print the current graph to stdout
 *
 * Intended for debugging and the project report.  Shows every
 * non-zero edge as  "session X -> session Y".  Prints nothing if
 * the graph is empty.
 * --------------------------------------------------------------- */
void wfg_dump(void);

/* ---------------------------------------------------------------
 * wfg_destroy — clean up the wait-for graph
 *
 * Destroys the mutex and resets the matrix.  Call at shutdown.
 * --------------------------------------------------------------- */
void wfg_destroy(void);

#endif /* WAITFOR_H */

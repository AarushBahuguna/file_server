#ifndef RWLOCK_H
#define RWLOCK_H
#define LOCK_READ 0
#define LOCK_WRITE 1
#define READER_PREFERENCE 0
#define WRITER_PREFERENCE 1
#define FAIR_POLICY 2
int lock_manager_init(void);
int lock_acquire(int file_id, int mode, int session);
void lock_release(int file_id, int session);
void lock_set_policy(int policy);
void lock_manager_destroy(void);
#endif

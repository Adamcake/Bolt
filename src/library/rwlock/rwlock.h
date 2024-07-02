#ifndef _BOLT_LIBRARY_RWLOCK_H_
#define _BOLT_LIBRARY_RWLOCK_H_

#if defined(_WIN32)
#include <windows.h>
typedef SRWLOCK RWLock;
#else
#include <pthread.h>
typedef pthread_rwlock_t RWLock;
#endif

void _bolt_rwlock_init(RWLock*);
void _bolt_rwlock_lock_read(RWLock*);
void _bolt_rwlock_lock_write(RWLock*);
void _bolt_rwlock_unlock_read(RWLock*);
void _bolt_rwlock_unlock_write(RWLock*);
void _bolt_rwlock_destroy(RWLock*);

#endif

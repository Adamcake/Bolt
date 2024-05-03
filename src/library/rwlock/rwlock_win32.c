#include "rwlock.h"

void _bolt_rwlock_init(RWLock* lock) {
    InitializeSRWLock(lock);
}

void _bolt_rwlock_lock_read(RWLock* lock) {
    AcquireSRWLockShared(lock);
}

void _bolt_rwlock_lock_write(RWLock* lock) {
    AcquireSRWLockExclusive(lock);
}

void _bolt_rwlock_unlock_read(RWLock* lock) {
    ReleaseSRWLockShared(lock);
}

void _bolt_rwlock_unlock_write(RWLock* lock) {
    ReleaseSRWLockExclusive(lock);
}

void _bolt_rwlock_destroy(RWLock* lock) {}

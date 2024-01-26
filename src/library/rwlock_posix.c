#include "rwlock.h"

void _bolt_rwlock_init(RWLock* lock) {
    pthread_rwlock_init(lock, NULL);
}

void _bolt_rwlock_lock_read(RWLock* lock) {
    pthread_rwlock_rdlock(lock);
}

void _bolt_rwlock_lock_write(RWLock* lock) {
    pthread_rwlock_wrlock(lock);
}

void _bolt_rwlock_unlock_read(RWLock* lock) {
    pthread_rwlock_unlock(lock);
}

void _bolt_rwlock_unlock_write(RWLock* lock) {
    pthread_rwlock_unlock(lock);
}

void _bolt_rwlock_destroy(RWLock* lock) {
    pthread_rwlock_destroy(lock);
}

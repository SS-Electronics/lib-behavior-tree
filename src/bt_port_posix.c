/**
 * @file  bt_port_posix.c
 * @brief POSIX pthreads port — uses pthread_mutex_t.
 *
 * Use when BT_PLATFORM == BT_PLATFORM_POSIX.
 * Link with -lpthread.
 */
#define _GNU_SOURCE        /* enables PTHREAD_MUTEX_RECURSIVE on strict C99 */
#include "bt_port.h"

#if (BT_PLATFORM == BT_PLATFORM_POSIX)

bool bt_port_mutex_create(bt_mutex_t *mutex)
{
    pthread_mutexattr_t attr;
    if (pthread_mutexattr_init(&attr) != 0) return false;

    /* Recursive mutex: bt_tree_tick may call back into the tree safely */
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    bool ok = (pthread_mutex_init(mutex, &attr) == 0);
    pthread_mutexattr_destroy(&attr);
    return ok;
}

void bt_port_mutex_destroy(bt_mutex_t *mutex)
{
    pthread_mutex_destroy(mutex);
}

void bt_port_mutex_lock(bt_mutex_t *mutex)
{
    pthread_mutex_lock(mutex);
}

void bt_port_mutex_unlock(bt_mutex_t *mutex)
{
    pthread_mutex_unlock(mutex);
}

#endif /* BT_PLATFORM_POSIX */

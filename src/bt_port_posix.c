/**
 * @file   bt_port_posix.c
 * @brief  POSIX pthreads port — recursive mutex via `pthread_mutex_t`.
 * @author Subhajit Roy <subhajitroy005@gmail.com>
 *
 * @ingroup BT_PORT
 *
 * @details
 * Compile this file when `BT_PLATFORM == BT_PLATFORM_POSIX`.
 * Link your application with `-lpthread`.
 *
 * A **recursive** mutex attribute is set so that callbacks executing inside
 * `bt_tree_tick` can safely call library utilities that also acquire the
 * mutex (e.g., a custom composite calling `bt_node_tick` on a sub-tree).
 *
 * **Feature-test note:** `pthread_mutexattr_settype` and
 * `PTHREAD_MUTEX_RECURSIVE` are XSI extensions and require either
 * `-D_POSIX_C_SOURCE=200809L` or `-D_GNU_SOURCE` on the compiler command
 * line when building with `-std=c99`.  The example Makefile already includes
 * this flag.
 */
#define _GNU_SOURCE   /* ensures PTHREAD_MUTEX_RECURSIVE is visible */
#include "bt_port.h"

#if (BT_PLATFORM == BT_PLATFORM_POSIX)

/**
 * @brief Initialise a recursive POSIX mutex.
 *
 * @param[out] mutex  Pointer to the `pthread_mutex_t` to initialise.
 * @return @c true on success; @c false if `pthread_mutex_init` fails.
 */
bool bt_port_mutex_create(bt_mutex_t *mutex)
{
    pthread_mutexattr_t attr;
    if (pthread_mutexattr_init(&attr) != 0) return false;
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    bool ok = (pthread_mutex_init(mutex, &attr) == 0);
    pthread_mutexattr_destroy(&attr);
    return ok;
}

/**
 * @brief Destroy a POSIX mutex and release its resources.
 *
 * @param[in,out] mutex  Mutex to destroy.
 */
void bt_port_mutex_destroy(bt_mutex_t *mutex)
{
    pthread_mutex_destroy(mutex);
}

/**
 * @brief Acquire the mutex (blocking).
 *
 * @param[in,out] mutex  Mutex to lock.
 */
void bt_port_mutex_lock(bt_mutex_t *mutex)
{
    pthread_mutex_lock(mutex);
}

/**
 * @brief Release the mutex.
 *
 * @param[in,out] mutex  Mutex to unlock.
 */
void bt_port_mutex_unlock(bt_mutex_t *mutex)
{
    pthread_mutex_unlock(mutex);
}

#endif /* BT_PLATFORM_POSIX */

/**
 * @file   bt_port_bare.c
 * @brief  Bare-metal port — all mutex operations are no-ops.
 * @author Subhajit Roy <subhajitroy005@gmail.com>
 *
 * @ingroup BT_PORT
 *
 * @details
 * Use when `BT_PLATFORM == BT_PLATFORM_BARE` (i.e., a single-threaded
 * environment with no RTOS).  Thread safety is the caller's responsibility;
 * it is guaranteed simply by never calling `bt_tree_tick` from more than
 * one context simultaneously.
 *
 * @note Passing `thread_safe = true` to @ref bt_tree_create is valid even
 *       with this port — the mutex functions are simply no-ops, so there is
 *       no runtime overhead.
 */
#include "bt_port.h"

#if (BT_PLATFORM == BT_PLATFORM_BARE)

/**
 * @brief No-op mutex creation.
 * @return Always returns @c true.
 */
bool bt_port_mutex_create (bt_mutex_t *mutex) { (void)mutex; return true; }

/**
 * @brief No-op mutex destruction.
 */
void bt_port_mutex_destroy(bt_mutex_t *mutex) { (void)mutex; }

/**
 * @brief No-op mutex lock.
 */
void bt_port_mutex_lock   (bt_mutex_t *mutex) { (void)mutex; }

/**
 * @brief No-op mutex unlock.
 */
void bt_port_mutex_unlock (bt_mutex_t *mutex) { (void)mutex; }

#endif /* BT_PLATFORM_BARE */

/**
 * @file  bt_port_bare.c
 * @brief Bare-metal port — mutex operations are no-ops.
 *
 * Use when BT_PLATFORM == BT_PLATFORM_BARE (single-threaded, no RTOS).
 * Thread safety is the caller's responsibility.
 */
#include "bt_port.h"

#if (BT_PLATFORM == BT_PLATFORM_BARE)

bool bt_port_mutex_create (bt_mutex_t *mutex) { (void)mutex; return true; }
void bt_port_mutex_destroy(bt_mutex_t *mutex) { (void)mutex; }
void bt_port_mutex_lock   (bt_mutex_t *mutex) { (void)mutex; }
void bt_port_mutex_unlock (bt_mutex_t *mutex) { (void)mutex; }

#endif /* BT_PLATFORM_BARE */

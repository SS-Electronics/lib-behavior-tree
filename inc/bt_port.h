/**
 * @file  bt_port.h
 * @brief Platform-abstraction layer: mutex type and operations.
 *
 * This header is included by behavior_tree.h and should not be included
 * directly by application code.
 *
 * Platform constants are defined FIRST so that conf_behaviour_tree.h can
 * reference them symbolically when selecting BT_PLATFORM.
 */
#ifndef BT_PORT_H
#define BT_PORT_H

#include <stdbool.h>
#include <stdlib.h>

/* ---------------------------------------------------------------------------
 *  Platform constants — available before conf_behaviour_tree.h is included
 * ------------------------------------------------------------------------- */
#define BT_PLATFORM_BARE        0  /**< No RTOS, no thread safety            */
#define BT_PLATFORM_POSIX       1  /**< POSIX pthreads                       */
#define BT_PLATFORM_FREERTOS    2  /**< FreeRTOS semphr.h                    */

/* Pull in the project-specific configuration */
#include "conf_behaviour_tree.h"

/* ---------------------------------------------------------------------------
 *  Default fallbacks (in case conf_behaviour_tree.h omits a setting)
 * ------------------------------------------------------------------------- */
#ifndef BT_PLATFORM
#  define BT_PLATFORM                  BT_PLATFORM_BARE
#endif

#ifndef BT_CHILDREN_INITIAL_CAPACITY
#  define BT_CHILDREN_INITIAL_CAPACITY 4
#endif

#ifndef BT_ENABLE_NODE_NAMES
#  define BT_ENABLE_NODE_NAMES         1
#endif

#ifndef BT_ENABLE_STATS
#  define BT_ENABLE_STATS              1
#endif

/* ---------------------------------------------------------------------------
 *  Memory allocator
 * ------------------------------------------------------------------------- */
#ifndef BT_MALLOC
#  define BT_MALLOC    malloc
#endif

#ifndef BT_FREE
#  define BT_FREE      free
#endif

/* ---------------------------------------------------------------------------
 *  Mutex type — selected by BT_PLATFORM
 * ------------------------------------------------------------------------- */
#if   (BT_PLATFORM == BT_PLATFORM_FREERTOS)
#  include "FreeRTOS.h"
#  include "semphr.h"
   typedef SemaphoreHandle_t   bt_mutex_t;

#elif (BT_PLATFORM == BT_PLATFORM_POSIX)
#  include <pthread.h>
   typedef pthread_mutex_t     bt_mutex_t;

#else  /* BT_PLATFORM_BARE — mutex operations are no-ops */
   typedef int                 bt_mutex_t;
#endif

/* ---------------------------------------------------------------------------
 *  Port function prototypes
 *  Implemented in src/bt_port_<platform>.c
 * ------------------------------------------------------------------------- */

/**
 * bt_port_mutex_create - Initialise a mutex.
 * @return true on success, false on failure.
 */
bool bt_port_mutex_create (bt_mutex_t *mutex);

/** bt_port_mutex_destroy - Release all resources held by a mutex. */
void bt_port_mutex_destroy(bt_mutex_t *mutex);

/** bt_port_mutex_lock   - Acquire the mutex (blocking). */
void bt_port_mutex_lock   (bt_mutex_t *mutex);

/** bt_port_mutex_unlock - Release the mutex. */
void bt_port_mutex_unlock (bt_mutex_t *mutex);

#endif /* BT_PORT_H */

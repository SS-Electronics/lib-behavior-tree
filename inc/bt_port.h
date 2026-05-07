/**
 * @file   bt_port.h
 * @brief  Platform-abstraction layer: mutex type selection and port prototypes.
 * @author Subhajit Roy <subhajitroy005@gmail.com>
 *
 * @defgroup BT_PORT Platform Port
 * @{
 *
 * This header is included by `behavior_tree.h` and should not be included
 * directly by application code.
 *
 * **How it works:**
 * 1. Platform ID constants (BT_PLATFORM_BARE, etc.) are defined first.
 * 2. `<conf_behaviour_tree.h>` (angle-brackets so the project's version takes
 *    priority over the library's template) is included for user settings.
 * 3. Based on the selected @c BT_PLATFORM, the correct @c bt_mutex_t typedef
 *    and the required RTOS headers are pulled in.
 * 4. The four port functions are declared; exactly one of
 *    `src/bt_port_bare.c`, `src/bt_port_posix.c`, or
 *    `src/bt_port_freertos.c` must be compiled.
 *
 * **Include path rule:** place your project's `conf_behaviour_tree.h`
 * directory **before** `lib-behavior-tree/inc/` in your compiler's -I list:
 * @code
 *   gcc ... -I<project>/include  -Ilib-behavior-tree/inc ...
 * @endcode
 */
#ifndef BT_PORT_H
#define BT_PORT_H

#include <stdbool.h>
#include <stdlib.h>

/* ===========================================================================
 *  Platform constants — defined BEFORE conf_behaviour_tree.h is included so
 *  that the user can reference them symbolically in their config file.
 * ========================================================================= */

/** @brief Bare-metal target — no RTOS, single-threaded, mutex is a no-op. */
#define BT_PLATFORM_BARE        0

/** @brief POSIX target — POSIX pthreads recursive mutex. */
#define BT_PLATFORM_POSIX       1

/** @brief FreeRTOS target — xSemaphoreCreateRecursiveMutex. */
#define BT_PLATFORM_FREERTOS    2

/* Pull in the project-specific configuration.
 * Angle brackets ensure the compiler searches -I paths in order, so a
 * project that places its own conf_behaviour_tree.h in a directory listed
 * BEFORE lib-behavior-tree/inc/ will have its version found first. */
#include <conf_behaviour_tree.h>

/* ===========================================================================
 *  Default fallbacks (in case conf_behaviour_tree.h omits a setting)
 * ========================================================================= */

/** @cond INTERNAL */
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
/** @endcond */

/* ===========================================================================
 *  Memory allocator
 * ========================================================================= */

/**
 * @brief Allocate @p n bytes of memory.
 *
 * Defaults to the standard @c malloc.  Override in `conf_behaviour_tree.h`
 * to use an RTOS heap (e.g., @c pvPortMalloc) or a custom pool allocator.
 */
#ifndef BT_MALLOC
#  define BT_MALLOC    malloc
#endif

/**
 * @brief Free a previously BT_MALLOC-allocated block.
 *
 * Defaults to the standard @c free.  Override to match your @c BT_MALLOC.
 */
#ifndef BT_FREE
#  define BT_FREE      free
#endif

/* ===========================================================================
 *  Mutex type — selected by BT_PLATFORM
 * ========================================================================= */

#if   (BT_PLATFORM == BT_PLATFORM_FREERTOS)
/**
 * @brief FreeRTOS recursive-mutex handle.
 * Implemented in `src/bt_port_freertos.c`.
 */
#  include "FreeRTOS.h"
#  include "semphr.h"
   typedef SemaphoreHandle_t   bt_mutex_t;

#elif (BT_PLATFORM == BT_PLATFORM_POSIX)
/**
 * @brief POSIX recursive mutex.
 * Implemented in `src/bt_port_posix.c`.
 */
#  include <pthread.h>
   typedef pthread_mutex_t     bt_mutex_t;

#else
/**
 * @brief Dummy mutex type for bare-metal (no locking performed).
 * Implemented in `src/bt_port_bare.c`.
 */
   typedef int                 bt_mutex_t;
#endif

/* ===========================================================================
 *  Port function prototypes
 * ========================================================================= */

/**
 * @brief Initialise a platform mutex.
 *
 * Called by @ref bt_tree_create when `thread_safe = true`.
 *
 * @param[out] mutex  Pointer to the mutex object to initialise.
 * @return @c true on success, @c false on failure.
 */
bool bt_port_mutex_create (bt_mutex_t *mutex);

/**
 * @brief Release all resources held by a mutex.
 *
 * Called by @ref bt_tree_destroy.
 *
 * @param[in,out] mutex  Mutex to destroy.
 */
void bt_port_mutex_destroy(bt_mutex_t *mutex);

/**
 * @brief Acquire the mutex (blocking).
 *
 * Used internally by @ref bt_tree_tick and @ref bt_tree_destroy.
 *
 * @param[in,out] mutex  Mutex to lock.
 */
void bt_port_mutex_lock   (bt_mutex_t *mutex);

/**
 * @brief Release the mutex.
 *
 * @param[in,out] mutex  Mutex to unlock.
 */
void bt_port_mutex_unlock (bt_mutex_t *mutex);

/** @} */ /* BT_PORT */

#endif /* BT_PORT_H */

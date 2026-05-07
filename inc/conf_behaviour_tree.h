/**
 * @file   conf_behaviour_tree.h
 * @brief  lib-behavior-tree user configuration template.
 * @author Subhajit Roy <subhajitroy005@gmail.com>
 *
 * @defgroup BT_CONFIG Configuration
 * @{
 *
 * **How to use this file:**
 * 1. Copy this file to your project's include directory.
 * 2. Edit the settings below.
 * 3. Ensure your project's include directory appears **before**
 *    `lib-behavior-tree/inc/` in the compiler's -I path.
 *
 * The library includes this file with angle brackets (`<conf_behaviour_tree.h>`)
 * so that your project-specific copy takes precedence over this template.
 *
 * Platform constants `BT_PLATFORM_BARE`, `BT_PLATFORM_POSIX`, and
 * `BT_PLATFORM_FREERTOS` are defined by `bt_port.h` **before** this file is
 * included, so you can reference them symbolically here.
 */
#ifndef CONF_BEHAVIOUR_TREE_H
#define CONF_BEHAVIOUR_TREE_H

/* ===========================================================================
 *  Platform / RTOS selection
 *  Choose exactly one of:
 *    BT_PLATFORM_BARE      – No RTOS.  Single-threaded.  Mutex is a no-op.
 *                            Include src/bt_port_bare.c in your build.
 *    BT_PLATFORM_POSIX     – POSIX pthreads (Linux, macOS, Zephyr POSIX …).
 *                            Include src/bt_port_posix.c; link with -lpthread.
 *    BT_PLATFORM_FREERTOS  – FreeRTOS (any MCU with a FreeRTOS port).
 *                            Include src/bt_port_freertos.c.
 *                            Requires configUSE_RECURSIVE_MUTEXES = 1.
 * ========================================================================= */

/**
 * @brief Target platform identifier.
 *
 * Set to BT_PLATFORM_BARE, BT_PLATFORM_POSIX, or BT_PLATFORM_FREERTOS.
 */
#define BT_PLATFORM    BT_PLATFORM_POSIX

/* ===========================================================================
 *  Memory allocator overrides
 *
 *  Uncomment and set these to route all library allocations through the
 *  RTOS heap or a custom memory pool.  Both macros must use the same
 *  heap (a block allocated with BT_MALLOC must be freeable with BT_FREE).
 * ========================================================================= */

/**
 * @brief Override the default @c malloc.
 *
 * Example — FreeRTOS heap:
 * @code
 *   #define BT_MALLOC    pvPortMalloc
 * @endcode
 */
/* #define BT_MALLOC    pvPortMalloc */

/**
 * @brief Override the default @c free.
 *
 * Example — FreeRTOS heap:
 * @code
 *   #define BT_FREE      vPortFree
 * @endcode
 */
/* #define BT_FREE      vPortFree   */

/* ===========================================================================
 *  Child-array initial capacity
 *
 *  Composite nodes (Sequence, Selector, Parallel) allocate a small array
 *  for child pointers.  The array doubles when full.  Setting this to the
 *  typical fanout of your tree avoids any reallocation.
 * ========================================================================= */

/**
 * @brief Initial capacity of the children pointer array per composite node.
 *
 * Tune to your tree's typical branching factor.
 * Minimum useful value: 2.  Default: 4.
 */
#define BT_CHILDREN_INITIAL_CAPACITY    4

/* ===========================================================================
 *  Optional features
 * ========================================================================= */

/**
 * @brief Enable human-readable node names.
 *
 * When 1, each `bt_node_t` stores a @c const @c char* name pointer (one
 * pointer of overhead per node).  Set to 0 to strip names from production
 * builds and reduce per-node memory.
 */
#define BT_ENABLE_NODE_NAMES    1

/**
 * @brief Enable per-tree tick statistics.
 *
 * When 1, @ref bt_tree_t gains @c tick_count (uint32_t) and
 * @c last_status (bt_status_t) fields updated on every @ref bt_tree_tick.
 * Useful for watchdog monitoring and performance profiling.
 */
#define BT_ENABLE_STATS    1

/** @} */ /* BT_CONFIG */

#endif /* CONF_BEHAVIOUR_TREE_H */

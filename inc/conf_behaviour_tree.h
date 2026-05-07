/**
 * @file  conf_behaviour_tree.h
 * @brief User configuration template for lib-behavior-tree.
 *
 * Copy this file into your project's include path and adjust settings.
 * It is included by bt_port.h after platform constants are defined, so
 * BT_PLATFORM_BARE / BT_PLATFORM_POSIX / BT_PLATFORM_FREERTOS are already
 * available when this file is processed.
 */
#ifndef CONF_BEHAVIOUR_TREE_H
#define CONF_BEHAVIOUR_TREE_H

/* ---------------------------------------------------------------------------
 *  Platform / RTOS selection
 *  Choose exactly one of:
 *    BT_PLATFORM_BARE      – No RTOS, single-threaded, mutex is a no-op.
 *    BT_PLATFORM_POSIX     – POSIX pthreads (Linux, macOS, POSIX RTOS).
 *    BT_PLATFORM_FREERTOS  – FreeRTOS SemaphoreHandle_t mutex.
 * ------------------------------------------------------------------------- */
#define BT_PLATFORM    BT_PLATFORM_POSIX

/* ---------------------------------------------------------------------------
 *  Memory allocator
 *  Override to use an RTOS heap, memory pool, or custom allocator.
 *  Both must be thread-safe if BT_PLATFORM != BT_PLATFORM_BARE.
 *
 *  FreeRTOS example:
 *    #define BT_MALLOC    pvPortMalloc
 *    #define BT_FREE      vPortFree
 * ------------------------------------------------------------------------- */
/* #define BT_MALLOC    pvPortMalloc */
/* #define BT_FREE      vPortFree   */

/* ---------------------------------------------------------------------------
 *  Initial child-array capacity for composite nodes (Sequence/Selector/
 *  Parallel).  The array grows automatically (doubles) when full.
 *  Tune to your typical fanout to avoid realloc on small MCUs.
 * ------------------------------------------------------------------------- */
#define BT_CHILDREN_INITIAL_CAPACITY    4

/* ---------------------------------------------------------------------------
 *  Node names
 *  When enabled each node stores a const char* name pointer (one pointer
 *  overhead per node).  Disable for production builds on memory-constrained
 *  targets.
 * ------------------------------------------------------------------------- */
#define BT_ENABLE_NODE_NAMES    1

/* ---------------------------------------------------------------------------
 *  Tick-count and last-status statistics stored in bt_tree_t.
 *  Disable to save a few bytes per tree on very tight targets.
 * ------------------------------------------------------------------------- */
#define BT_ENABLE_STATS    1

#endif /* CONF_BEHAVIOUR_TREE_H */

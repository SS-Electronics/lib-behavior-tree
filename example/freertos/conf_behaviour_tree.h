/**
 * @file   conf_behaviour_tree.h  (FreeRTOS example)
 * @brief  lib-behavior-tree configuration for the FreeRTOS motor-controller
 *         example.  This file is project-specific and lives in the example
 *         directory rather than in lib-behavior-tree/inc/.
 * @author Subhajit Roy <subhajitroy005@gmail.com>
 */
#ifndef CONF_BEHAVIOUR_TREE_H
#define CONF_BEHAVIOUR_TREE_H

/* Use the FreeRTOS platform port (src/bt_port_freertos.c). */
#define BT_PLATFORM    BT_PLATFORM_FREERTOS

/* Route allocations through the FreeRTOS heap.
 * pvPortMalloc / vPortFree are provided by freertos_sim.h on POSIX and by
 * the real FreeRTOS heap implementation on an embedded target. */
#define BT_MALLOC    pvPortMalloc
#define BT_FREE      vPortFree

/* Motor controller tree rarely exceeds 4 children per composite node. */
#define BT_CHILDREN_INITIAL_CAPACITY    4

/* Keep node names in debug builds; strip them in production. */
#ifndef NDEBUG
#  define BT_ENABLE_NODE_NAMES    1
#else
#  define BT_ENABLE_NODE_NAMES    0
#endif

/* Enable tick statistics (useful for monitoring task timing). */
#define BT_ENABLE_STATS    1

#endif /* CONF_BEHAVIOUR_TREE_H */

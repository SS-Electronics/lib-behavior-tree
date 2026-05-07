/**
 * @file  behavior_tree.h
 * @brief lib-behavior-tree — generic, thread-safe behaviour tree for C.
 *
 * This is the only header application code needs to include.
 *
 * Quick-start:
 *   1. Copy conf_behaviour_tree.h into your project and adjust settings.
 *   2. Add inc/ to your include paths.
 *   3. Compile src/behavior_tree.c, src/bt_composite.c, src/bt_decorator.c
 *      and exactly one src/bt_port_<platform>.c.
 *   4. Include this header and build your tree with the factory functions.
 */
#ifndef BEHAVIOR_TREE_H
#define BEHAVIOR_TREE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "bt_port.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ===========================================================================
 *  Enumerations
 * ========================================================================= */

/** Node execution status returned by every tick. */
typedef enum {
    BT_SUCCESS = 0,   /**< Node completed successfully                      */
    BT_FAILURE = 1,   /**< Node completed with failure                      */
    BT_RUNNING = 2,   /**< Node is still executing (long-running action)    */
    BT_INVALID = 3,   /**< Node has not been ticked yet / was halted        */
    BT_ERROR   = 4,   /**< Internal error (e.g. allocation failure)         */
} bt_status_t;

/** Structural role of a node in the tree. */
typedef enum {
    /* Leaf nodes */
    BT_NODE_ACTION           = 0,
    BT_NODE_CONDITION        = 1,
    /* Composite nodes */
    BT_NODE_SEQUENCE         = 2,  /**< AND  – succeeds if all children succeed  */
    BT_NODE_SELECTOR         = 3,  /**< OR   – succeeds if any child succeeds    */
    BT_NODE_PARALLEL         = 4,  /**< ALL  – runs all children simultaneously  */
    /* Decorator nodes */
    BT_NODE_INVERTER         = 5,  /**< Flips SUCCESS<->FAILURE                 */
    BT_NODE_REPEATER         = 6,  /**< Loops child N times (or forever)        */
    BT_NODE_RETRY            = 7,  /**< Retries child on FAILURE up to N times  */
    BT_NODE_FORCE_SUCCESS    = 8,  /**< Always returns SUCCESS                  */
    BT_NODE_FORCE_FAILURE    = 9,  /**< Always returns FAILURE                  */
} bt_node_type_t;

/* ===========================================================================
 *  Callback types
 * ========================================================================= */

struct bt_node;

/**
 * bt_tick_fn_t - Main execution callback.
 * Must return BT_SUCCESS, BT_FAILURE, or BT_RUNNING.
 * Should never block; use BT_RUNNING to indicate work in progress.
 */
typedef bt_status_t (*bt_tick_fn_t)   (struct bt_node *node, void *context);

/**
 * bt_on_init_fn_t - Called once at the start of each new execution cycle.
 * Use this to reset node-local state before execution begins.
 */
typedef void        (*bt_on_init_fn_t)(struct bt_node *node, void *context);

/**
 * bt_on_halt_fn_t - Called when the node is forcibly interrupted.
 * Use this to clean up ongoing operations (cancel timers, release locks …).
 */
typedef void        (*bt_on_halt_fn_t)(struct bt_node *node, void *context);

/* ===========================================================================
 *  Node structure
 * ========================================================================= */

typedef struct bt_node {
    bt_node_type_t    type;
    bt_status_t       status;

    /* --- Tree hierarchy --- */
    struct bt_node   *parent;
    struct bt_node  **children;        /**< Dynamic array of child pointers   */
    uint16_t          child_count;
    uint16_t          child_capacity;

    /* --- User callbacks --- */
    bt_tick_fn_t      tick_fn;
    bt_on_init_fn_t   on_init_fn;
    bt_on_halt_fn_t   on_halt_fn;

    /* --- User payload --- */
    void             *user_data;

    /* --- Internal execution state --- */
    bool              initialized;     /**< on_init called for current cycle  */
    uint16_t          running_child;   /**< Memory composite: resume index    */

    /* --- Decorator / parallel parameters --- */
    int32_t           repeat_count;    /**< Current repeat / retry counter    */
    int32_t           max_count;       /**< Limit (-1 = infinite)             */
    uint16_t          success_threshold; /**< Parallel: min successes needed  */

#if BT_ENABLE_NODE_NAMES
    const char       *name;            /**< Human-readable label (debug only) */
#endif
} bt_node_t;

/* ===========================================================================
 *  Tree structure
 * ========================================================================= */

typedef struct bt_tree {
    bt_node_t  *root;
    void       *context;     /**< Passed unchanged to every callback          */
    bt_mutex_t  mutex;
    bool        thread_safe;

#if BT_ENABLE_STATS
    uint32_t    tick_count;   /**< Total number of bt_tree_tick calls         */
    bt_status_t last_status;  /**< Status returned by the most recent tick   */
#endif
} bt_tree_t;

/* ===========================================================================
 *  Tree API
 * ========================================================================= */

/**
 * bt_tree_create - Allocate and initialise a behaviour tree.
 *
 * @param root         Root node, already built with factory / add_child calls.
 * @param context      Opaque user pointer forwarded to every callback.
 * @param thread_safe  When true a mutex is created; bt_tree_tick is safe to
 *                     call from multiple tasks (e.g. one tick task + one
 *                     configuration task).
 * @return New tree pointer, or NULL if allocation fails.
 */
bt_tree_t  *bt_tree_create (bt_node_t *root, void *context, bool thread_safe);

/**
 * bt_tree_destroy - Halt the tree, free all nodes, then free the tree.
 */
void        bt_tree_destroy(bt_tree_t *tree);

/**
 * bt_tree_tick - Execute one tick of the behaviour tree.
 *
 * Call this periodically from your control loop or RTOS task.
 * Thread-safe when the tree was created with thread_safe = true.
 *
 * @return Root node status after this tick.
 */
bt_status_t bt_tree_tick   (bt_tree_t *tree);

/* ===========================================================================
 *  Node factory helpers
 * ========================================================================= */

/**
 * bt_action_create - Create a leaf action node.
 *
 * @param name        Debug label (stored by pointer; pass a string literal).
 * @param tick_fn     Mandatory tick callback.
 * @param on_init_fn  Called on first tick of each execution cycle (optional).
 * @param on_halt_fn  Called when the node is interrupted (optional).
 * @param user_data   Forwarded through node->user_data.
 */
bt_node_t *bt_action_create   (const char     *name,
                                bt_tick_fn_t    tick_fn,
                                bt_on_init_fn_t on_init_fn,
                                bt_on_halt_fn_t on_halt_fn,
                                void           *user_data);

/**
 * bt_condition_create - Create a leaf condition node.
 * tick_fn must return BT_SUCCESS or BT_FAILURE (never BT_RUNNING).
 */
bt_node_t *bt_condition_create(const char     *name,
                                bt_tick_fn_t    tick_fn,
                                void           *user_data);

/** bt_sequence_create - AND composite: succeeds only if all children succeed. */
bt_node_t *bt_sequence_create (const char *name);

/** bt_selector_create - OR composite: succeeds when the first child succeeds. */
bt_node_t *bt_selector_create (const char *name);

/**
 * bt_parallel_create - ALL composite: ticks all children every tick.
 *
 * @param success_threshold  Number of children that must succeed for the
 *                           parallel to return SUCCESS.  The node returns
 *                           FAILURE when (child_count - failures) <
 *                           success_threshold, making success impossible.
 */
bt_node_t *bt_parallel_create (const char *name, uint16_t success_threshold);

/** bt_inverter_create - Decorator: flips SUCCESS<->FAILURE; passes RUNNING. */
bt_node_t *bt_inverter_create     (const char *name);

/**
 * bt_repeater_create - Decorator: repeats child up to max_count times.
 * @param max_count  -1 repeats indefinitely; otherwise repeats exactly N times.
 */
bt_node_t *bt_repeater_create     (const char *name, int32_t max_count);

/**
 * bt_retry_create - Decorator: retries child on FAILURE up to max_retries.
 * @param max_retries  -1 retries indefinitely; otherwise up to N attempts.
 */
bt_node_t *bt_retry_create        (const char *name, int32_t max_retries);

/** bt_force_success_create - Decorator: maps all terminal states to SUCCESS. */
bt_node_t *bt_force_success_create(const char *name);

/** bt_force_failure_create - Decorator: maps all terminal states to FAILURE. */
bt_node_t *bt_force_failure_create(const char *name);

/* ===========================================================================
 *  Node manipulation
 * ========================================================================= */

/**
 * bt_node_add_child - Append a child to a composite or decorator node.
 * The child array grows automatically.
 * @return BT_SUCCESS, or BT_ERROR if allocation fails.
 */
bt_status_t bt_node_add_child(bt_node_t *parent, bt_node_t *child);

/**
 * bt_node_destroy - Recursively free a node and all of its descendants.
 * Halt the tree (or the sub-tree) before calling this; on_halt is NOT invoked.
 */
void        bt_node_destroy  (bt_node_t *node);

/* ===========================================================================
 *  Low-level node execution  (useful for custom composite/decorator types)
 * ========================================================================= */

/**
 * bt_node_tick - Tick a single node.
 * Calls on_init on the first tick of a new execution cycle, then tick_fn.
 * Resets the initialized flag when tick_fn returns a terminal status.
 */
bt_status_t bt_node_tick(bt_node_t *node, void *context);

/**
 * bt_node_halt - Forcibly interrupt a node and all of its running descendants.
 * Recursively calls on_halt callbacks and resets execution state.
 * No-op if the node is not currently initialized (not mid-cycle).
 */
void        bt_node_halt(bt_node_t *node, void *context);

/* ===========================================================================
 *  Utilities
 * ========================================================================= */

/** bt_status_str    - Return a human-readable name for a bt_status_t value. */
const char *bt_status_str   (bt_status_t    status);

/** bt_node_type_str - Return a human-readable name for a bt_node_type_t. */
const char *bt_node_type_str(bt_node_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* BEHAVIOR_TREE_H */

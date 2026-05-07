/**
 * @file  behavior_tree.h
 * @brief lib-behavior-tree — public API.
 *        See README.md (Doxygen mainpage) for the full architecture overview.
 *
 * @note Only this header needs to be included by application code.
 *
 * **lib-behavior-tree** is a generic, thread-safe, dynamically-allocated
 * Behaviour Tree library written in pure C99.  It targets embedded systems
 * running bare-metal, FreeRTOS, or any POSIX-compatible RTOS.
 *
 * @section arch_sec Architecture
 *
 * @subsection tree_sec Tree structure
 * A tree is a directed acyclic graph rooted at a single `bt_node_t`.
 * Composite and decorator nodes own a dynamic array of child pointers.
 * Leaf nodes (Action, Condition) have no children but carry user callbacks.
 *
 * @code
 *   bt_tree_t
 *   └── root : bt_node_t *
 *       ├── type          (BT_NODE_SEQUENCE, BT_NODE_SELECTOR, …)
 *       ├── status        (BT_SUCCESS / BT_FAILURE / BT_RUNNING / BT_INVALID)
 *       ├── children[]    (dynamic array, grows on demand)
 *       ├── tick_fn       (composite/decorator built-in, or user leaf fn)
 *       ├── on_init_fn    (called once at start of each execution cycle)
 *       ├── on_halt_fn    (called when node is forcibly interrupted)
 *       └── user_data     (opaque payload forwarded to every callback)
 * @endcode
 *
 * @subsection lifecycle_sec Node lifecycle
 * @code
 *   NOT_STARTED ──bt_node_tick()──> on_init() called → tick_fn()
 *                                          │
 *              ┌───────────────────────────┼──────────────────────┐
 *           RUNNING                    SUCCESS                 FAILURE
 *     (initialized=true)          (initialized=false)   (initialized=false)
 *              │                           │                      │
 *      next tick resumes            next tick calls         next tick calls
 *      without on_init              on_init again           on_init again
 *              │
 *   bt_node_halt() ──> on_halt() called, initialized=false, status=INVALID
 * @endcode
 *
 * @subsection composite_sec Composite node policies
 * - **Sequence** and **Selector**: stateful/memory policy — resumes from the
 *   last RUNNING child rather than restarting from child 0 each tick.  This
 *   avoids re-executing completed actions.
 * - **Parallel**: fully reactive — all children are ticked on every call.
 *   Use Parallel to build monitoring patterns: place conditions alongside a
 *   long-running action; if any condition fails, the Parallel fails and the
 *   action is halted immediately.
 *
 * @subsection thread_sec Thread safety
 * Each tree optionally owns a recursive mutex (`bt_port.h` supplies the
 * platform-specific type and operations).  When `thread_safe = true` is
 * passed to `bt_tree_create()`, `bt_tree_tick()` holds the mutex for the
 * entire duration of the tick.  Node creation, `bt_node_add_child()`, and
 * tree construction are NOT protected — build the tree before sharing it.
 *
 * @subsection memory_sec Memory
 * All allocations go through `BT_MALLOC` / `BT_FREE`, overridable in
 * `conf_behaviour_tree.h`.  A `bt_node_t` is ~60–72 bytes.
 *
 * @section usage_sec Quick-start
 * 1. Copy `inc/conf_behaviour_tree.h` to your project's include path.
 * 2. Set `BT_PLATFORM` and optionally `BT_MALLOC` / `BT_FREE`.
 * 3. Add `inc/` to your compiler's include search path.
 * 4. Compile `src/behavior_tree.c`, `src/bt_composite.c`,
 *    `src/bt_decorator.c`, and exactly one `src/bt_port_<platform>.c`.
 * 5. `#include "behavior_tree.h"` and use the factory functions.
 *
 * @author  Subhajit Roy <subhajitroy005@gmail.com>
 * @version 1.0.0
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

/**
 * @defgroup BT_TYPES Types and enumerations
 * @{
 */

/**
 * @brief Execution status returned by every node tick.
 *
 * Leaf callbacks must return one of BT_SUCCESS, BT_FAILURE, or BT_RUNNING.
 * BT_INVALID and BT_ERROR are set by the library, never by user code.
 */
typedef enum {
    BT_SUCCESS = 0,  /**< Node completed its task successfully               */
    BT_FAILURE = 1,  /**< Node completed but did not achieve its goal        */
    BT_RUNNING = 2,  /**< Node is still executing (long-running operation)   */
    BT_INVALID = 3,  /**< Node has not been ticked / was halted              */
    BT_ERROR   = 4,  /**< Internal error (NULL pointer, alloc failure, etc.) */
} bt_status_t;

/**
 * @brief Structural role of a node within the tree.
 */
typedef enum {
    /* --- Leaf nodes ---------------------------------------------------- */
    BT_NODE_ACTION        = 0, /**< User-defined action (may return RUNNING) */
    BT_NODE_CONDITION     = 1, /**< Instant check; returns SUCCESS or FAILURE */
    /* --- Composite nodes ----------------------------------------------- */
    BT_NODE_SEQUENCE      = 2, /**< AND  – children executed left-to-right   */
    BT_NODE_SELECTOR      = 3, /**< OR   – tries children until one succeeds */
    BT_NODE_PARALLEL      = 4, /**< ALL  – all children ticked every tick    */
    /* --- Decorator nodes ----------------------------------------------- */
    BT_NODE_INVERTER      = 5, /**< Flips SUCCESS ↔ FAILURE                 */
    BT_NODE_REPEATER      = 6, /**< Loops child N times (or forever)        */
    BT_NODE_RETRY         = 7, /**< Retries child on FAILURE up to N times  */
    BT_NODE_FORCE_SUCCESS = 8, /**< Maps any terminal status to SUCCESS      */
    BT_NODE_FORCE_FAILURE = 9, /**< Maps any terminal status to FAILURE      */
} bt_node_type_t;

/** @} */ /* BT_TYPES */

/**
 * @defgroup BT_CALLBACKS Callback signatures
 * @{
 */

struct bt_node;

/**
 * @brief Main tick callback — the node's per-cycle execution logic.
 *
 * Must return BT_SUCCESS, BT_FAILURE, or BT_RUNNING.  Must not block.
 * Use BT_RUNNING to signal work in progress; the tree scheduler will call
 * this function again on the next @ref bt_tree_tick invocation.
 *
 * @param[in] node     The node being ticked.
 * @param[in] context  User context pointer forwarded from @ref bt_tree_t.
 * @return             BT_SUCCESS, BT_FAILURE, or BT_RUNNING.
 */
typedef bt_status_t (*bt_tick_fn_t)   (struct bt_node *node, void *context);

/**
 * @brief Called once at the beginning of each new execution cycle.
 *
 * Invoked by @ref bt_node_tick the first time a node is ticked after it
 * was created or after a previous cycle completed (returned SUCCESS/FAILURE)
 * or the node was halted.  Use it to reset state-machine variables, timers,
 * counters, or any other per-cycle state.
 *
 * @param[in] node     The node being initialised.
 * @param[in] context  User context pointer.
 */
typedef void        (*bt_on_init_fn_t)(struct bt_node *node, void *context);

/**
 * @brief Called when the node is forcibly interrupted before completing.
 *
 * Invoked by @ref bt_node_halt when a parent composite decides to abort a
 * RUNNING child (e.g., a Sequence sibling failed, or the tree was destroyed).
 * Use it to cancel ongoing hardware operations, release locks, or free
 * temporary resources.
 *
 * @param[in] node     The node being halted.
 * @param[in] context  User context pointer.
 */
typedef void        (*bt_on_halt_fn_t)(struct bt_node *node, void *context);

/** @} */ /* BT_CALLBACKS */

/**
 * @defgroup BT_NODE Node structure
 * @{
 */

/**
 * @brief Single node in the behaviour tree.
 *
 * Composite and decorator nodes own a heap-allocated array of child pointers
 * that grows automatically.  Leaf nodes have @c child_count == 0.
 *
 * @note Do not modify fields directly; use the factory and manipulation
 *       functions instead.  Internal fields (initialized, running_child,
 *       repeat_count) are managed by the library.
 */
typedef struct bt_node {
    bt_node_type_t   type;              /**< Structural role of this node     */
    bt_status_t      status;            /**< Status after the most recent tick */

    /* --- Tree hierarchy ------------------------------------------------- */
    struct bt_node  *parent;            /**< Parent node (NULL for root)       */
    struct bt_node **children;          /**< Heap-allocated child pointer array */
    uint16_t         child_count;       /**< Number of current children        */
    uint16_t         child_capacity;    /**< Allocated capacity of children[]  */

    /* --- User callbacks ------------------------------------------------- */
    bt_tick_fn_t     tick_fn;           /**< Mandatory execution callback      */
    bt_on_init_fn_t  on_init_fn;        /**< Optional cycle-start callback     */
    bt_on_halt_fn_t  on_halt_fn;        /**< Optional interrupt callback       */

    /* --- User payload --------------------------------------------------- */
    void            *user_data;         /**< Forwarded to every callback       */

    /* --- Internal execution state --------------------------------------- */
    bool             initialized;       /**< TRUE while mid-cycle (on_init ran) */
    uint16_t         running_child;     /**< Stateful composite: resume index  */

    /* --- Decorator / Parallel parameters -------------------------------- */
    int32_t          repeat_count;      /**< Current repeat / retry iteration  */
    int32_t          max_count;         /**< Maximum iterations (-1 = infinite) */
    uint16_t         success_threshold; /**< Parallel: min successes to pass   */

#if BT_ENABLE_NODE_NAMES
    const char      *name;              /**< Human-readable label (debug only) */
#endif
} bt_node_t;

/** @} */ /* BT_NODE */

/**
 * @defgroup BT_TREE Tree structure
 * @{
 */

/**
 * @brief Behaviour tree instance.
 *
 * Created with @ref bt_tree_create, destroyed with @ref bt_tree_destroy.
 * Call @ref bt_tree_tick periodically from your control loop or RTOS task.
 */
typedef struct bt_tree {
    bt_node_t  *root;         /**< Root node of the tree                      */
    void       *context;      /**< User context forwarded to every callback   */
    bt_mutex_t  mutex;        /**< Platform mutex (type from bt_port.h)       */
    bool        thread_safe;  /**< TRUE if mutex is active                    */

#if BT_ENABLE_STATS
    uint32_t    tick_count;   /**< Total bt_tree_tick calls since creation    */
    bt_status_t last_status;  /**< Root status of the most recent tick        */
#endif
} bt_tree_t;

/** @} */ /* BT_TREE */

/**
 * @defgroup BT_CORE Core API
 * @brief Tree lifetime management and tick execution.
 * @{
 */

/**
 * @brief Allocate and initialise a behaviour tree.
 *
 * @param[in] root         Root node; must be fully constructed (children
 *                         added) before this call.
 * @param[in] context      Opaque pointer forwarded unchanged to every
 *                         tick / on_init / on_halt callback.  May be NULL.
 * @param[in] thread_safe  When @c true a recursive mutex is created and
 *                         @ref bt_tree_tick is safe to call concurrently
 *                         from multiple RTOS tasks.  When @c false no mutex
 *                         overhead is incurred.
 * @return Pointer to the new tree, or @c NULL on allocation failure.
 *
 * @note Build the tree (nodes + children) before calling this.  Construction
 *       helpers are not protected by the tree mutex.
 */
bt_tree_t  *bt_tree_create (bt_node_t *root, void *context, bool thread_safe);

/**
 * @brief Halt and free the tree and every node it owns.
 *
 * Calls @ref bt_node_halt on the root (propagates to all RUNNING descendants),
 * then recursively frees all nodes, then frees the tree itself.  Destroys the
 * mutex if one was created.
 *
 * @param[in] tree  Tree to destroy.  May be @c NULL (no-op).
 *
 * @warning Do not call @ref bt_tree_tick after this.
 */
void        bt_tree_destroy(bt_tree_t *tree);

/**
 * @brief Execute one tick of the behaviour tree.
 *
 * Acquires the tree mutex (if thread_safe), ticks the root node, updates
 * statistics if enabled, releases the mutex, and returns the root status.
 *
 * Call this at a fixed rate from your control loop or an RTOS task:
 * @code
 *   while (1) {
 *       vTaskDelay(pdMS_TO_TICKS(20));
 *       bt_tree_tick(tree);
 *   }
 * @endcode
 *
 * @param[in] tree  Tree to tick.
 * @return          Root node status: BT_SUCCESS, BT_FAILURE, BT_RUNNING,
 *                  or BT_ERROR if @p tree / root is NULL.
 */
bt_status_t bt_tree_tick   (bt_tree_t *tree);

/** @} */ /* BT_CORE */

/**
 * @defgroup BT_FACTORY Node factory helpers
 * @brief Create leaf, composite, and decorator nodes.
 * @{
 */

/**
 * @brief Create a leaf **Action** node.
 *
 * Actions represent ongoing operations.  Their @p tick_fn may return
 * BT_RUNNING to signal work in progress; on the next tree tick @p tick_fn
 * is called again (without calling @p on_init_fn again) until the action
 * returns a terminal status.
 *
 * @param[in] name        Human-readable label (stored by pointer; use a
 *                        string literal).  Ignored if BT_ENABLE_NODE_NAMES=0.
 * @param[in] tick_fn     Mandatory execution callback (must not be NULL).
 * @param[in] on_init_fn  Called at the start of each execution cycle.
 *                        Pass NULL if not needed.
 * @param[in] on_halt_fn  Called when the node is forcibly interrupted.
 *                        Pass NULL if not needed.
 * @param[in] user_data   Available through node->user_data in callbacks.
 * @return Pointer to new node, or NULL on allocation failure.
 */
bt_node_t *bt_action_create   (const char     *name,
                                bt_tick_fn_t    tick_fn,
                                bt_on_init_fn_t on_init_fn,
                                bt_on_halt_fn_t on_halt_fn,
                                void           *user_data);

/**
 * @brief Create a leaf **Condition** node.
 *
 * Conditions are instant checks; @p tick_fn must return either BT_SUCCESS
 * or BT_FAILURE, never BT_RUNNING.  No @p on_halt_fn is registered because
 * conditions cannot be in the RUNNING state.
 *
 * @param[in] name      Human-readable label.
 * @param[in] tick_fn   Condition check callback (must not be NULL).
 * @param[in] user_data Forwarded to callbacks via node->user_data.
 * @return Pointer to new node, or NULL on allocation failure.
 */
bt_node_t *bt_condition_create(const char     *name,
                                bt_tick_fn_t    tick_fn,
                                void           *user_data);

/**
 * @brief Create a **Sequence** (→) composite node.
 *
 * Ticks children left-to-right (stateful/memory policy):
 * - Returns BT_SUCCESS if **all** children return BT_SUCCESS.
 * - Returns BT_FAILURE on the first child that returns BT_FAILURE.
 * - Returns BT_RUNNING while a child is RUNNING; resumes from that child
 *   on the next tick without re-executing earlier children.
 *
 * @param[in] name  Human-readable label.
 * @return Pointer to new node, or NULL on allocation failure.
 */
bt_node_t *bt_sequence_create (const char *name);

/**
 * @brief Create a **Selector** (?) composite node.
 *
 * Ticks children left-to-right (stateful/memory policy):
 * - Returns BT_SUCCESS on the first child that returns BT_SUCCESS.
 * - Returns BT_FAILURE if **all** children return BT_FAILURE.
 * - Returns BT_RUNNING while a child is RUNNING; resumes from that child
 *   on the next tick.
 *
 * @param[in] name  Human-readable label.
 * @return Pointer to new node, or NULL on allocation failure.
 */
bt_node_t *bt_selector_create (const char *name);

/**
 * @brief Create a **Parallel** composite node.
 *
 * Ticks **all** children on every tree tick (fully reactive).  This makes it
 * ideal for safety monitoring: combine conditions with long-running actions;
 * if any condition fails the whole parallel fails and the actions are halted.
 *
 * @code
 *   Parallel [threshold = N]
 *   ├── Condition A    (re-evaluated every tick)
 *   ├── Condition B    (re-evaluated every tick)
 *   └── Action C       (continues its RUNNING state across ticks)
 * @endcode
 *
 * @param[in] name               Human-readable label.
 * @param[in] success_threshold  Number of children that must return
 *                               BT_SUCCESS for the Parallel to return
 *                               BT_SUCCESS.  The node returns BT_FAILURE when
 *                               @c (child_count - failures) < threshold,
 *                               making success impossible.
 * @return Pointer to new node, or NULL on allocation failure.
 */
bt_node_t *bt_parallel_create (const char *name, uint16_t success_threshold);

/**
 * @brief Create an **Inverter** decorator node.
 *
 * Maps BT_SUCCESS → BT_FAILURE and BT_FAILURE → BT_SUCCESS.
 * BT_RUNNING passes through unchanged.
 *
 * @param[in] name  Human-readable label.
 * @return Pointer to new node, or NULL on allocation failure.
 */
bt_node_t *bt_inverter_create     (const char *name);

/**
 * @brief Create a **Repeater** decorator node.
 *
 * Repeats the child node @p max_count times.  The child is re-initialised
 * (@p on_init is called) at the start of each repetition.
 *
 * - Returns BT_RUNNING while repeating.
 * - Returns BT_SUCCESS after completing all repetitions.
 * - Returns BT_FAILURE immediately if the child returns BT_FAILURE.
 *
 * @param[in] name       Human-readable label.
 * @param[in] max_count  Number of repetitions.  Pass @c -1 to repeat
 *                       indefinitely (the node never returns BT_SUCCESS).
 * @return Pointer to new node, or NULL on allocation failure.
 */
bt_node_t *bt_repeater_create     (const char *name, int32_t max_count);

/**
 * @brief Create a **Retry** decorator node.
 *
 * Retries the child node on BT_FAILURE up to @p max_retries times.
 *
 * - Returns BT_SUCCESS immediately when the child returns BT_SUCCESS.
 * - Returns BT_RUNNING while retrying.
 * - Returns BT_FAILURE after exhausting all retry attempts.
 *
 * @param[in] name         Human-readable label.
 * @param[in] max_retries  Maximum number of retry attempts.  Pass @c -1 for
 *                         unlimited retries (never returns BT_FAILURE).
 * @return Pointer to new node, or NULL on allocation failure.
 */
bt_node_t *bt_retry_create        (const char *name, int32_t max_retries);

/**
 * @brief Create a **ForceSuccess** decorator node.
 *
 * Maps any terminal status (SUCCESS or FAILURE) to BT_SUCCESS.
 * BT_RUNNING passes through unchanged.
 *
 * @param[in] name  Human-readable label.
 * @return Pointer to new node, or NULL on allocation failure.
 */
bt_node_t *bt_force_success_create(const char *name);

/**
 * @brief Create a **ForceFailure** decorator node.
 *
 * Maps any terminal status to BT_FAILURE.  BT_RUNNING passes through.
 *
 * @param[in] name  Human-readable label.
 * @return Pointer to new node, or NULL on allocation failure.
 */
bt_node_t *bt_force_failure_create(const char *name);

/** @} */ /* BT_FACTORY */

/**
 * @defgroup BT_MANIP Node manipulation
 * @{
 */

/**
 * @brief Append a child node to a composite or decorator node.
 *
 * The children array grows automatically (doubles in capacity when full).
 * Sets @c child->parent to @p parent.
 *
 * @param[in,out] parent  Node to attach the child to.
 * @param[in]     child   Node to append.
 * @return BT_SUCCESS on success, BT_ERROR on allocation failure or NULL input.
 */
bt_status_t bt_node_add_child(bt_node_t *parent, bt_node_t *child);

/**
 * @brief Recursively free a node and all of its descendants.
 *
 * Frees the children array and the node struct for every node in the
 * sub-tree.  Does NOT call @p on_halt — halt the tree (or sub-tree) first
 * if nodes may still be RUNNING.
 *
 * @param[in] node  Root of the sub-tree to free.  May be NULL (no-op).
 */
void        bt_node_destroy(bt_node_t *node);

/** @} */ /* BT_MANIP */

/**
 * @defgroup BT_LOW Low-level node execution
 * @brief Useful for implementing custom composite or decorator types.
 * @{
 */

/**
 * @brief Tick a single node.
 *
 * Calls @p on_init_fn the first time a node is ticked in a new execution
 * cycle (when @c initialized is @c false), then calls @p tick_fn.  Resets
 * the @c initialized flag when @p tick_fn returns a terminal status so
 * @p on_init_fn is called again next cycle.
 *
 * @param[in,out] node     Node to tick.
 * @param[in]     context  User context pointer.
 * @return                 Status returned by @p tick_fn, or BT_ERROR if
 *                         @p node or @p tick_fn is NULL.
 */
bt_status_t bt_node_tick(bt_node_t *node, void *context);

/**
 * @brief Forcibly interrupt a node and all of its running descendants.
 *
 * Recursively calls @ref bt_node_halt on every initialized child, then
 * calls this node's @p on_halt_fn (if set), then resets @c initialized,
 * @c status, @c running_child, and @c repeat_count.
 *
 * This function is a no-op if @c node is @c NULL or @c node->initialized
 * is @c false (the node is not currently mid-cycle).
 *
 * @param[in,out] node     Node to halt.
 * @param[in]     context  User context pointer.
 */
void        bt_node_halt(bt_node_t *node, void *context);

/** @} */ /* BT_LOW */

/**
 * @defgroup BT_UTIL Utilities
 * @{
 */

/**
 * @brief Return a human-readable string for a @ref bt_status_t value.
 *
 * @param[in] status  Status value to stringify.
 * @return Static string literal, e.g., @c "SUCCESS", @c "RUNNING".
 *         Returns @c "UNKNOWN" for out-of-range values.
 */
const char *bt_status_str   (bt_status_t    status);

/**
 * @brief Return a human-readable string for a @ref bt_node_type_t value.
 *
 * @param[in] type  Node type to stringify.
 * @return Static string literal, e.g., @c "Sequence", @c "Action".
 */
const char *bt_node_type_str(bt_node_type_t type);

/** @} */ /* BT_UTIL */

#ifdef __cplusplus
}
#endif

#endif /* BEHAVIOR_TREE_H */

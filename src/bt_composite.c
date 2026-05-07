/**
 * @file   bt_composite.c
 * @brief  Tick implementations for Sequence, Selector, and Parallel nodes.
 * @author Subhajit Roy <subhajitroy005@gmail.com>
 *
 * @ingroup BT_INTERNAL_TICKS
 *
 * @details
 * ## Sequence and Selector — stateful/memory policy
 *
 * Both use `bt_node_t::running_child` to remember which child was RUNNING
 * on the previous tick.  On resume, iteration starts from that index rather
 * than from child 0.  This means:
 *
 *  - Completed actions (SUCCESS) before the running child are not re-executed.
 *  - If the parent forces a restart (halt then re-tick), `running_child` is
 *    reset to 0 by @ref bt_node_halt.
 *
 * This policy is preferable for most embedded use-cases where action nodes
 * carry ongoing work that must not be interrupted unnecessarily.
 *
 * ## Parallel — fully reactive policy
 *
 * Every child is ticked on every tree tick.  Children that returned a
 * terminal status in a previous tick have `initialized = false`, so
 * @ref bt_node_tick will call their `on_init_fn` again and re-execute them.
 * This makes conditions re-evaluate on every tick and is the standard way
 * to implement "monitoring" trees:
 *
 * @code
 *   Parallel [threshold = N]
 *   ├── Condition A   (re-evaluated every tick — reactive guard)
 *   └── Action B      (continues across ticks while RUNNING)
 * @endcode
 *
 * If Condition A fails on any tick, the Parallel immediately returns
 * FAILURE and halts Action B.
 */
#include "bt_internal.h"

/* ===========================================================================
 *  Sequence  (→)
 * ========================================================================= */

/**
 * @brief Tick implementation for Sequence nodes.
 *
 * Iterates children starting from `node->running_child`:
 *  - BT_RUNNING  → record index, return BT_RUNNING.
 *  - BT_FAILURE  → halt any earlier initialised siblings, reset index,
 *                  return BT_FAILURE.
 *  - BT_SUCCESS  → advance to the next child.
 *
 * All children succeeded → reset index, return BT_SUCCESS.
 *
 * @param[in,out] node  Sequence node being ticked.
 * @param[in]     ctx   User context pointer.
 * @return BT_SUCCESS, BT_FAILURE, BT_RUNNING, or BT_ERROR.
 */
bt_status_t bt_sequence_tick(bt_node_t *node, void *ctx)
{
    for (uint16_t i = node->running_child; i < node->child_count; i++) {
        bt_status_t s = bt_node_tick(node->children[i], ctx);

        switch (s) {
        case BT_RUNNING:
            node->running_child = i;
            return BT_RUNNING;

        case BT_FAILURE:
            /* Halt any initialised siblings that preceded the failing child */
            for (uint16_t j = 0; j < i; j++)
                bt_node_halt(node->children[j], ctx);
            node->running_child = 0;
            return BT_FAILURE;

        case BT_SUCCESS:
            break; /* advance to next child */

        default:
            node->running_child = 0;
            return BT_ERROR;
        }
    }

    node->running_child = 0;
    return BT_SUCCESS;
}

/* ===========================================================================
 *  Selector  (?)
 * ========================================================================= */

/**
 * @brief Tick implementation for Selector nodes.
 *
 * Iterates children starting from `node->running_child`:
 *  - BT_RUNNING  → record index, return BT_RUNNING.
 *  - BT_SUCCESS  → halt earlier siblings, reset index, return BT_SUCCESS.
 *  - BT_FAILURE  → try the next child.
 *
 * All children failed → reset index, return BT_FAILURE.
 *
 * @param[in,out] node  Selector node being ticked.
 * @param[in]     ctx   User context pointer.
 * @return BT_SUCCESS, BT_FAILURE, BT_RUNNING, or BT_ERROR.
 */
bt_status_t bt_selector_tick(bt_node_t *node, void *ctx)
{
    for (uint16_t i = node->running_child; i < node->child_count; i++) {
        bt_status_t s = bt_node_tick(node->children[i], ctx);

        switch (s) {
        case BT_RUNNING:
            node->running_child = i;
            return BT_RUNNING;

        case BT_SUCCESS:
            for (uint16_t j = 0; j < i; j++)
                bt_node_halt(node->children[j], ctx);
            node->running_child = 0;
            return BT_SUCCESS;

        case BT_FAILURE:
            break; /* try the next child */

        default:
            node->running_child = 0;
            return BT_ERROR;
        }
    }

    node->running_child = 0;
    return BT_FAILURE;
}

/* ===========================================================================
 *  Parallel
 * ========================================================================= */

/**
 * @brief Tick implementation for Parallel nodes.
 *
 * Every child is ticked unconditionally on each call (fully reactive).
 * Children that completed (SUCCESS or FAILURE) in a previous tick have
 * `initialized = false`; @ref bt_node_tick will re-initialise and re-run
 * them, making conditions re-evaluate every tick.
 *
 * Decision logic:
 *  - success_count >= success_threshold → halt all children, return SUCCESS.
 *  - (child_count − failure_count) < success_threshold → success impossible,
 *    halt all children, return FAILURE.
 *  - Otherwise → return RUNNING.
 *
 * @param[in,out] node  Parallel node being ticked.
 * @param[in]     ctx   User context pointer.
 * @return BT_SUCCESS, BT_FAILURE, or BT_RUNNING.
 */
bt_status_t bt_parallel_tick(bt_node_t *node, void *ctx)
{
    uint16_t       success_count = 0;
    uint16_t       failure_count = 0;
    const uint16_t n             = node->child_count;
    const uint16_t thresh        = node->success_threshold;

    for (uint16_t i = 0; i < n; i++) {
        bt_status_t s = bt_node_tick(node->children[i], ctx);
        if      (s == BT_SUCCESS) success_count++;
        else if (s == BT_FAILURE) failure_count++;
    }

    if (success_count >= thresh) {
        for (uint16_t i = 0; i < n; i++)
            bt_node_halt(node->children[i], ctx);
        return BT_SUCCESS;
    }

    /* Success is now impossible — fail fast */
    if ((uint16_t)(n - failure_count) < thresh) {
        for (uint16_t i = 0; i < n; i++)
            bt_node_halt(node->children[i], ctx);
        return BT_FAILURE;
    }

    return BT_RUNNING;
}

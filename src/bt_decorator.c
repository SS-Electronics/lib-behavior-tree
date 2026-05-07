/**
 * @file   bt_decorator.c
 * @brief  Tick implementations for all decorator nodes.
 * @author Subhajit Roy <subhajitroy005@gmail.com>
 *
 * @ingroup BT_INTERNAL_TICKS
 *
 * @details
 * Each decorator wraps exactly one child (`children[0]`).  Decorators
 * modify the child's status or control how many times it is executed.
 *
 * | Decorator    | SUCCESS in    | FAILURE in    | RUNNING in  |
 * |:------------ |:------------- |:------------- |:----------- |
 * | Inverter     | → FAILURE     | → SUCCESS     | → RUNNING   |
 * | Repeater     | repeat/done   | → FAILURE     | → RUNNING   |
 * | Retry        | → SUCCESS     | retry/exhaust | → RUNNING   |
 * | ForceSuccess | → SUCCESS     | → SUCCESS     | → RUNNING   |
 * | ForceFailure | → FAILURE     | → FAILURE     | → RUNNING   |
 */
#include "bt_internal.h"

/* ===========================================================================
 *  Inverter
 * ========================================================================= */

/**
 * @brief Tick implementation for Inverter nodes.
 *
 * Flips SUCCESS ↔ FAILURE.  BT_RUNNING and BT_ERROR pass through unchanged.
 *
 * @param[in,out] node  Inverter node being ticked.
 * @param[in]     ctx   User context pointer.
 * @return BT_FAILURE if child returned BT_SUCCESS; BT_SUCCESS if child
 *         returned BT_FAILURE; the child's raw status otherwise.
 */
bt_status_t bt_inverter_tick(bt_node_t *node, void *ctx)
{
    if (node->child_count == 0) return BT_ERROR;

    bt_status_t s = bt_node_tick(node->children[0], ctx);
    if      (s == BT_SUCCESS) return BT_FAILURE;
    else if (s == BT_FAILURE) return BT_SUCCESS;
    return s;
}

/* ===========================================================================
 *  Repeater
 * ========================================================================= */

/**
 * @brief Tick implementation for Repeater nodes.
 *
 * Repeats the child exactly `max_count` times (`max_count == -1` repeats
 * forever).  The child is re-initialised by calling @ref bt_node_halt before
 * each new repetition (so its `on_init_fn` fires at the start of every pass).
 *
 * State machine:
 *  - child returns BT_FAILURE → propagate BT_FAILURE immediately.
 *  - child returns BT_RUNNING → propagate BT_RUNNING.
 *  - child returns BT_SUCCESS → increment `repeat_count`.
 *    - If `max_count != -1 && repeat_count >= max_count` → reset counter,
 *      return BT_SUCCESS.
 *    - Otherwise → halt child to reset it, return BT_RUNNING.
 *
 * @param[in,out] node  Repeater node being ticked.
 * @param[in]     ctx   User context pointer.
 * @return BT_RUNNING while repeating; BT_SUCCESS after all repetitions;
 *         BT_FAILURE if the child fails; BT_ERROR if no child is attached.
 */
bt_status_t bt_repeater_tick(bt_node_t *node, void *ctx)
{
    if (node->child_count == 0) return BT_ERROR;

    bt_status_t s = bt_node_tick(node->children[0], ctx);

    if (s == BT_FAILURE) return BT_FAILURE;

    if (s == BT_SUCCESS) {
        node->repeat_count++;
        if (node->max_count != -1 && node->repeat_count >= node->max_count) {
            node->repeat_count = 0;
            return BT_SUCCESS;
        }
        /* Re-arm the child for the next repetition */
        bt_node_halt(node->children[0], ctx);
    }

    return BT_RUNNING;
}

/* ===========================================================================
 *  Retry
 * ========================================================================= */

/**
 * @brief Tick implementation for Retry nodes.
 *
 * Retries the child on BT_FAILURE up to `max_count` times
 * (`max_count == -1` retries indefinitely).  The child is re-initialised
 * by @ref bt_node_halt between attempts.
 *
 * State machine:
 *  - child returns BT_SUCCESS → reset counter, return BT_SUCCESS.
 *  - child returns BT_RUNNING → propagate BT_RUNNING.
 *  - child returns BT_FAILURE → increment `repeat_count`.
 *    - If `max_count != -1 && repeat_count >= max_count` → reset counter,
 *      return BT_FAILURE (all attempts exhausted).
 *    - Otherwise → halt child to reset it, return BT_RUNNING.
 *
 * @param[in,out] node  Retry node being ticked.
 * @param[in]     ctx   User context pointer.
 * @return BT_SUCCESS on first child success; BT_FAILURE after exhausting
 *         all retries; BT_RUNNING while retrying; BT_ERROR if no child.
 */
bt_status_t bt_retry_tick(bt_node_t *node, void *ctx)
{
    if (node->child_count == 0) return BT_ERROR;

    bt_status_t s = bt_node_tick(node->children[0], ctx);

    if (s == BT_SUCCESS) {
        node->repeat_count = 0;
        return BT_SUCCESS;
    }

    if (s == BT_FAILURE) {
        node->repeat_count++;
        if (node->max_count != -1 && node->repeat_count >= node->max_count) {
            node->repeat_count = 0;
            return BT_FAILURE;
        }
        bt_node_halt(node->children[0], ctx);
    }

    return BT_RUNNING;
}

/* ===========================================================================
 *  Force Success
 * ========================================================================= */

/**
 * @brief Tick implementation for ForceSuccess nodes.
 *
 * Maps any terminal result (SUCCESS or FAILURE) to BT_SUCCESS.
 * BT_RUNNING passes through unchanged so the child can complete
 * its work normally.
 *
 * @param[in,out] node  ForceSuccess node being ticked.
 * @param[in]     ctx   User context pointer.
 * @return BT_SUCCESS when the child terminates; BT_RUNNING while it runs;
 *         BT_ERROR if no child is attached.
 */
bt_status_t bt_force_success_tick(bt_node_t *node, void *ctx)
{
    if (node->child_count == 0) return BT_ERROR;
    bt_status_t s = bt_node_tick(node->children[0], ctx);
    if (s == BT_RUNNING) return BT_RUNNING;
    return BT_SUCCESS;
}

/* ===========================================================================
 *  Force Failure
 * ========================================================================= */

/**
 * @brief Tick implementation for ForceFailure nodes.
 *
 * Maps any terminal result to BT_FAILURE.  BT_RUNNING passes through.
 *
 * @param[in,out] node  ForceFailure node being ticked.
 * @param[in]     ctx   User context pointer.
 * @return BT_FAILURE when the child terminates; BT_RUNNING while it runs;
 *         BT_ERROR if no child is attached.
 */
bt_status_t bt_force_failure_tick(bt_node_t *node, void *ctx)
{
    if (node->child_count == 0) return BT_ERROR;
    bt_status_t s = bt_node_tick(node->children[0], ctx);
    if (s == BT_RUNNING) return BT_RUNNING;
    return BT_FAILURE;
}

/**
 * @file  bt_composite.c
 * @brief Tick implementations for Sequence, Selector, and Parallel nodes.
 *
 * Sequence and Selector use a *stateful/memory* policy:
 *   - The index of the last RUNNING child is remembered.
 *   - On the next tick the composite resumes from that child instead of
 *     restarting from child 0.  This avoids re-executing completed actions.
 *
 * Parallel uses a *per-cycle memory* policy:
 *   - All children are ticked every tick.
 *   - A child that already settled (SUCCESS or FAILURE) within the current
 *     parallel cycle is not re-ticked until the parallel starts a new cycle.
 *   - bt_parallel_on_init resets all children's settled state at the
 *     beginning of each new cycle.
 */
#include "bt_internal.h"

/* ===========================================================================
 *  Sequence  (→)
 *
 *  Returns SUCCESS  if every child returns SUCCESS.
 *  Returns FAILURE  on the first child that returns FAILURE.
 *  Returns RUNNING  if the currently executing child returns RUNNING.
 * ========================================================================= */
bt_status_t bt_sequence_tick(bt_node_t *node, void *ctx)
{
    for (uint16_t i = node->running_child; i < node->child_count; i++) {
        bt_status_t s = bt_node_tick(node->children[i], ctx);

        switch (s) {
        case BT_RUNNING:
            node->running_child = i;
            return BT_RUNNING;

        case BT_FAILURE:
            /* Halt any siblings that might still be initialized */
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
 *
 *  Returns SUCCESS  on the first child that returns SUCCESS.
 *  Returns FAILURE  if every child returns FAILURE.
 *  Returns RUNNING  if the currently executing child returns RUNNING.
 * ========================================================================= */
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
 *
 *  All children are ticked on every call.  Children that have already
 *  settled (SUCCESS or FAILURE) in this cycle are not re-ticked.
 *
 *  Returns SUCCESS  when successes >= success_threshold.
 *  Returns FAILURE  when remaining possible successes < success_threshold.
 *  Returns RUNNING  otherwise.
 *
 *  bt_parallel_on_init (called once per new cycle) resets per-cycle child
 *  state so that every child starts fresh at the beginning of each cycle.
 * ========================================================================= */

void bt_parallel_on_init(bt_node_t *node, void *ctx)
{
    (void)ctx;
    /* Mark all children as not-yet-settled for the new cycle */
    for (uint16_t i = 0; i < node->child_count; i++) {
        node->children[i]->status      = BT_INVALID;
        node->children[i]->initialized = false;
    }
}

bt_status_t bt_parallel_tick(bt_node_t *node, void *ctx)
{
    uint16_t       success_count = 0;
    uint16_t       failure_count = 0;
    const uint16_t n             = node->child_count;
    const uint16_t thresh        = node->success_threshold;

    for (uint16_t i = 0; i < n; i++) {
        bt_node_t  *child = node->children[i];
        bt_status_t s;

        /* Re-use a settled result from this cycle rather than re-ticking */
        if (child->status == BT_SUCCESS || child->status == BT_FAILURE) {
            s = child->status;
        } else {
            s = bt_node_tick(child, ctx);
        }

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

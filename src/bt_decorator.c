/**
 * @file  bt_decorator.c
 * @brief Tick implementations for all decorator nodes.
 *
 * Each decorator wraps exactly one child (children[0]).
 */
#include "bt_internal.h"

/* ===========================================================================
 *  Inverter
 *
 *  SUCCESS  → FAILURE
 *  FAILURE  → SUCCESS
 *  RUNNING  → RUNNING  (pass-through)
 * ========================================================================= */
bt_status_t bt_inverter_tick(bt_node_t *node, void *ctx)
{
    if (node->child_count == 0) return BT_ERROR;

    bt_status_t s = bt_node_tick(node->children[0], ctx);
    if      (s == BT_SUCCESS) return BT_FAILURE;
    else if (s == BT_FAILURE) return BT_SUCCESS;
    return s; /* BT_RUNNING or BT_ERROR */
}

/* ===========================================================================
 *  Repeater
 *
 *  Repeats child exactly max_count times (max_count == -1 → infinite).
 *  Returns RUNNING while repeating.
 *  Returns SUCCESS after reaching max_count.
 *  Returns FAILURE if the child returns FAILURE.
 * ========================================================================= */
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

        /* Force child to re-initialise next tick */
        bt_node_halt(node->children[0], ctx);
    }

    return BT_RUNNING;
}

/* ===========================================================================
 *  Retry
 *
 *  Retries child on FAILURE up to max_count times (max_count == -1 → infinite).
 *  Returns RUNNING while retrying.
 *  Returns SUCCESS immediately when child returns SUCCESS.
 *  Returns FAILURE after exhausting all retry attempts.
 * ========================================================================= */
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

        /* Re-initialise child for the next attempt */
        bt_node_halt(node->children[0], ctx);
    }

    return BT_RUNNING;
}

/* ===========================================================================
 *  Force Success
 *
 *  Maps SUCCESS → SUCCESS, FAILURE → SUCCESS, RUNNING → RUNNING.
 * ========================================================================= */
bt_status_t bt_force_success_tick(bt_node_t *node, void *ctx)
{
    if (node->child_count == 0) return BT_ERROR;

    bt_status_t s = bt_node_tick(node->children[0], ctx);
    if (s == BT_RUNNING) return BT_RUNNING;
    return BT_SUCCESS;
}

/* ===========================================================================
 *  Force Failure
 *
 *  Maps SUCCESS → FAILURE, FAILURE → FAILURE, RUNNING → RUNNING.
 * ========================================================================= */
bt_status_t bt_force_failure_tick(bt_node_t *node, void *ctx)
{
    if (node->child_count == 0) return BT_ERROR;

    bt_status_t s = bt_node_tick(node->children[0], ctx);
    if (s == BT_RUNNING) return BT_RUNNING;
    return BT_FAILURE;
}

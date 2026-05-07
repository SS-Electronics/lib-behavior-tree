/**
 * @file  behavior_tree.c
 * @brief Core: tree management, node creation, bt_node_tick/halt, utilities.
 */
#include "behavior_tree.h"
#include "bt_internal.h"
#include <string.h>

/* ===========================================================================
 *  Internal helpers
 * ========================================================================= */

static bt_node_t *node_alloc(bt_node_type_t type, const char *name,
                              bt_tick_fn_t tick_fn)
{
    bt_node_t *n = (bt_node_t *)BT_MALLOC(sizeof(bt_node_t));
    if (!n) return NULL;
    memset(n, 0, sizeof(bt_node_t));
    n->type      = type;
    n->status    = BT_INVALID;
    n->max_count = -1;
    n->tick_fn   = tick_fn;
#if BT_ENABLE_NODE_NAMES
    n->name = name;
#else
    (void)name;
#endif
    return n;
}

/* ===========================================================================
 *  bt_node_tick / bt_node_halt
 * ========================================================================= */

bt_status_t bt_node_tick(bt_node_t *node, void *context)
{
    if (!node || !node->tick_fn) return BT_ERROR;

    /* Call on_init once per execution cycle */
    if (!node->initialized) {
        if (node->on_init_fn) node->on_init_fn(node, context);
        node->initialized = true;
    }

    bt_status_t s = node->tick_fn(node, context);
    node->status  = s;

    /* Reset so on_init fires again at the start of the next cycle */
    if (s != BT_RUNNING) node->initialized = false;

    return s;
}

void bt_node_halt(bt_node_t *node, void *context)
{
    if (!node || !node->initialized) return;

    /* Recursively halt all running descendants first */
    for (uint16_t i = 0; i < node->child_count; i++)
        bt_node_halt(node->children[i], context);

    if (node->on_halt_fn) node->on_halt_fn(node, context);

    node->status        = BT_INVALID;
    node->initialized   = false;
    node->running_child = 0;
    node->repeat_count  = 0;
}

/* ===========================================================================
 *  Node factory
 * ========================================================================= */

bt_node_t *bt_action_create(const char *name, bt_tick_fn_t tick_fn,
                             bt_on_init_fn_t on_init_fn,
                             bt_on_halt_fn_t on_halt_fn,
                             void *user_data)
{
    bt_node_t *n = node_alloc(BT_NODE_ACTION, name, tick_fn);
    if (!n) return NULL;
    n->on_init_fn = on_init_fn;
    n->on_halt_fn = on_halt_fn;
    n->user_data  = user_data;
    return n;
}

bt_node_t *bt_condition_create(const char *name, bt_tick_fn_t tick_fn,
                                void *user_data)
{
    bt_node_t *n = node_alloc(BT_NODE_CONDITION, name, tick_fn);
    if (!n) return NULL;
    n->user_data = user_data;
    return n;
}

bt_node_t *bt_sequence_create(const char *name)
{
    return node_alloc(BT_NODE_SEQUENCE, name, bt_sequence_tick);
}

bt_node_t *bt_selector_create(const char *name)
{
    return node_alloc(BT_NODE_SELECTOR, name, bt_selector_tick);
}

bt_node_t *bt_parallel_create(const char *name, uint16_t success_threshold)
{
    bt_node_t *n = node_alloc(BT_NODE_PARALLEL, name, bt_parallel_tick);
    if (!n) return NULL;
    n->success_threshold = success_threshold;
    n->on_init_fn        = bt_parallel_on_init;  /* resets per-cycle child state */
    return n;
}

bt_node_t *bt_inverter_create(const char *name)
{
    return node_alloc(BT_NODE_INVERTER, name, bt_inverter_tick);
}

bt_node_t *bt_repeater_create(const char *name, int32_t max_count)
{
    bt_node_t *n = node_alloc(BT_NODE_REPEATER, name, bt_repeater_tick);
    if (!n) return NULL;
    n->max_count = max_count;
    return n;
}

bt_node_t *bt_retry_create(const char *name, int32_t max_retries)
{
    bt_node_t *n = node_alloc(BT_NODE_RETRY, name, bt_retry_tick);
    if (!n) return NULL;
    n->max_count = max_retries;
    return n;
}

bt_node_t *bt_force_success_create(const char *name)
{
    return node_alloc(BT_NODE_FORCE_SUCCESS, name, bt_force_success_tick);
}

bt_node_t *bt_force_failure_create(const char *name)
{
    return node_alloc(BT_NODE_FORCE_FAILURE, name, bt_force_failure_tick);
}

/* ===========================================================================
 *  bt_node_add_child / bt_node_destroy
 * ========================================================================= */

bt_status_t bt_node_add_child(bt_node_t *parent, bt_node_t *child)
{
    if (!parent || !child) return BT_ERROR;

    if (parent->child_count >= parent->child_capacity) {
        uint16_t new_cap = (parent->child_capacity == 0)
                         ? (uint16_t)BT_CHILDREN_INITIAL_CAPACITY
                         : (uint16_t)(parent->child_capacity * 2u);

        bt_node_t **arr = (bt_node_t **)BT_MALLOC(
                              (size_t)new_cap * sizeof(bt_node_t *));
        if (!arr) return BT_ERROR;

        if (parent->children) {
            for (uint16_t i = 0; i < parent->child_count; i++)
                arr[i] = parent->children[i];
            BT_FREE(parent->children);
        }
        parent->children       = arr;
        parent->child_capacity = new_cap;
    }

    parent->children[parent->child_count++] = child;
    child->parent = parent;
    return BT_SUCCESS;
}

void bt_node_destroy(bt_node_t *node)
{
    if (!node) return;
    for (uint16_t i = 0; i < node->child_count; i++)
        bt_node_destroy(node->children[i]);
    if (node->children) BT_FREE(node->children);
    BT_FREE(node);
}

/* ===========================================================================
 *  Tree
 * ========================================================================= */

bt_tree_t *bt_tree_create(bt_node_t *root, void *context, bool thread_safe)
{
    bt_tree_t *tree = (bt_tree_t *)BT_MALLOC(sizeof(bt_tree_t));
    if (!tree) return NULL;
    memset(tree, 0, sizeof(bt_tree_t));

    tree->root        = root;
    tree->context     = context;
    tree->thread_safe = thread_safe;

#if BT_ENABLE_STATS
    tree->last_status = BT_INVALID;
#endif

    if (thread_safe && !bt_port_mutex_create(&tree->mutex)) {
        BT_FREE(tree);
        return NULL;
    }

    return tree;
}

void bt_tree_destroy(bt_tree_t *tree)
{
    if (!tree) return;

    if (tree->thread_safe) bt_port_mutex_lock(&tree->mutex);

    bt_node_halt   (tree->root, tree->context);
    bt_node_destroy(tree->root);

    if (tree->thread_safe) {
        bt_port_mutex_unlock (&tree->mutex);
        bt_port_mutex_destroy(&tree->mutex);
    }

    BT_FREE(tree);
}

bt_status_t bt_tree_tick(bt_tree_t *tree)
{
    if (!tree || !tree->root) return BT_ERROR;

    if (tree->thread_safe) bt_port_mutex_lock(&tree->mutex);

    bt_status_t s = bt_node_tick(tree->root, tree->context);

#if BT_ENABLE_STATS
    tree->tick_count++;
    tree->last_status = s;
#endif

    if (tree->thread_safe) bt_port_mutex_unlock(&tree->mutex);

    return s;
}

/* ===========================================================================
 *  Utilities
 * ========================================================================= */

const char *bt_status_str(bt_status_t status)
{
    switch (status) {
        case BT_SUCCESS: return "SUCCESS";
        case BT_FAILURE: return "FAILURE";
        case BT_RUNNING: return "RUNNING";
        case BT_INVALID: return "INVALID";
        case BT_ERROR:   return "ERROR";
        default:         return "UNKNOWN";
    }
}

const char *bt_node_type_str(bt_node_type_t type)
{
    switch (type) {
        case BT_NODE_ACTION:        return "Action";
        case BT_NODE_CONDITION:     return "Condition";
        case BT_NODE_SEQUENCE:      return "Sequence";
        case BT_NODE_SELECTOR:      return "Selector";
        case BT_NODE_PARALLEL:      return "Parallel";
        case BT_NODE_INVERTER:      return "Inverter";
        case BT_NODE_REPEATER:      return "Repeater";
        case BT_NODE_RETRY:         return "Retry";
        case BT_NODE_FORCE_SUCCESS: return "ForceSuccess";
        case BT_NODE_FORCE_FAILURE: return "ForceFailure";
        default:                    return "Unknown";
    }
}

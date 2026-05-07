/**
 * @file   bt_internal.h
 * @brief  Internal declarations shared between library translation units.
 * @author Subhajit Roy <subhajitroy005@gmail.com>
 *
 * @warning NOT part of the public API.  Do not include this header from
 *          application code.  Only `behavior_tree.c`, `bt_composite.c`,
 *          and `bt_decorator.c` should include it.
 */
#ifndef BT_INTERNAL_H
#define BT_INTERNAL_H

#include "behavior_tree.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup BT_INTERNAL_TICKS Internal tick implementations
 * @brief Tick functions for composite and decorator nodes.
 *
 * These symbols are not part of the public API.  They are set as the
 * @c tick_fn of the respective node type by the factory functions in
 * `behavior_tree.c`.
 * @{
 */

/** @brief Tick implementation for @ref BT_NODE_SEQUENCE nodes. */
bt_status_t bt_sequence_tick     (bt_node_t *node, void *ctx);

/** @brief Tick implementation for @ref BT_NODE_SELECTOR nodes. */
bt_status_t bt_selector_tick     (bt_node_t *node, void *ctx);

/** @brief Tick implementation for @ref BT_NODE_PARALLEL nodes. */
bt_status_t bt_parallel_tick     (bt_node_t *node, void *ctx);

/** @brief Tick implementation for @ref BT_NODE_INVERTER nodes. */
bt_status_t bt_inverter_tick     (bt_node_t *node, void *ctx);

/** @brief Tick implementation for @ref BT_NODE_REPEATER nodes. */
bt_status_t bt_repeater_tick     (bt_node_t *node, void *ctx);

/** @brief Tick implementation for @ref BT_NODE_RETRY nodes. */
bt_status_t bt_retry_tick        (bt_node_t *node, void *ctx);

/** @brief Tick implementation for @ref BT_NODE_FORCE_SUCCESS nodes. */
bt_status_t bt_force_success_tick(bt_node_t *node, void *ctx);

/** @brief Tick implementation for @ref BT_NODE_FORCE_FAILURE nodes. */
bt_status_t bt_force_failure_tick(bt_node_t *node, void *ctx);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* BT_INTERNAL_H */

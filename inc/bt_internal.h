/**
 * @file  bt_internal.h
 * @brief Internal declarations shared between library translation units.
 *
 * NOT part of the public API – do not include from application code.
 */
#ifndef BT_INTERNAL_H
#define BT_INTERNAL_H

#include "behavior_tree.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Tick function prototypes — implemented in bt_composite.c / bt_decorator.c */
bt_status_t bt_sequence_tick     (bt_node_t *node, void *ctx);
bt_status_t bt_selector_tick     (bt_node_t *node, void *ctx);
bt_status_t bt_parallel_tick     (bt_node_t *node, void *ctx);
bt_status_t bt_inverter_tick     (bt_node_t *node, void *ctx);
bt_status_t bt_repeater_tick     (bt_node_t *node, void *ctx);
bt_status_t bt_retry_tick        (bt_node_t *node, void *ctx);
bt_status_t bt_force_success_tick(bt_node_t *node, void *ctx);
bt_status_t bt_force_failure_tick(bt_node_t *node, void *ctx);

/* Parallel node init — resets per-cycle child state */
void bt_parallel_on_init(bt_node_t *node, void *ctx);

#ifdef __cplusplus
}
#endif

#endif /* BT_INTERNAL_H */

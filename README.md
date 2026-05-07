# lib-behavior-tree

A generic, thread-safe behaviour tree library written in C, designed for embedded systems with or without an RTOS.

---

## Features

| Feature | Detail |
|---|---|
| **Language** | Pure C99 |
| **Memory** | Dynamic (configurable allocator — swap in `pvPortMalloc` / pool) |
| **Thread safety** | Per-tree recursive mutex; POSIX, FreeRTOS, or no-op (bare metal) |
| **Node types** | Action, Condition, Sequence, Selector, Parallel, Inverter, Repeater, Retry, ForceSuccess, ForceFailure |
| **Composite policy** | Stateful/memory — resumes from the last RUNNING child |
| **Footprint** | ~2 KB code; one `bt_node_t` ≈ 60–72 bytes depending on config |

---

## File layout

```
lib-behavior-tree/
├── inc/
│   ├── conf_behaviour_tree.h   ← copy & edit for your project
│   ├── behavior_tree.h         ← only header your app needs to include
│   ├── bt_port.h               ← platform abstraction (mutex)
│   └── bt_internal.h           ← internal use; do not include in app code
└── src/
    ├── behavior_tree.c         ← core: tree, node management, tick/halt
    ├── bt_composite.c          ← Sequence, Selector, Parallel
    ├── bt_decorator.c          ← Inverter, Repeater, Retry, Force*
    ├── bt_port_bare.c          ← bare-metal port (no-op mutex)
    ├── bt_port_posix.c         ← POSIX pthreads port
    └── bt_port_freertos.c      ← FreeRTOS port
```

---

## Integration

### Step 1 — Copy the configuration file

Copy `inc/conf_behaviour_tree.h` to your project's include path (wherever your
build system finds headers).  Adjust the settings inside.

```c
/* Select your platform */
#define BT_PLATFORM    BT_PLATFORM_FREERTOS   /* or POSIX, or BARE */

/* Optional: use the RTOS heap */
#define BT_MALLOC    pvPortMalloc
#define BT_FREE      vPortFree
```

### Step 2 — Add source files to your build

Compile these four files:

```
src/behavior_tree.c
src/bt_composite.c
src/bt_decorator.c
src/bt_port_<your_platform>.c   ← pick exactly one
```

Add `inc/` to your include search path.

### Step 3 — Include the public header

```c
#include "behavior_tree.h"
```

---

## Quick-start example

```c
#include "behavior_tree.h"
#include <stdio.h>

/* -----------------------------------------------------------------------
 * Application context passed to every callback.
 * ----------------------------------------------------------------------- */
typedef struct {
    int  battery_level;   /* 0-100 */
    bool target_reached;
} robot_ctx_t;

/* -----------------------------------------------------------------------
 * Condition: battery OK?
 * ----------------------------------------------------------------------- */
static bt_status_t cond_battery_ok(bt_node_t *node, void *ctx)
{
    (void)node;
    robot_ctx_t *r = (robot_ctx_t *)ctx;
    return (r->battery_level > 20) ? BT_SUCCESS : BT_FAILURE;
}

/* -----------------------------------------------------------------------
 * Action: navigate to target (simulates a long-running action).
 * ----------------------------------------------------------------------- */
static int nav_step = 0;

static void action_navigate_init(bt_node_t *node, void *ctx)
{
    (void)node; (void)ctx;
    nav_step = 0;
    printf("[Navigate] starting\n");
}

static bt_status_t action_navigate_tick(bt_node_t *node, void *ctx)
{
    (void)node;
    robot_ctx_t *r = (robot_ctx_t *)ctx;

    if (r->target_reached) {
        printf("[Navigate] target reached\n");
        return BT_SUCCESS;
    }

    printf("[Navigate] step %d\n", ++nav_step);
    return BT_RUNNING;
}

static void action_navigate_halt(bt_node_t *node, void *ctx)
{
    (void)node; (void)ctx;
    printf("[Navigate] interrupted\n");
}

/* -----------------------------------------------------------------------
 * Action: charge battery.
 * ----------------------------------------------------------------------- */
static bt_status_t action_charge(bt_node_t *node, void *ctx)
{
    (void)node;
    robot_ctx_t *r = (robot_ctx_t *)ctx;
    r->battery_level = 100;
    printf("[Charge] battery full\n");
    return BT_SUCCESS;
}

/* -----------------------------------------------------------------------
 * Build the tree:
 *
 *   Selector
 *   ├── Sequence           (navigate when battery OK)
 *   │   ├── Condition: battery OK?
 *   │   └── Action:    navigate
 *   └── Action: charge     (fallback)
 *
 * ----------------------------------------------------------------------- */
static bt_tree_t *build_robot_tree(robot_ctx_t *ctx)
{
    /* Leaf nodes */
    bt_node_t *cond = bt_condition_create("BatteryOK",
                                           cond_battery_ok, NULL);

    bt_node_t *nav  = bt_action_create("Navigate",
                                        action_navigate_tick,
                                        action_navigate_init,
                                        action_navigate_halt,
                                        NULL);

    bt_node_t *charge = bt_action_create("Charge",
                                          action_charge,
                                          NULL, NULL, NULL);

    /* Sequence: battery OK? → navigate */
    bt_node_t *seq = bt_sequence_create("NavSeq");
    bt_node_add_child(seq, cond);
    bt_node_add_child(seq, nav);

    /* Selector: try NavSeq, else charge */
    bt_node_t *sel = bt_selector_create("Root");
    bt_node_add_child(sel, seq);
    bt_node_add_child(sel, charge);

    /* thread_safe = true: safe to call bt_tree_tick from an RTOS task */
    return bt_tree_create(sel, ctx, true);
}

int main(void)
{
    robot_ctx_t robot = { .battery_level = 80, .target_reached = false };

    bt_tree_t *tree = build_robot_tree(&robot);

    /* Simulate a few ticks */
    for (int i = 0; i < 5; i++) {
        bt_status_t s = bt_tree_tick(tree);
        printf("tick %d → %s\n\n", i + 1, bt_status_str(s));

        if (i == 2) robot.target_reached = true;  /* arrive at target */
    }

    bt_tree_destroy(tree);
    return 0;
}
```

Build and run (Linux/macOS):

```sh
gcc -std=c99 -Iinc \
    src/behavior_tree.c src/bt_composite.c src/bt_decorator.c \
    src/bt_port_posix.c \
    example.c \
    -lpthread -o example
./example
```

Expected output:

```
[Navigate] starting
[Navigate] step 1
tick 1 → RUNNING

[Navigate] step 2
tick 2 → RUNNING

[Navigate] step 3
tick 3 → RUNNING

[Navigate] target reached
tick 4 → SUCCESS

[Navigate] starting
[Navigate] target reached
tick 5 → SUCCESS
```

---

## Node reference

### Status values

| Value | Meaning |
|---|---|
| `BT_SUCCESS` | Node completed successfully |
| `BT_FAILURE` | Node completed with failure |
| `BT_RUNNING` | Node is in progress (long-running action) |
| `BT_INVALID` | Node has not been ticked / was halted |
| `BT_ERROR` | Internal error (e.g. allocation failure, missing tick_fn) |

### Composite nodes

| Node | Symbol | Behaviour |
|---|---|---|
| **Sequence** | `→` | Ticks children left-to-right. Returns SUCCESS if all succeed; returns FAILURE on the first failure; returns RUNNING if a child is RUNNING. Resumes from the last RUNNING child on subsequent ticks (stateful/memory). |
| **Selector** | `?` | Ticks children left-to-right. Returns SUCCESS on the first success; returns FAILURE if all fail; returns RUNNING if a child is RUNNING. Resumes from the last RUNNING child (stateful/memory). |
| **Parallel** | `⇉` | Ticks all children every tick. Returns SUCCESS when `successes ≥ success_threshold`; returns FAILURE when success becomes impossible; returns RUNNING otherwise. |

### Decorator nodes

| Node | Behaviour |
|---|---|
| **Inverter** | SUCCESS ↔ FAILURE; RUNNING passes through. |
| **Repeater** | Repeats child `max_count` times (`-1` = forever). Returns FAILURE if child fails. |
| **Retry** | Retries child on FAILURE up to `max_retries` times (`-1` = unlimited). Returns SUCCESS immediately on first success. |
| **ForceSuccess** | Maps any terminal status to SUCCESS; RUNNING passes through. |
| **ForceFailure** | Maps any terminal status to FAILURE; RUNNING passes through. |

---

## Thread safety

```
bt_tree_create(root, context, true)   ← enables the mutex
```

When `thread_safe = true`:
- `bt_tree_tick` acquires a recursive mutex for the duration of the tick.
- The mutex is recursive so callbacks may safely call library utilities.
- `bt_tree_destroy` acquires then destroys the mutex.
- Node creation and `bt_node_add_child` are **not** protected; build the tree
  before sharing it between tasks.

For bare-metal or single-task usage pass `thread_safe = false` — no mutex
overhead.

---

## Callbacks

### `bt_tick_fn_t` — mandatory for leaf nodes

```c
bt_status_t my_action(bt_node_t *node, void *context)
{
    my_ctx_t *ctx = (my_ctx_t *)context;
    /* ... */
    return BT_RUNNING;   /* or BT_SUCCESS / BT_FAILURE */
}
```

### `bt_on_init_fn_t` — called once at the start of each execution cycle

```c
void my_action_init(bt_node_t *node, void *context)
{
    /* Reset timers, state-machine variables, etc. */
}
```

### `bt_on_halt_fn_t` — called when the node is forcibly interrupted

```c
void my_action_halt(bt_node_t *node, void *context)
{
    /* Cancel ongoing operations, release resources, etc. */
}
```

---

## Custom allocator (FreeRTOS example)

In `conf_behaviour_tree.h`:

```c
#define BT_PLATFORM    BT_PLATFORM_FREERTOS
#define BT_MALLOC      pvPortMalloc
#define BT_FREE        vPortFree
```

---

## Porting to a new RTOS

1. Add a `src/bt_port_myos.c` that implements:

```c
bool bt_port_mutex_create (bt_mutex_t *mutex);
void bt_port_mutex_destroy(bt_mutex_t *mutex);
void bt_port_mutex_lock   (bt_mutex_t *mutex);
void bt_port_mutex_unlock (bt_mutex_t *mutex);
```

2. In `bt_port.h` add a new `BT_PLATFORM_MYOS` constant and add the
   corresponding `typedef` for `bt_mutex_t`.

3. Set `#define BT_PLATFORM BT_PLATFORM_MYOS` in `conf_behaviour_tree.h`.

---

## License

See [LICENSE](LICENSE).

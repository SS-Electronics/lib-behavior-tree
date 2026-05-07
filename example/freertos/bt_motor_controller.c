/**
 * @file   bt_motor_controller.c
 * @brief  FreeRTOS example: 3-phase motor-drive safety supervisor.
 * @author Subhajit Roy <subhajitroy005@gmail.com>
 *
 * @details
 * Demonstrates lib-behavior-tree running inside a FreeRTOS application with
 * two concurrent tasks:
 *
 *  - **vSensorTask** (10 ms period): Reads ADC values (current, temperature)
 *    and a digital input (emergency-stop) into a shared @ref motor_ctx_t.
 *  - **vBtTask** (20 ms period): Ticks the behaviour tree.  The tree mutex
 *    provides the thread-safety boundary between the tasks.
 *
 * Behaviour tree topology
 * @code
 *   Selector [root]
 *   ├── Parallel [monitored_op, threshold=3]   ← all 3 branches must not fail
 *   │   ├── Condition : estop_clear             ← re-evaluated every tick
 *   │   ├── Condition : temp_ok                 ← re-evaluated every tick
 *   │   └── Sequence  [run_motor]               ← stateful (memory) action chain
 *   │       ├── Action : enable_drive   (one-shot SUCCESS; idempotent)
 *   │       └── Action : ramp_speed     (RUNNING while ramping, then holds RUNNING)
 *   └── Sequence [fault_response]
 *       ├── Action    : disable_drive
 *       └── Action    : set_fault_led
 * @endcode
 *
 * The Parallel node re-ticks all three children on every tree tick.
 * As soon as either condition returns FAILURE (E-stop pressed, over-temp),
 * the Parallel fails, which causes the Selector to fall through to the
 * fault-response branch.  This is the canonical BT safety-monitoring pattern.
 *
 * Simulation
 * ----------
 * The example ships with freertos_sim.h, a thin POSIX wrapper that maps
 * FreeRTOS types and APIs to pthreads so the code can be compiled and run on
 * Linux / macOS without real hardware.
 *
 * @code
 *   gcc -std=c99 -Wall -Wextra \
 *       -I../../inc -I. \
 *       ../../src/behavior_tree.c \
 *       ../../src/bt_composite.c  \
 *       ../../src/bt_decorator.c  \
 *       ../../src/bt_port_freertos.c \
 *       bt_motor_controller.c \
 *       -lpthread -o motor_controller
 *   ./motor_controller
 * @endcode
 *
 * Porting to real hardware
 * ------------------------
 * 1. Remove freertos_sim.h / FreeRTOS.h / semphr.h from the include path.
 * 2. Replace the simulated sensor reads with actual HAL / driver calls.
 * 3. Replace the simulated actuator writes with PWM / GPIO writes.
 * 4. Adjust task stack sizes and priorities for your RTOS configuration.
 */

/* Pull in the FreeRTOS simulation before any FreeRTOS header is needed.
 * On a real target, remove this include; FreeRTOS.h comes from the BSP. */
#include "freertos_sim.h"

#include "behavior_tree.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ===========================================================================
 *  Shared application context
 *  Updated by vSensorTask; read by every BT callback.
 *  Protected access is guaranteed because bt_tree_tick holds the tree mutex
 *  and vSensorTask acquires the same mutex before writing sensor data.
 * ========================================================================= */

/** Runtime sensor and actuator state shared between tasks. */
typedef struct {
    /* --- Sensor inputs (written by vSensorTask) --- */
    bool    estop_active;      /**< TRUE when E-stop button is pressed         */
    float   temperature_c;     /**< Drive board temperature in °C              */
    float   phase_current_a;   /**< RMS phase current reading in Amperes       */

    /* --- Actuator outputs (written by BT callbacks) --- */
    bool    drive_enabled;     /**< TRUE when PWM gate signals are active      */
    float   speed_setpoint;    /**< Target speed in RPM                        */
    float   speed_actual;      /**< Current motor speed in RPM (simulated)     */
    bool    fault_led;         /**< TRUE when fault indicator LED is on        */

    /* --- Tree mutex shared with the sensor task for context protection --- */
    SemaphoreHandle_t ctx_mutex;
} motor_ctx_t;

/* ===========================================================================
 *  Configuration constants
 * ========================================================================= */
#define TEMP_LIMIT_C          85.0f   /**< Thermal shutdown threshold          */
#define SPEED_TARGET_RPM    1500.0f   /**< Nominal operating speed             */
#define RAMP_RATE_RPM_PER_TICK  50.0f /**< Speed increase per BT tick          */
#define SENSOR_TASK_PERIOD_MS   10U   /**< vSensorTask period                  */
#define BT_TASK_PERIOD_MS       20U   /**< vBtTask period                      */
#define SIM_DURATION_MS       1200U   /**< Simulation stops after this long    */

/* ===========================================================================
 *  Condition callbacks
 * ========================================================================= */

/**
 * @brief Condition — E-stop is not active.
 * Returns BT_SUCCESS when the drive is safe to enable.
 */
static bt_status_t cond_estop_clear(bt_node_t *node, void *ctx)
{
    (void)node;
    const motor_ctx_t *m = (const motor_ctx_t *)ctx;
    if (m->estop_active) {
        printf("  [COND] E-stop ACTIVE — blocking normal operation\n");
        return BT_FAILURE;
    }
    return BT_SUCCESS;
}

/**
 * @brief Condition — Drive temperature is within limits.
 */
static bt_status_t cond_temp_ok(bt_node_t *node, void *ctx)
{
    (void)node;
    const motor_ctx_t *m = (const motor_ctx_t *)ctx;
    if (m->temperature_c >= TEMP_LIMIT_C) {
        printf("  [COND] Over-temperature (%.1f °C) — blocking normal operation\n",
               m->temperature_c);
        return BT_FAILURE;
    }
    return BT_SUCCESS;
}

/* ===========================================================================
 *  Action callbacks — normal operation branch
 * ========================================================================= */

/**
 * @brief Action — enable the PWM gate drive (one-shot).
 * Always returns BT_SUCCESS; repeated ticks are harmless.
 */
static bt_status_t action_enable_drive(bt_node_t *node, void *ctx)
{
    (void)node;
    motor_ctx_t *m = (motor_ctx_t *)ctx;
    if (!m->drive_enabled) {
        m->drive_enabled = true;
        printf("  [ACT ] Drive enabled (PWM on)\n");
    }
    return BT_SUCCESS;
}

/**
 * @brief on_init for ramp_speed — resets the ramp each new cycle.
 */
static void action_ramp_init(bt_node_t *node, void *ctx)
{
    (void)node;
    const motor_ctx_t *m = (const motor_ctx_t *)ctx;
    printf("  [INIT] Speed ramp starting from %.0f RPM\n", m->speed_actual);
}

/**
 * @brief Action — linearly ramp motor speed to SPEED_TARGET_RPM.
 *
 * Returns BT_RUNNING while still accelerating.
 * Returns BT_SUCCESS when the setpoint is reached.
 */
static bt_status_t action_ramp_speed(bt_node_t *node, void *ctx)
{
    (void)node;
    motor_ctx_t *m = (motor_ctx_t *)ctx;

    if (m->speed_actual < SPEED_TARGET_RPM) {
        m->speed_actual += RAMP_RATE_RPM_PER_TICK;
        if (m->speed_actual > SPEED_TARGET_RPM)
            m->speed_actual = SPEED_TARGET_RPM;
        printf("  [ACT ] Ramping: %.0f / %.0f RPM\n",
               m->speed_actual, SPEED_TARGET_RPM);
        return BT_RUNNING;
    }

    /* Hold at setpoint — return RUNNING to keep the Parallel alive. */
    return BT_RUNNING;
}

/**
 * @brief on_halt for ramp_speed — motor is coasted to a safe state.
 */
static void action_ramp_halt(bt_node_t *node, void *ctx)
{
    (void)node;
    motor_ctx_t *m = (motor_ctx_t *)ctx;
    printf("  [HALT] Ramp interrupted at %.0f RPM — coasting\n", m->speed_actual);
    m->speed_actual = 0.0f;
}

/* ===========================================================================
 *  Action callbacks — fault response branch
 * ========================================================================= */

/**
 * @brief Action — disable PWM output and zero the speed demand.
 */
static bt_status_t action_disable_drive(bt_node_t *node, void *ctx)
{
    (void)node;
    motor_ctx_t *m = (motor_ctx_t *)ctx;
    if (m->drive_enabled) {
        m->drive_enabled = false;
        m->speed_actual  = 0.0f;
        printf("  [ACT ] Drive DISABLED (fault)\n");
    }
    return BT_SUCCESS;
}

/**
 * @brief Action — illuminate the fault indicator LED.
 */
static bt_status_t action_fault_led(bt_node_t *node, void *ctx)
{
    (void)node;
    motor_ctx_t *m = (motor_ctx_t *)ctx;
    if (!m->fault_led) {
        m->fault_led = true;
        printf("  [ACT ] Fault LED ON\n");
    }
    return BT_SUCCESS;
}

/* ===========================================================================
 *  Tree construction
 * ========================================================================= */

/**
 * @brief Build the motor-controller behaviour tree.
 *
 * @code
 *   Selector [root]
 *   ├── Sequence [normal_op]
 *   │   ├── Condition : estop_clear
 *   │   ├── Condition : temp_ok
 *   │   ├── Action    : enable_drive
 *   │   └── Action    : ramp_speed
 *   └── Sequence [fault_response]
 *       ├── Action    : disable_drive
 *       └── Action    : set_fault_led
 * @endcode
 *
 * thread_safe = true so bt_tree_tick can be called from vBtTask while
 * vSensorTask simultaneously updates the shared context (after acquiring
 * the same ctx_mutex).
 */
static bt_tree_t *build_motor_tree(motor_ctx_t *ctx)
{
    /* --- Motor run sequence (stateful/memory: enable once, ramp, hold) --- */
    bt_node_t *act_en   = bt_action_create("enable_drive",
                                            action_enable_drive,
                                            NULL, NULL, NULL);
    bt_node_t *act_ramp = bt_action_create("ramp_speed",
                                            action_ramp_speed,
                                            action_ramp_init,
                                            action_ramp_halt,
                                            NULL);
    bt_node_t *seq_run  = bt_sequence_create("run_motor");
    bt_node_add_child(seq_run, act_en);
    bt_node_add_child(seq_run, act_ramp);

    /* --- Parallel monitor: all three branches must not fail.
     *     threshold = 3  →  fails if ANY of the three branches fails.
     *     The two conditions are re-evaluated on every tick (reactive).
     *     The motor sequence runs alongside and is halted on any fault. --- */
    bt_node_t *cond_es   = bt_condition_create("estop_clear",
                                                cond_estop_clear, NULL);
    bt_node_t *cond_temp = bt_condition_create("temp_ok",
                                                cond_temp_ok, NULL);
    bt_node_t *par_mon   = bt_parallel_create("monitored_op", /*threshold=*/3);
    bt_node_add_child(par_mon, cond_es);
    bt_node_add_child(par_mon, cond_temp);
    bt_node_add_child(par_mon, seq_run);

    /* --- Fault response branch --- */
    bt_node_t *act_dis = bt_action_create("disable_drive",
                                           action_disable_drive,
                                           NULL, NULL, NULL);
    bt_node_t *act_led = bt_action_create("set_fault_led",
                                           action_fault_led,
                                           NULL, NULL, NULL);
    bt_node_t *seq_fault = bt_sequence_create("fault_response");
    bt_node_add_child(seq_fault, act_dis);
    bt_node_add_child(seq_fault, act_led);

    /* --- Root selector: try normal operation, fall back to fault response --- */
    bt_node_t *sel_root = bt_selector_create("root");
    bt_node_add_child(sel_root, par_mon);
    bt_node_add_child(sel_root, seq_fault);

    return bt_tree_create(sel_root, ctx, /*thread_safe=*/true);
}

/* ===========================================================================
 *  FreeRTOS tasks
 * ========================================================================= */

/** Task parameters passed at creation time. */
typedef struct {
    bt_tree_t   *tree;
    motor_ctx_t *ctx;
} task_params_t;

/**
 * @brief Sensor task — simulates ADC reads and digital input sampling.
 *
 * Runs at SENSOR_TASK_PERIOD_MS.  Acquires ctx_mutex before writing to
 * prevent torn reads by vBtTask.
 *
 * In this simulation:
 *   - Temperature rises slowly.
 *   - E-stop fires briefly at t = 600 ms.
 *   - Temperature over-limit fires at t = 900 ms.
 */
static void vSensorTask(void *param)
{
    task_params_t *p   = (task_params_t *)param;
    motor_ctx_t   *ctx = p->ctx;
    uint32_t       tick = 0;

    printf("[SensorTask] started\n");

    while (tick * SENSOR_TASK_PERIOD_MS < SIM_DURATION_MS) {
        vTaskDelay(pdMS_TO_TICKS(SENSOR_TASK_PERIOD_MS));
        tick++;

        xSemaphoreTakeRecursive(ctx->ctx_mutex, portMAX_DELAY);

        /* Simulate rising temperature */
        ctx->temperature_c += 0.5f;

        /* Simulate a 200 ms E-stop event at 600 ms */
        uint32_t t_ms = tick * SENSOR_TASK_PERIOD_MS;
        ctx->estop_active = (t_ms >= 600 && t_ms < 800);

        /* Simulate thermal shutdown after 900 ms */
        if (t_ms >= 900) ctx->temperature_c = 90.0f;

        /* Simulate phase current reading */
        ctx->phase_current_a = ctx->drive_enabled ? 12.5f : 0.0f;

        xSemaphoreGiveRecursive(ctx->ctx_mutex);
    }

    printf("[SensorTask] simulation complete\n");
}

/**
 * @brief Behaviour-tree task — ticks the tree at BT_TASK_PERIOD_MS.
 *
 * The tree was created with thread_safe = true, so bt_tree_tick internally
 * holds the tree mutex.  Sensor data protected by ctx_mutex (different lock)
 * is updated between ticks.
 */
static void vBtTask(void *param)
{
    task_params_t *p    = (task_params_t *)param;
    bt_tree_t     *tree = p->tree;
    motor_ctx_t   *ctx  = p->ctx;
    uint32_t       tick = 0;

    printf("[BtTask] started  (period: %u ms)\n", BT_TASK_PERIOD_MS);

    while (tick * BT_TASK_PERIOD_MS < SIM_DURATION_MS) {
        vTaskDelay(pdMS_TO_TICKS(BT_TASK_PERIOD_MS));
        tick++;

        /* Acquire ctx_mutex to snapshot consistent sensor data, then tick */
        xSemaphoreTakeRecursive(ctx->ctx_mutex, portMAX_DELAY);
        bt_status_t s = bt_tree_tick(tree);
        xSemaphoreGiveRecursive(ctx->ctx_mutex);

        printf("[BtTask] tick %3u → %-8s  speed=%.0f RPM  temp=%.1f°C  "
               "estop=%d  drive=%d\n",
               tick, bt_status_str(s),
               ctx->speed_actual, ctx->temperature_c,
               (int)ctx->estop_active, (int)ctx->drive_enabled);
    }

    printf("[BtTask] simulation complete  (%u total ticks)\n", tick);
#if BT_ENABLE_STATS
    printf("[BtTask] tree tick count=%u  last_status=%s\n",
           tree->tick_count, bt_status_str(tree->last_status));
#endif
}

/* ===========================================================================
 *  Entry point
 * ========================================================================= */

int main(void)
{
    printf("=== Motor Controller — lib-behavior-tree FreeRTOS Example ===\n\n");

    /* Initialise shared context */
    motor_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.temperature_c  = 25.0f;
    ctx.speed_setpoint = SPEED_TARGET_RPM;

    ctx.ctx_mutex = xSemaphoreCreateRecursiveMutex();
    if (!ctx.ctx_mutex) {
        fprintf(stderr, "Failed to create context mutex\n");
        return 1;
    }

    /* Build the tree (thread_safe=true creates its own internal mutex) */
    bt_tree_t *tree = build_motor_tree(&ctx);
    if (!tree) {
        fprintf(stderr, "Failed to build behaviour tree\n");
        vSemaphoreDelete(ctx.ctx_mutex);
        return 1;
    }

    /* Task parameters (stack-allocated; both tasks finish before main exits) */
    task_params_t params = { .tree = tree, .ctx = &ctx };

    /* Create FreeRTOS tasks */
    TaskHandle_t sensor_handle, bt_handle;

    if (xTaskCreate(vSensorTask, "SensorTask", 512, &params, 2,
                    &sensor_handle) != pdPASS) {
        fprintf(stderr, "Failed to create sensor task\n");
        return 1;
    }
    if (xTaskCreate(vBtTask, "BtTask", 1024, &params, 3,
                    &bt_handle) != pdPASS) {
        fprintf(stderr, "Failed to create BT task\n");
        return 1;
    }

    /* In a real FreeRTOS application vTaskStartScheduler() never returns.
     * In simulation we join the created pthreads to wait for completion. */
    pthread_join(*((pthread_t *)sensor_handle), NULL);
    pthread_join(*((pthread_t *)bt_handle),     NULL);

    /* Teardown */
    bt_tree_destroy(tree);
    vSemaphoreDelete(ctx.ctx_mutex);

    printf("\n=== Simulation finished ===\n");
    return 0;
}

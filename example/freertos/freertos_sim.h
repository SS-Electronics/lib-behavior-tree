/**
 * @file   freertos_sim.h
 * @brief  Minimal FreeRTOS API simulation using POSIX pthreads.
 * @author Subhajit Roy <subhajitroy005@gmail.com>
 *
 * Allows the FreeRTOS example to compile and run on Linux / macOS without a
 * real FreeRTOS environment.  On a real embedded target, remove this file
 * from the build; the toolchain's FreeRTOS port provides the same symbols.
 *
 * Implemented subset:
 *   - Recursive mutex (SemaphoreHandle_t)
 *   - Task creation (xTaskCreate / vTaskDelete)
 *   - Task delay (vTaskDelay / pdMS_TO_TICKS)
 *   - Heap wrappers (pvPortMalloc / vPortFree → malloc / free)
 *   - Scheduler entry point stub (vTaskStartScheduler)
 */
#ifndef FREERTOS_SIM_H
#define FREERTOS_SIM_H

#include <pthread.h>
#include <time.h>     /* nanosleep */
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>

/* ===========================================================================
 *  Basic types
 * ========================================================================= */
typedef int        BaseType_t;
typedef uint32_t   TickType_t;
typedef uint32_t   UBaseType_t;
typedef uint16_t   StackType_t;

#define pdTRUE     ((BaseType_t)1)
#define pdFALSE    ((BaseType_t)0)
#define pdPASS     pdTRUE
#define pdFAIL     pdFALSE

#define portMAX_DELAY         ((TickType_t)0xFFFFFFFFUL)
#define configTICK_RATE_HZ    1000U

/** Convert milliseconds to RTOS ticks (1 ms per tick in simulation). */
#define pdMS_TO_TICKS(ms)    ((TickType_t)(ms))

/* ===========================================================================
 *  Heap (redirect to standard allocator)
 * ========================================================================= */
#define pvPortMalloc    malloc
#define vPortFree       free

/* ===========================================================================
 *  Recursive mutex (SemaphoreHandle_t → pthread_mutex_t *)
 * ========================================================================= */
typedef pthread_mutex_t *SemaphoreHandle_t;

static inline SemaphoreHandle_t xSemaphoreCreateRecursiveMutex(void)
{
    pthread_mutexattr_t  attr;
    pthread_mutex_t     *m = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
    if (!m) return NULL;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE_NP); /* Linux NP variant; no feature-test required */
    if (pthread_mutex_init(m, &attr) != 0) { free(m); m = NULL; }
    pthread_mutexattr_destroy(&attr);
    return m;
}

static inline BaseType_t xSemaphoreTakeRecursive(SemaphoreHandle_t s,
                                                  TickType_t        timeout)
{
    (void)timeout;
    return (pthread_mutex_lock(s) == 0) ? pdTRUE : pdFALSE;
}

static inline BaseType_t xSemaphoreGiveRecursive(SemaphoreHandle_t s)
{
    return (pthread_mutex_unlock(s) == 0) ? pdTRUE : pdFALSE;
}

static inline void vSemaphoreDelete(SemaphoreHandle_t s)
{
    if (s) { pthread_mutex_destroy(s); free(s); }
}

/* ===========================================================================
 *  Task creation / deletion
 * ========================================================================= */
typedef void (*TaskFunction_t)(void *param);
typedef void *TaskHandle_t;

typedef struct { TaskFunction_t fn; void *arg; } _sim_task_arg_t;

static void *_sim_task_trampoline(void *arg)
{
    _sim_task_arg_t *ta = (_sim_task_arg_t *)arg;
    ta->fn(ta->arg);
    free(ta);
    return NULL;
}

static inline BaseType_t xTaskCreate(TaskFunction_t    fn,
                                      const char       *name,
                                      uint16_t          stack_depth,
                                      void             *param,
                                      UBaseType_t       priority,
                                      TaskHandle_t     *out_handle)
{
    (void)name; (void)stack_depth; (void)priority;
    _sim_task_arg_t *ta = (_sim_task_arg_t *)malloc(sizeof(_sim_task_arg_t));
    if (!ta) return pdFAIL;
    ta->fn  = fn;
    ta->arg = param;
    pthread_t *t = (pthread_t *)malloc(sizeof(pthread_t));
    if (!t) { free(ta); return pdFAIL; }
    if (pthread_create(t, NULL, _sim_task_trampoline, ta) != 0) {
        free(ta); free(t); return pdFAIL;
    }
    if (out_handle) *out_handle = (TaskHandle_t)t;
    else            pthread_detach(*t);
    return pdPASS;
}

static inline void vTaskDelete(TaskHandle_t handle) { (void)handle; }

/* ===========================================================================
 *  Timing
 * ========================================================================= */

/** Block the calling task for @p ticks RTOS ticks (1 tick = 1 ms here). */
static inline void vTaskDelay(TickType_t ticks)
{
    struct timespec ts;
    ts.tv_sec  = (time_t)(ticks / 1000U);
    ts.tv_nsec = (long)((ticks % 1000U) * 1000000L);
    nanosleep(&ts, NULL);
}

/* ===========================================================================
 *  Scheduler start (no-op in simulation — main() blocks via pthread_join)
 * ========================================================================= */
static inline void vTaskStartScheduler(void) { /* POSIX threads are already running */ }

#endif /* FREERTOS_SIM_H */

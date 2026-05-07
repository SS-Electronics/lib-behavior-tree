/**
 * @file   bt_port_freertos.c
 * @brief  FreeRTOS port — recursive mutex via `xSemaphoreCreateRecursiveMutex`.
 * @author Subhajit Roy <subhajitroy005@gmail.com>
 *
 * @ingroup BT_PORT
 *
 * @details
 * Compile this file when `BT_PLATFORM == BT_PLATFORM_FREERTOS`.
 *
 * **FreeRTOS configuration requirement:**
 * `configUSE_RECURSIVE_MUTEXES` must be set to `1` in `FreeRTOSConfig.h`.
 *
 * **Recursion:** A recursive mutex is used so that callbacks executing inside
 * `bt_tree_tick` can safely call any library function that also acquires the
 * tree mutex without deadlocking.
 *
 * **Simulation:** When building the FreeRTOS example on Linux / macOS, the
 * `example/freertos/FreeRTOS.h` and `semphr.h` stubs redirect to
 * `freertos_sim.h`, which implements the FreeRTOS API using POSIX pthreads.
 */
#include "bt_port.h"

#if (BT_PLATFORM == BT_PLATFORM_FREERTOS)

/**
 * @brief Create a FreeRTOS recursive mutex.
 *
 * @param[out] mutex  Pointer to the `SemaphoreHandle_t` to initialise.
 * @return @c true on success; @c false if `xSemaphoreCreateRecursiveMutex`
 *         returns @c NULL (insufficient heap memory).
 */
bool bt_port_mutex_create(bt_mutex_t *mutex)
{
    *mutex = xSemaphoreCreateRecursiveMutex();
    return (*mutex != NULL);
}

/**
 * @brief Delete a FreeRTOS recursive mutex and return its memory to the heap.
 *
 * @param[in,out] mutex  Mutex handle to delete.  Set to @c NULL after deletion.
 */
void bt_port_mutex_destroy(bt_mutex_t *mutex)
{
    if (*mutex) {
        vSemaphoreDelete(*mutex);
        *mutex = NULL;
    }
}

/**
 * @brief Acquire the mutex (blocking, waits indefinitely).
 *
 * Calls `xSemaphoreTakeRecursive` with `portMAX_DELAY`.
 *
 * @param[in,out] mutex  Mutex handle to take.
 */
void bt_port_mutex_lock(bt_mutex_t *mutex)
{
    xSemaphoreTakeRecursive(*mutex, portMAX_DELAY);
}

/**
 * @brief Release the mutex.
 *
 * @param[in,out] mutex  Mutex handle to give.
 */
void bt_port_mutex_unlock(bt_mutex_t *mutex)
{
    xSemaphoreGiveRecursive(*mutex);
}

#endif /* BT_PLATFORM_FREERTOS */

/**
 * @file  bt_port_freertos.c
 * @brief FreeRTOS port — uses a recursive mutex (SemaphoreHandle_t).
 *
 * Use when BT_PLATFORM == BT_PLATFORM_FREERTOS.
 * Requires FreeRTOS with configUSE_RECURSIVE_MUTEXES = 1.
 */
#include "bt_port.h"

#if (BT_PLATFORM == BT_PLATFORM_FREERTOS)

bool bt_port_mutex_create(bt_mutex_t *mutex)
{
    *mutex = xSemaphoreCreateRecursiveMutex();
    return (*mutex != NULL);
}

void bt_port_mutex_destroy(bt_mutex_t *mutex)
{
    if (*mutex) {
        vSemaphoreDelete(*mutex);
        *mutex = NULL;
    }
}

void bt_port_mutex_lock(bt_mutex_t *mutex)
{
    xSemaphoreTakeRecursive(*mutex, portMAX_DELAY);
}

void bt_port_mutex_unlock(bt_mutex_t *mutex)
{
    xSemaphoreGiveRecursive(*mutex);
}

#endif /* BT_PLATFORM_FREERTOS */

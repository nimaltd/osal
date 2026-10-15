/**
 * @file        osal_cmsis_v2.h
 * @brief       The osal port for FreeRTOS through CMSIS-RTOS v2.
 * @version     1.1.0
 *
 * @author      Nima Askari (NimaLTD)
 * @email       nima.askari@gmail.com
 * @github      https://www.github.com/nimaltd
 * @linkedin    https://www.linkedin.com/in/nimaltd
 * @youtube     https://www.youtube.com/@nimaltd
 * @instagram   https://instagram.com/github.nimaltd
 *
 * @copyright   (c) 2026 Nima Askari (NimaLTD)
 *              SPDX-License-Identifier: Apache-2.0
 *              See LICENSE.md in the project root for the full license text.
 *
 * @note        Included by osal.h when OSAL_RTOS is OSAL_RTOS_CMSIS_V2. Include
 *              osal.h, never this file. CMSIS-RTOS v2 takes ticks, and
 *              milliseconds become ticks through FreeRTOS's own
 *              pdMS_TO_TICKS(), which a project can define its own way in
 *              FreeRTOSConfig.h.
 */

#ifndef OSAL_CMSIS_V2_H
#define OSAL_CMSIS_V2_H

/*
 * ****************************************************************************************************
 * Includes
 * ****************************************************************************************************
*/

#include "FreeRTOS.h"
#include "cmsis_os2.h"

/*
 * ****************************************************************************************************
 * Configuration checks
 * ****************************************************************************************************
*/

#if !defined(OSAL_H) || (OSAL_RTOS != OSAL_RTOS_CMSIS_V2)
#error "osal_cmsis_v2.h is included by osal.h when OSAL_RTOS is OSAL_RTOS_CMSIS_V2. Include osal.h"
#endif

#ifdef __cplusplus
extern "C"
{
#endif

/*
 * ****************************************************************************************************
 * Types
 * ****************************************************************************************************
*/

/*****************************************************************************************************/
/**
 * @brief A mutex. Put one wherever it is needed, a struct member included.
 */
typedef struct
{
    osMutexId_t mutex; /**< The RTOS mutex. */

} osal_mutex_t;

/*
 * ****************************************************************************************************
 * Private function prototypes
 * ****************************************************************************************************
*/

static inline uint32_t osal_ticks(uint32_t ms);

/*
 * ****************************************************************************************************
 * Public function implementations
 * ****************************************************************************************************
*/

/*****************************************************************************************************/
/**
 * @brief Let the other threads of the same priority run, and come back at once if none is waiting.
 *
 * For a loop that keeps the CPU busy itself, such as a read by polling in
 * pieces: between two pieces the threads of the same priority get a turn, and
 * when none is waiting it costs a few microseconds. A thread of lower priority
 * never runs, so a loop that waits on hardware calls osal_pause() instead.
 */
static inline void osal_yield(void)
{
    (void)osThreadYield();
}

/*****************************************************************************************************/
/**
 * @brief Pause a loop that waits on hardware, so every other thread runs meanwhile.
 *
 * Sleeps one tick, 1 ms at the usual 1 kHz, the shortest sleep the RTOS has,
 * so threads of every priority run. Call it between two questions to a chip or
 * a DMA that is still busy. A yield would let no thread of lower priority run
 * until the wait ends, which for an erase can be minutes.
 */
static inline void osal_pause(void)
{
    (void)osDelay(1U);
}

/*****************************************************************************************************/
/**
 * @brief Create a mutex. Call it once per mutex.
 *
 * @param[out] mutex  Mutex to create.
 * @return OSAL_ERR_NONE, OSAL_ERR_INVALID or OSAL_ERR_MUTEX.
 */
static inline osal_err_t osal_mutex_create(osal_mutex_t *mutex)
{
    osal_err_t err = OSAL_ERR_INVALID;

    if (mutex != NULL)
    {
        /* Priority inheritance, so a low priority thread holding the mutex is
           not held up by a medium one while a high priority thread waits for
           it. */
        const osMutexAttr_t attr = { "osal", osMutexPrioInherit, NULL, 0U };

        mutex->mutex = osMutexNew(&attr);
        err          = OSAL_ERR_NONE;

        /* NULL most often means the RTOS heap is too small. */
        if (mutex->mutex == NULL)
        {
            err = OSAL_ERR_MUTEX;
        }
    }

    return err;
}

/*****************************************************************************************************/
/**
 * @brief Take a mutex, waiting up to timeout_ms for another thread to give it back.
 *
 * @param[in,out] mutex       Mutex from osal_mutex_create().
 * @param[in]     timeout_ms  How long to wait. HAL_MAX_DELAY waits for ever.
 * @return OSAL_ERR_NONE, OSAL_ERR_INVALID, OSAL_ERR_TIMEOUT or OSAL_ERR_MUTEX.
 */
static inline osal_err_t osal_mutex_lock(osal_mutex_t *mutex, uint32_t timeout_ms)
{
    osal_err_t err = OSAL_ERR_INVALID;

    if (mutex != NULL)
    {
        uint32_t   ticks  = osWaitForever;
        osStatus_t status = osError;

        /* HAL_MAX_DELAY waits for ever. Anything else becomes ticks. */
        if (timeout_ms != HAL_MAX_DELAY)
        {
            ticks = osal_ticks(timeout_ms);
        }

        status = osMutexAcquire(mutex->mutex, ticks);

        if (status == osOK)
        {
            /* Taken. */
            err = OSAL_ERR_NONE;
        }
        else if ((status == osErrorTimeout) || (status == osErrorResource))
        {
            /* Another thread kept it for the whole wait. */
            err = OSAL_ERR_TIMEOUT;
        }
        else
        {
            /* Refused outright, such as from an interrupt. */
            err = OSAL_ERR_MUTEX;
        }
    }

    return err;
}

/*****************************************************************************************************/
/**
 * @brief Give back a mutex taken with osal_mutex_lock().
 *
 * @param[in,out] mutex  Mutex from osal_mutex_create().
 */
static inline void osal_mutex_unlock(osal_mutex_t *mutex)
{
    if (mutex != NULL)
    {
        (void)osMutexRelease(mutex->mutex);
    }
}

/*****************************************************************************************************/
/**
 * @brief Wait ms milliseconds, letting other threads run.
 *
 * @param[in] ms  Milliseconds. 0 returns at once, and HAL_MAX_DELAY never returns.
 */
static inline void osal_delay_ms(uint32_t ms)
{
    if (ms == HAL_MAX_DELAY)
    {
        /* For ever, as HAL_MAX_DELAY is for a mutex. One sleep, even the
           longest, ends after 49 days, so the thread sleeps again. */
        for (;;)
        {
            (void)osDelay(osWaitForever);
        }
    }
    else if (ms > 0U)
    {
        (void)osDelay(osal_ticks(ms));
    }
    else
    {
        /* 0 returns at once, since a zero sleep is not the same on every RTOS. */
    }
}

/*
 * ****************************************************************************************************
 * Private function implementations
 * ****************************************************************************************************
*/

/*****************************************************************************************************/
/**
 * @brief Turn milliseconds into ticks with FreeRTOS's pdMS_TO_TICKS(), and never a wait into none.
 *
 * @param[in] ms  Milliseconds.
 * @return Ticks: what pdMS_TO_TICKS() gives, and at least one for any wait.
 */
static inline uint32_t osal_ticks(uint32_t ms)
{
    uint32_t ticks = (uint32_t)pdMS_TO_TICKS(ms);

    /* pdMS_TO_TICKS() drops what is left of a tick, so on a 100 Hz tick 5 ms
       is no tick at all, and a sleep or a mutex wait would end at once. A
       wait that was asked for gets one tick at least. */
    if ((ticks == 0U) && (ms > 0U))
    {
        ticks = 1U;
    }

    return ticks;
}

#ifdef __cplusplus
}
#endif

#endif /* OSAL_CMSIS_V2_H */

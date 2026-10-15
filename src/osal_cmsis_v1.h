/**
 * @file        osal_cmsis_v1.h
 * @brief       The osal port for FreeRTOS through CMSIS-RTOS v1.
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
 * @note        Included by osal.h when OSAL_RTOS is OSAL_RTOS_CMSIS_V1. Include
 *              osal.h, never this file. CMSIS-RTOS v1 takes milliseconds and
 *              turns them into ticks itself, so every time goes in as it is.
 *              The mutex is FreeRTOS's recursive one, through ST's
 *              osRecursiveMutex calls, as a ThreadX mutex is.
 */

#ifndef OSAL_CMSIS_V1_H
#define OSAL_CMSIS_V1_H

/*
 * ****************************************************************************************************
 * Includes
 * ****************************************************************************************************
*/

#include "cmsis_os.h"

/*
 * ****************************************************************************************************
 * Configuration checks
 * ****************************************************************************************************
*/

#if !defined(OSAL_H) || (OSAL_RTOS != OSAL_RTOS_CMSIS_V1)
#error "osal_cmsis_v1.h is included by osal.h when OSAL_RTOS is OSAL_RTOS_CMSIS_V1. Include osal.h"
#endif

/* cmsis_os.h brings FreeRTOS.h. Without recursive mutexes, ST's layer makes
   no mutex at all, and every osal_mutex_create() would fail. */
#if !defined(configUSE_RECURSIVE_MUTEXES) || (configUSE_RECURSIVE_MUTEXES != 1)
#error "osal needs configUSE_RECURSIVE_MUTEXES 1: in CubeMX, FreeRTOS, Config parameters, USE_RECURSIVE_MUTEXES Enabled"
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
    osMutexId mutex; /**< The RTOS mutex. */

} osal_mutex_t;

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
    if (!osal_in_interrupt())
    {
        (void)osThreadYield();
    }
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
    /* 1 ms, which FreeRTOS's CMSIS-RTOS v1 layer makes one tick at least. ST's
       osDelay() does not check for an interrupt, so osal does. */
    if (!osal_in_interrupt())
    {
        (void)osDelay(1U);
    }
}

/*****************************************************************************************************/
/**
 * @brief Create a mutex. A second call on the same one gives it back as new.
 *
 * @param[out] mutex  Mutex to create.
 * @return OSAL_ERR_NONE, OSAL_ERR_INVALID or OSAL_ERR_MUTEX.
 */
static inline osal_err_t osal_mutex_create(osal_mutex_t *mutex)
{
    osal_err_t err = OSAL_ERR_INVALID;

    if ((mutex != NULL) && osal_in_interrupt())
    {
        /* FreeRTOS makes no mutex in an interrupt. */
        err = OSAL_ERR_MUTEX;
    }
    else if (mutex != NULL)
    {
        /* With no control block in the definition, every call allocates a
           mutex of its own, from the RTOS heap, so a second call makes a new
           one. FreeRTOS gives every mutex priority inheritance. */
        osMutexDef(osal_mutex);

        mutex->mutex = osRecursiveMutexCreate(osMutex(osal_mutex));
        err          = OSAL_ERR_NONE;

        /* NULL most often means the RTOS heap is too small. */
        if (mutex->mutex == NULL)
        {
            err = OSAL_ERR_MUTEX;
        }
    }
    else
    {
        /* A NULL mutex. */
    }

    return err;
}

/*****************************************************************************************************/
/**
 * @brief Take a mutex, waiting up to timeout_ms for another thread to give it back.
 *
 * The thread that holds it may take it again, and gives it back once for each
 * time it took it.
 *
 * @param[in,out] mutex       Mutex from osal_mutex_create().
 * @param[in]     timeout_ms  How long to wait. 0 tries once, and HAL_MAX_DELAY waits for ever.
 * @return OSAL_ERR_NONE, OSAL_ERR_INVALID, OSAL_ERR_TIMEOUT or OSAL_ERR_MUTEX.
 */
static inline osal_err_t osal_mutex_lock(osal_mutex_t *mutex, uint32_t timeout_ms)
{
    osal_err_t err    = OSAL_ERR_INVALID;
    osStatus   status = osOK;

    if ((mutex != NULL) && osal_in_interrupt())
    {
        /* ST's layer would try from an interrupt, where the other ports refuse. */
        err = OSAL_ERR_MUTEX;
    }
    else if (mutex != NULL)
    {
        /* CMSIS-RTOS v1 waits in milliseconds, and its osWaitForever is the
           same value as HAL_MAX_DELAY, so the timeout goes in as it is. */
        status = osRecursiveMutexWait(mutex->mutex, timeout_ms);

        if (status == osOK)
        {
            /* Taken. */
            err = OSAL_ERR_NONE;
        }
        else if (status == osErrorOS)
        {
            /* ST's layer says osErrorOS when another thread kept it for the
               whole wait, or for a wait of 0 held it at once. */
            err = OSAL_ERR_TIMEOUT;
        }
        else
        {
            /* A mutex never created. */
            err = OSAL_ERR_MUTEX;
        }
    }
    else
    {
        /* A NULL mutex. */
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
    if ((mutex != NULL) && !osal_in_interrupt())
    {
        (void)osRecursiveMutexRelease(mutex->mutex);
    }
}

/*****************************************************************************************************/
/**
 * @brief Wait ms milliseconds, letting other threads run.
 *
 * @param[in] ms  Milliseconds. 0 returns at once, and HAL_MAX_DELAY never returns.
 *                From an interrupt it returns at once, as on every port.
 */
static inline void osal_delay_ms(uint32_t ms)
{
    if (osal_in_interrupt())
    {
        /* No sleep in an interrupt. ST's osDelay() would try. */
    }
    else if (ms == HAL_MAX_DELAY)
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
        /* CMSIS-RTOS v1 takes milliseconds, and turns them into ticks itself. */
        (void)osDelay(ms);
    }
    else
    {
        /* 0 returns at once, since a zero sleep is not the same on every RTOS. */
    }
}

#ifdef __cplusplus
}
#endif

#endif /* OSAL_CMSIS_V1_H */

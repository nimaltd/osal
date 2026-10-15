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
 *              osal.h, never this file.
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
 * Private function prototypes
 * ****************************************************************************************************
*/

static inline uint32_t osal_v1_ms(uint32_t ms);

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
    /* 1 ms, which FreeRTOS's CMSIS-RTOS v1 layer makes one tick at least. */
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
        /* With no control block in the definition, every call allocates a
           mutex of its own, from the RTOS heap. FreeRTOS gives every mutex
           priority inheritance. */
        osMutexDef(osal_mutex);

        mutex->mutex = osMutexCreate(osMutex(osal_mutex));
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
        uint32_t wait   = osWaitForever;
        osStatus status = osErrorOS;

        /* HAL_MAX_DELAY waits for ever, and is the same value as osWaitForever.
           Anything else is rounded up to whole ticks. */
        if (timeout_ms != HAL_MAX_DELAY)
        {
            wait = osal_v1_ms(timeout_ms);
        }

        status = osMutexWait(mutex->mutex, wait);

        if (status == osOK)
        {
            /* Taken. */
            err = OSAL_ERR_NONE;
        }
        else if ((status == osErrorTimeoutResource) || (status == osErrorResource))
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
 * @brief Wait at least ms milliseconds, letting other threads run.
 *
 * @param[in] ms  Milliseconds. 0 returns at once.
 */
static inline void osal_delay_ms(uint32_t ms)
{
    /* 0 returns at once, since a zero sleep is not the same on every RTOS.
       Anything else sleeps the thread, so other threads run, for whole ticks
       and never less than asked. */
    if (ms > 0U)
    {
        (void)osDelay(osal_v1_ms(ms));
    }
}

/*
 * ****************************************************************************************************
 * Private function implementations
 * ****************************************************************************************************
*/

/*****************************************************************************************************/
/**
 * @brief Round milliseconds up to whole ticks, still in the milliseconds CMSIS-RTOS v1 takes.
 *
 * FreeRTOS's CMSIS-RTOS v1 layer turns milliseconds into ticks by dividing by
 * the tick period and dropping the rest. On a 10 ms tick, 15 ms would become
 * one tick, 10 ms, shorter than asked. Rounded up here first, it is 20 ms, two
 * ticks. On the usual 1 kHz tick nothing changes.
 *
 * @param[in] ms  Milliseconds.
 * @return Milliseconds that are a whole number of ticks, one short of
 *         osWaitForever at most.
 */
static inline uint32_t osal_v1_ms(uint32_t ms)
{
    uint32_t hz     = (uint32_t)osKernelSysTickFrequency;
    uint32_t period = 1000U / hz;
    uint64_t up     = ms;

    /* The same period the layer divides by. A tick of 1 ms or faster needs no
       rounding. */
    if (period > 1U)
    {
        up = (uint64_t)osal_ticks(ms, hz) * period;

        /* One short of "wait for ever", so a long finite wait stays finite. */
        if (up > 0xFFFFFFFEU)
        {
            up = 0xFFFFFFFEU;
        }
    }

    return (uint32_t)up;
}

#ifdef __cplusplus
}
#endif

#endif /* OSAL_CMSIS_V1_H */

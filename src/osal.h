/**
 * @file        osal.h
 * @brief       Mutex and delay for STM32, on bare metal, FreeRTOS or ThreadX.
 * @version     1.0.0
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
 * @note        The whole library is this header, so there is nothing to
 *              compile. Not for use from an interrupt, and with an RTOS,
 *              only from a thread once the RTOS has started.
 */

#ifndef OSAL_H
#define OSAL_H

/*
 * ****************************************************************************************************
 * Includes
 * ****************************************************************************************************
*/

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "main.h"
#include "osal_config.h"

/*
 * ****************************************************************************************************
 * Configuration checks
 * ****************************************************************************************************
*/

/* Checked here and never in osal_config.h. That file is the user's: it is copied
   once and never replaced, so a check in it can be edited away and would never
   reach anyone who installed before it was added. */

/* The values OSAL_RTOS can take. They start at 1 on purpose: a misspelt name
   counts as 0 inside #if, and 0 is then refused below instead of quietly
   meaning no RTOS. */
#define OSAL_RTOS_NONE      1
#define OSAL_RTOS_CMSIS_V1  2
#define OSAL_RTOS_CMSIS_V2  3
#define OSAL_RTOS_THREADX   4

#ifndef OSAL_RTOS
#error "OSAL_RTOS is not defined. Add it to osal_config.h"
#elif (OSAL_RTOS != OSAL_RTOS_NONE) && (OSAL_RTOS != OSAL_RTOS_CMSIS_V1) && \
      (OSAL_RTOS != OSAL_RTOS_CMSIS_V2) && (OSAL_RTOS != OSAL_RTOS_THREADX)
#error "OSAL_RTOS must be one of the OSAL_RTOS_ values listed in osal_config.h"
#endif

/* The mutex type comes from the RTOS, so its header is needed here. */
#if OSAL_RTOS == OSAL_RTOS_CMSIS_V1
#include "cmsis_os.h"
#elif OSAL_RTOS == OSAL_RTOS_CMSIS_V2
#include "cmsis_os2.h"
#elif OSAL_RTOS == OSAL_RTOS_THREADX
#include "tx_api.h"
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
 * @brief Error values returned by the mutex functions.
 */
typedef enum
{
    OSAL_ERR_NONE    = 0, /**< Done.                                              */
    OSAL_ERR_INVALID = 1, /**< A NULL mutex.                                      */
    OSAL_ERR_TIMEOUT = 2, /**< Another thread kept the mutex for the whole wait.  */
    OSAL_ERR_MUTEX   = 3, /**< The RTOS could not create or take the mutex.       */

} osal_err_t;

/*****************************************************************************************************/
/**
 * @brief A mutex. Put one wherever it is needed, a struct member included.
 */
typedef struct
{
#if OSAL_RTOS == OSAL_RTOS_CMSIS_V1
    osMutexId   mutex;  /**< The RTOS mutex.                                 */
#elif OSAL_RTOS == OSAL_RTOS_CMSIS_V2
    osMutexId_t mutex;  /**< The RTOS mutex.                                 */
#elif OSAL_RTOS == OSAL_RTOS_THREADX
    TX_MUTEX    mutex;  /**< The RTOS mutex, kept here so no heap is needed. */
#else
    uint8_t     unused; /**< Bare metal needs none, and C wants a member.    */
#endif

} osal_mutex_t;

/*
 * ****************************************************************************************************
 * Private function prototypes
 * ****************************************************************************************************
*/

#if (OSAL_RTOS == OSAL_RTOS_CMSIS_V2) || (OSAL_RTOS == OSAL_RTOS_THREADX)
static inline uint32_t osal_ticks(uint32_t ms, uint32_t tick_hz);
#endif

/*
 * ****************************************************************************************************
 * Public function implementations
 * ****************************************************************************************************
*/

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
        err = OSAL_ERR_NONE;

#if OSAL_RTOS == OSAL_RTOS_CMSIS_V1
        {
            /* With no control block in the definition, every call allocates
               a mutex of its own, from the RTOS heap. */
            osMutexDef(osal_mutex);

            mutex->mutex = osMutexCreate(osMutex(osal_mutex));
        }

        /* NULL most often means the RTOS heap is too small. */
        if (mutex->mutex == NULL)
        {
            err = OSAL_ERR_MUTEX;
        }
#elif OSAL_RTOS == OSAL_RTOS_CMSIS_V2
        {
            /* Priority inheritance, so a low priority thread holding the mutex
               is not held up by a medium one while a high priority thread
               waits for it. */
            const osMutexAttr_t attr = { "osal", osMutexPrioInherit, NULL, 0U };

            mutex->mutex = osMutexNew(&attr);
        }

        /* NULL most often means the RTOS heap is too small. */
        if (mutex->mutex == NULL)
        {
            err = OSAL_ERR_MUTEX;
        }
#elif OSAL_RTOS == OSAL_RTOS_THREADX
        /* Inside osal_mutex_t, so no heap is needed. Priority inheritance, for
           the same reason as above. */
        if (tx_mutex_create(&mutex->mutex, (CHAR *)"osal", TX_INHERIT) != TX_SUCCESS)
        {
            err = OSAL_ERR_MUTEX;
        }
#else
        /* Bare metal has no other thread to keep out. */
        mutex->unused = 0U;
#endif
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
        err = OSAL_ERR_NONE;

#if OSAL_RTOS == OSAL_RTOS_CMSIS_V1
        {
            /* CMSIS-RTOS v1 waits in milliseconds, and its osWaitForever is
               the same value as HAL_MAX_DELAY, so the timeout goes in as it is. */
            osStatus status = osMutexWait(mutex->mutex, timeout_ms);

            if ((status == osErrorTimeoutResource) || (status == osErrorResource))
            {
                /* Another thread kept it for the whole wait. */
                err = OSAL_ERR_TIMEOUT;
            }
            else if (status != osOK)
            {
                /* Refused outright, such as from an interrupt. */
                err = OSAL_ERR_MUTEX;
            }
            else
            {
                /* Taken. */
            }
        }
#elif OSAL_RTOS == OSAL_RTOS_CMSIS_V2
        {
            uint32_t   ticks  = osWaitForever;
            osStatus_t status = osError;

            /* HAL_MAX_DELAY waits for ever. Anything else becomes ticks. */
            if (timeout_ms != HAL_MAX_DELAY)
            {
                ticks = osal_ticks(timeout_ms, osKernelGetTickFreq());
            }

            status = osMutexAcquire(mutex->mutex, ticks);

            if ((status == osErrorTimeout) || (status == osErrorResource))
            {
                /* Another thread kept it for the whole wait. */
                err = OSAL_ERR_TIMEOUT;
            }
            else if (status != osOK)
            {
                /* Refused outright, such as from an interrupt. */
                err = OSAL_ERR_MUTEX;
            }
            else
            {
                /* Taken. */
            }
        }
#elif OSAL_RTOS == OSAL_RTOS_THREADX
        {
            ULONG ticks  = TX_WAIT_FOREVER;
            UINT  status = TX_SUCCESS;

            /* HAL_MAX_DELAY waits for ever. Anything else becomes ticks. */
            if (timeout_ms != HAL_MAX_DELAY)
            {
                ticks = osal_ticks(timeout_ms, (uint32_t)TX_TIMER_TICKS_PER_SECOND);
            }

            status = tx_mutex_get(&mutex->mutex, ticks);

            if (status == TX_NOT_AVAILABLE)
            {
                /* Another thread kept it for the whole wait. */
                err = OSAL_ERR_TIMEOUT;
            }
            else if (status != TX_SUCCESS)
            {
                /* Refused outright, such as from an interrupt, or from
                   outside a thread. */
                err = OSAL_ERR_MUTEX;
            }
            else
            {
                /* Taken. */
            }
        }
#else
        /* Bare metal has no other thread to keep out. */
        (void)timeout_ms;
#endif
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
#if (OSAL_RTOS == OSAL_RTOS_CMSIS_V1) || (OSAL_RTOS == OSAL_RTOS_CMSIS_V2)
        (void)osMutexRelease(mutex->mutex);
#elif OSAL_RTOS == OSAL_RTOS_THREADX
        (void)tx_mutex_put(&mutex->mutex);
#endif
    }
}

/*****************************************************************************************************/
/**
 * @brief Wait at least ms milliseconds, letting other threads run when there is an RTOS.
 *
 * @param[in] ms  Milliseconds. 0 returns at once.
 */
static inline void osal_delay_ms(uint32_t ms)
{
    /* 0 returns at once. Each RTOS treats a zero sleep its own way, and
       HAL_Delay(0) waits a tick, so none of them is asked. */
    if (ms > 0U)
    {
        /* With an RTOS the thread sleeps, so other threads run, for at least
           one tick even on a tick slower than 1 ms. */
#if OSAL_RTOS == OSAL_RTOS_CMSIS_V1
        /* CMSIS-RTOS v1 takes milliseconds. */
        (void)osDelay(ms);
#elif OSAL_RTOS == OSAL_RTOS_CMSIS_V2
        (void)osDelay(osal_ticks(ms, osKernelGetTickFreq()));
#elif OSAL_RTOS == OSAL_RTOS_THREADX
        (void)tx_thread_sleep(osal_ticks(ms, (uint32_t)TX_TIMER_TICKS_PER_SECOND));
#else
        HAL_Delay(ms);
#endif
    }
}

/*
 * ****************************************************************************************************
 * Private function implementations
 * ****************************************************************************************************
*/

#if (OSAL_RTOS == OSAL_RTOS_CMSIS_V2) || (OSAL_RTOS == OSAL_RTOS_THREADX)
/*****************************************************************************************************/
/**
 * @brief Turn milliseconds into RTOS ticks, rounding up.
 *
 * @param[in] ms       Milliseconds.
 * @param[in] tick_hz  The RTOS tick rate.
 * @return The number of ticks.
 */
static inline uint32_t osal_ticks(uint32_t ms, uint32_t tick_hz)
{
    /* Rounded up, so a short wait does not become no wait at all on a slow
       tick. 64 bits, so a long wait cannot overflow on the way. */
    uint64_t ticks = (((uint64_t)ms * tick_hz) + 999U) / 1000U;

    /* One short of the RTOS's "wait for ever", so a long finite timeout
       cannot turn into an endless one. */
    if (ticks > 0xFFFFFFFEU)
    {
        ticks = 0xFFFFFFFEU;
    }

    return (uint32_t)ticks;
}
#endif

#ifdef __cplusplus
}
#endif

#endif /* OSAL_H */

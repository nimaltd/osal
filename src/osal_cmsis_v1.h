/**
 * @file        osal_cmsis_v1.h
 * @brief       The osal port for FreeRTOS through CMSIS-RTOS v1.
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
        /* CMSIS-RTOS v1 waits in milliseconds, and its osWaitForever is the
           same value as HAL_MAX_DELAY, so the timeout goes in as it is. */
        osStatus status = osMutexWait(mutex->mutex, timeout_ms);

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
       Anything else sleeps the thread, so other threads run. CMSIS-RTOS v1
       takes milliseconds. */
    if (ms > 0U)
    {
        (void)osDelay(ms);
    }
}

#ifdef __cplusplus
}
#endif

#endif /* OSAL_CMSIS_V1_H */

/**
 * @file        osal_none.h
 * @brief       The osal port for bare metal, with no RTOS.
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
 * @note        Included by osal.h when OSAL_RTOS is OSAL_RTOS_NONE. Include
 *              osal.h, never this file.
 */

#ifndef OSAL_NONE_H
#define OSAL_NONE_H

/*
 * ****************************************************************************************************
 * Configuration checks
 * ****************************************************************************************************
*/

#if !defined(OSAL_H) || (OSAL_RTOS != OSAL_RTOS_NONE)
#error "osal_none.h is included by osal.h when OSAL_RTOS is OSAL_RTOS_NONE. Include osal.h"
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
    uint8_t unused; /**< Bare metal needs none, and C wants a member. */

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
 * @return OSAL_ERR_NONE or OSAL_ERR_INVALID.
 */
static inline osal_err_t osal_mutex_create(osal_mutex_t *mutex)
{
    osal_err_t err = OSAL_ERR_INVALID;

    if (mutex != NULL)
    {
        /* Bare metal has no other thread to keep out. */
        mutex->unused = 0U;
        err           = OSAL_ERR_NONE;
    }

    return err;
}

/*****************************************************************************************************/
/**
 * @brief Take a mutex, waiting up to timeout_ms for another thread to give it back.
 *
 * @param[in,out] mutex       Mutex from osal_mutex_create().
 * @param[in]     timeout_ms  How long to wait. HAL_MAX_DELAY waits for ever.
 * @return OSAL_ERR_NONE or OSAL_ERR_INVALID.
 */
static inline osal_err_t osal_mutex_lock(osal_mutex_t *mutex, uint32_t timeout_ms)
{
    osal_err_t err = OSAL_ERR_INVALID;

    /* Bare metal has no other thread to keep out, so there is never a wait. */
    (void)timeout_ms;

    if (mutex != NULL)
    {
        err = OSAL_ERR_NONE;
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
    /* Nothing was taken, so there is nothing to give back. */
    (void)mutex;
}

/*****************************************************************************************************/
/**
 * @brief Wait at least ms milliseconds.
 *
 * @param[in] ms  Milliseconds. 0 returns at once.
 */
static inline void osal_delay_ms(uint32_t ms)
{
    /* 0 returns at once, since HAL_Delay(0) still waits a tick. */
    if (ms > 0U)
    {
        HAL_Delay(ms);
    }
}

#ifdef __cplusplus
}
#endif

#endif /* OSAL_NONE_H */

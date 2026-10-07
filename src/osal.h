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
 * @note        The only osal file to include. It reads OSAL_RTOS from
 *              osal_config.h and includes the one port header for that RTOS.
 *              Not for use from an interrupt, and with an RTOS, only from a
 *              thread once the RTOS has started.
 */

#ifndef OSAL_H
#define OSAL_H

/*
 * ****************************************************************************************************
 * Includes
 * ****************************************************************************************************
*/

#include <stddef.h>
#include <stdint.h>

#include "main.h"
#include "osal_config.h"

/*
 * ****************************************************************************************************
 * Configuration checks
 * ****************************************************************************************************
*/

/* Checked here and never in osal_config.h. The part of that file between its
   USER CODE markers is the user's and survives every update, so a check in it
   could be edited away, and one added later would never reach it. */

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

/*
 * ****************************************************************************************************
 * Private function implementations
 * ****************************************************************************************************
*/

/*****************************************************************************************************/
/**
 * @brief Turn milliseconds into RTOS ticks, rounding up. Shared by the ports that count in ticks.
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

#ifdef __cplusplus
}
#endif

/*
 * ****************************************************************************************************
 * RTOS port
 * ****************************************************************************************************
*/

/* The one port header for the chosen RTOS. It brings osal_mutex_t and the
   functions, and the other ports are never compiled. It comes last because it
   uses osal_err_t and osal_ticks() from above. */
#if OSAL_RTOS == OSAL_RTOS_NONE
#include "osal_none.h"
#elif OSAL_RTOS == OSAL_RTOS_CMSIS_V1
#include "osal_cmsis_v1.h"
#elif OSAL_RTOS == OSAL_RTOS_CMSIS_V2
#include "osal_cmsis_v2.h"
#elif OSAL_RTOS == OSAL_RTOS_THREADX
#include "osal_threadx.h"
#endif

#endif /* OSAL_H */

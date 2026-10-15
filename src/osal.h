/**
 * @file        osal.h
 * @brief       OS abstraction layer for STM32: the same yield, pause, mutex and delay on
 *              bare metal, FreeRTOS and ThreadX.
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
 * @note        The only osal file to include. It reads OSAL_RTOS from
 *              osal_config.h and includes the one port header for that RTOS.
 *              Every port behaves the same. With an RTOS, call it from a
 *              thread once the RTOS has started. Never call it from a
 *              critical section: a lock or a delay there is not refused, and
 *              cannot work with interrupts masked. From an interrupt a mutex
 *              is refused and nothing waits.
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
 * @brief Whether the CPU is in an interrupt handler, or any other exception. Shared by every port.
 *
 * Every osal call asks it first, so from an interrupt each behaves the same on
 * every port: a mutex is refused, and nothing waits. The RTOSes differ there:
 * FreeRTOS's CMSIS-RTOS v1 layer would take a mutex from an interrupt, and
 * sleep in one, which fails.
 *
 * @return true in an exception, false in thread mode.
 */
static inline bool osal_in_interrupt(void)
{
    /* IPSR holds the number of the exception running, and 0 in thread mode.
       __get_IPSR() comes with the CMSIS core header main.h brings in. */
    return (__get_IPSR() != 0U);
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
   uses osal_err_t and osal_in_interrupt() from above. */
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

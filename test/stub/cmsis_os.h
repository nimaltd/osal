/**
 * @file        cmsis_os.h
 * @brief       Host stub for the CMSIS-RTOS v1 API, as CubeMX generates it for FreeRTOS.
 * @version     1.0.0
 *
 * @author      Nima Askari (NimaLTD)
 * @email       nima.askari@gmail.com
 * @github      https://www.github.com/nimaltd
 *
 * @copyright   (c) 2026 Nima Askari (NimaLTD)
 *              SPDX-License-Identifier: Apache-2.0
 *              See LICENSE.md in the project root for the full license text.
 *
 * @note        Only what osal uses, with the real names and values, and the
 *              recursive mutex calls ST adds to the standard ones. The
 *              functions are implemented by the tests, over a fake kernel.
 */

#ifndef CMSIS_OS_H
#define CMSIS_OS_H

/*
 * ****************************************************************************************************
 * Includes
 * ****************************************************************************************************
*/

#include <stdint.h>

#include "FreeRTOS.h"

/*
 * ****************************************************************************************************
 * Macros
 * ****************************************************************************************************
*/

#define osWaitForever           0xFFFFFFFFU

#define osMutexDef(name)        const osMutexDef_t os_mutex_def_##name = { 0U }
#define osMutex(name)           &os_mutex_def_##name

/*
 * ****************************************************************************************************
 * Types
 * ****************************************************************************************************
*/

/*****************************************************************************************************/
/**
 * @brief Status codes, with the values CMSIS-RTOS v1 gives them.
 */
typedef enum
{
    osOK                   = 0x00,
    osErrorParameter       = 0x80,
    osErrorResource        = 0x81,
    osErrorISR             = 0x82,
    osErrorTimeoutResource = 0xC1,
    osErrorOS              = 0xFF,

} osStatus;

/*****************************************************************************************************/
/**
 * @brief Mutex definition, filled in by osMutexDef().
 */
typedef struct
{
    uint32_t dummy;

} osMutexDef_t;

/*****************************************************************************************************/
/**
 * @brief Mutex handle. A pointer, as the FreeRTOS port makes it.
 */
typedef struct test_os_mutex *osMutexId;

/*
 * ****************************************************************************************************
 * Public function prototypes
 * ****************************************************************************************************
*/

/*****************************************************************************************************/
/**
 * @brief Create a recursive mutex. ST's own call, beside the standard ones.
 */
osMutexId osRecursiveMutexCreate(const osMutexDef_t *mutex_def);

/*****************************************************************************************************/
/**
 * @brief Take a recursive mutex, waiting up to millisec. osErrorOS when it was not had.
 */
osStatus osRecursiveMutexWait(osMutexId mutex_id, uint32_t millisec);

/*****************************************************************************************************/
/**
 * @brief Give a recursive mutex back.
 */
osStatus osRecursiveMutexRelease(osMutexId mutex_id);

/*****************************************************************************************************/
/**
 * @brief Let the threads of the same priority run.
 */
osStatus osThreadYield(void);

/*****************************************************************************************************/
/**
 * @brief Sleep for millisec.
 */
osStatus osDelay(uint32_t millisec);

#endif /* CMSIS_OS_H */

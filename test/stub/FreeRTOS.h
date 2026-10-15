/**
 * @file        FreeRTOS.h
 * @brief       Host stub for the part of FreeRTOS.h osal uses through CMSIS-RTOS v2.
 * @version     1.1.0
 *
 * @author      Nima Askari (NimaLTD)
 * @email       nima.askari@gmail.com
 * @github      https://www.github.com/nimaltd
 *
 * @copyright   (c) 2026 Nima Askari (NimaLTD)
 *              SPDX-License-Identifier: Apache-2.0
 *              See LICENSE.md in the project root for the full license text.
 *
 * @note        Only what osal uses, with the real names. pdMS_TO_TICKS() is
 *              written as FreeRTOS's projdefs.h writes it.
 */

#ifndef INC_FREERTOS_H
#define INC_FREERTOS_H

/*
 * ****************************************************************************************************
 * Includes
 * ****************************************************************************************************
*/

#include <stdint.h>

/*
 * ****************************************************************************************************
 * Macros
 * ****************************************************************************************************
*/

/* The tick rate, a function here so the tests can change it. In a project it
   is FreeRTOSConfig.h's configTICK_RATE_HZ. */
#define configTICK_RATE_HZ          test_tick_rate()

/* Milliseconds to ticks, the rest of a tick dropped. A project may define its
   own in FreeRTOSConfig.h, so FreeRTOS only defines it when there is none. */
#ifndef pdMS_TO_TICKS
#define pdMS_TO_TICKS(xTimeInMs)    ((TickType_t)(((uint64_t)(xTimeInMs) * (uint64_t)configTICK_RATE_HZ) / (uint64_t)1000U))
#endif

/*
 * ****************************************************************************************************
 * Types
 * ****************************************************************************************************
*/

/*****************************************************************************************************/
/**
 * @brief A count of ticks, 32 bits on a Cortex-M.
 */
typedef uint32_t TickType_t;

/*
 * ****************************************************************************************************
 * Public function prototypes
 * ****************************************************************************************************
*/

/*****************************************************************************************************/
/**
 * @brief The tick rate the tests have set.
 */
uint32_t test_tick_rate(void);

#endif /* INC_FREERTOS_H */

/**
 * @file        FreeRTOS.h
 * @brief       Host stub for the part of FreeRTOS.h osal uses through CMSIS-RTOS.
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

#include <stddef.h>
#include <stdint.h>

/*
 * ****************************************************************************************************
 * Macros
 * ****************************************************************************************************
*/

/* The tick rate, a function here so the tests can change it. In a project it
   is FreeRTOSConfig.h's configTICK_RATE_HZ. */
#define configTICK_RATE_HZ          test_tick_rate()

/* CubeMX's default, which osal needs. */
#define configUSE_RECURSIVE_MUTEXES 1

/* What CubeMX gives CMSIS-RTOS v2, which needs 56. A build may give another,
   as CMSIS-RTOS v1 projects often have 7. */
#ifndef configMAX_PRIORITIES
#define configMAX_PRIORITIES        56
#endif

/* The idle task's stack in words, CubeMX's default. */
#define configMINIMAL_STACK_SIZE    ((uint16_t)128)

/* Both on, as CubeMX makes them for CMSIS-RTOS v2. A build may switch static
   allocation off, as CMSIS-RTOS v1 projects usually have it, which takes the
   static members out of ST's osThreadDef_t. FreeRTOS.h makes both 1 when
   FreeRTOSConfig.h leaves them out, the dynamic one at least. */
#ifndef configSUPPORT_STATIC_ALLOCATION
#define configSUPPORT_STATIC_ALLOCATION 1
#endif

#ifndef configSUPPORT_DYNAMIC_ALLOCATION
#define configSUPPORT_DYNAMIC_ALLOCATION 1
#endif

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

/*****************************************************************************************************/
/**
 * @brief FreeRTOS's base types, as its Cortex-M ports define them.
 */
typedef uint32_t      StackType_t;
typedef long          BaseType_t;
typedef unsigned long UBaseType_t;

/*****************************************************************************************************/
/**
 * @brief A task's control block for static allocation. Only its size matters to anyone outside.
 */
typedef struct
{
    void *dummy[4];

} StaticTask_t;

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

/*****************************************************************************************************/
/**
 * @brief Take memory from the fake FreeRTOS heap.
 */
void *pvPortMalloc(size_t xWantedSize);

/*****************************************************************************************************/
/**
 * @brief Give memory back to the fake FreeRTOS heap.
 */
void vPortFree(void *pv);

#endif /* INC_FREERTOS_H */

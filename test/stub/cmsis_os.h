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
 *              recursive mutex and thread calls ST adds to the standard ones.
 *              The functions are implemented by the tests, over a fake kernel.
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
#include "task.h"

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
 * @brief Thread priorities, with the values CMSIS-RTOS v1 gives them.
 */
typedef enum
{
    osPriorityIdle         = -3,
    osPriorityLow          = -2,
    osPriorityBelowNormal  = -1,
    osPriorityNormal       = 0,
    osPriorityAboveNormal  = +1,
    osPriorityHigh         = +2,
    osPriorityRealtime     = +3,
    osPriorityError        = 0x84,

} osPriority;

/*****************************************************************************************************/
/**
 * @brief A thread's function, which takes a const pointer in CMSIS-RTOS v1.
 */
typedef void (*os_pthread)(void const *argument);

/*****************************************************************************************************/
/**
 * @brief Thread handle. A pointer, as the FreeRTOS port makes it.
 */
typedef struct test_os_thread *osThreadId;

/*****************************************************************************************************/
/**
 * @brief Control block for a thread made with static allocation.
 */
typedef StaticTask_t osStaticThreadDef_t;

/*****************************************************************************************************/
/**
 * @brief Thread definition, as ST lays it out, static members only with static allocation.
 */
typedef struct
{
    char                *name;         /**< Thread name.                            */
    os_pthread          pthread;       /**< Its function.                           */
    osPriority          tpriority;     /**< Its priority.                           */
    uint32_t            instances;     /**< Unused by ST's layer.                   */
    uint32_t            stacksize;     /**< Handed to FreeRTOS as it is, in words.  */
#if (configSUPPORT_STATIC_ALLOCATION == 1)
    uint32_t            *buffer;       /**< Stack for static allocation, or NULL.   */
    osStaticThreadDef_t *controlblock; /**< Block for static allocation, or NULL.   */
#endif

} osThreadDef_t;

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

/*****************************************************************************************************/
/**
 * @brief Whether the kernel runs: 0 before the start, 1 after it, -1 when FreeRTOS cannot tell.
 */
int32_t osKernelRunning(void);

/*****************************************************************************************************/
/**
 * @brief Create a thread and start it. NULL when it could not be made.
 */
osThreadId osThreadCreate(const osThreadDef_t *thread_def, void *argument);

/*****************************************************************************************************/
/**
 * @brief The thread running.
 */
osThreadId osThreadGetId(void);

/*****************************************************************************************************/
/**
 * @brief Delete a thread.
 */
osStatus osThreadTerminate(osThreadId thread_id);

/*****************************************************************************************************/
/**
 * @brief Suspend a thread. ST's own call, beside the standard ones.
 */
osStatus osThreadSuspend(osThreadId thread_id);

/*****************************************************************************************************/
/**
 * @brief Resume a suspended thread. ST's own call, beside the standard ones.
 */
osStatus osThreadResume(osThreadId thread_id);

/*****************************************************************************************************/
/**
 * @brief Hold the scheduler, so no other thread runs. ST's own call, beside the standard ones.
 */
osStatus osThreadSuspendAll(void);

/*****************************************************************************************************/
/**
 * @brief Let the scheduler go again. ST's own call, beside the standard ones.
 */
osStatus osThreadResumeAll(void);

#endif /* CMSIS_OS_H */

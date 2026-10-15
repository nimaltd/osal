/**
 * @file        cmsis_os2.h
 * @brief       Host stub for the CMSIS-RTOS v2 API.
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
 * @note        Only what osal uses, with the real names and values. The
 *              functions are implemented by the tests, over a fake kernel.
 */

#ifndef CMSIS_OS2_H
#define CMSIS_OS2_H

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

#define osWaitForever           0xFFFFFFFFU
#define osMutexRecursive        0x00000001U
#define osMutexPrioInherit      0x00000002U

/*
 * ****************************************************************************************************
 * Types
 * ****************************************************************************************************
*/

/*****************************************************************************************************/
/**
 * @brief Status codes, with the values CMSIS-RTOS v2 gives them.
 */
typedef enum
{
    osOK             = 0,
    osError          = -1,
    osErrorTimeout   = -2,
    osErrorResource  = -3,
    osErrorParameter = -4,
    osErrorNoMemory  = -5,
    osErrorISR       = -6,

} osStatus_t;

/*****************************************************************************************************/
/**
 * @brief Kernel states, with the values CMSIS-RTOS v2 gives them.
 */
typedef enum
{
    osKernelInactive  = 0,
    osKernelReady     = 1,
    osKernelRunning   = 2,
    osKernelLocked    = 3,
    osKernelSuspended = 4,
    osKernelError     = -1,

} osKernelState_t;

/*****************************************************************************************************/
/**
 * @brief Thread priorities, with the values CMSIS-RTOS v2 gives them. The ones osal uses.
 */
typedef enum
{
    osPriorityNone      = 0,
    osPriorityIdle      = 1,
    osPriorityLow       = 8,
    osPriorityNormal    = 24,
    osPriorityRealtime7 = 55,
    osPriorityISR       = 56,
    osPriorityError     = -1,

} osPriority_t;

/*****************************************************************************************************/
/**
 * @brief Thread handle.
 */
typedef void *osThreadId_t;

/*****************************************************************************************************/
/**
 * @brief A thread's function.
 */
typedef void (*osThreadFunc_t)(void *argument);

/*****************************************************************************************************/
/**
 * @brief Thread attributes.
 */
typedef struct
{
    const char   *name;      /**< Name, for a debugger.               */
    uint32_t     attr_bits;  /**< osThreadJoinable and so on.         */
    void         *cb_mem;    /**< Static control block, or NULL.      */
    uint32_t     cb_size;    /**< Size of cb_mem.                     */
    void         *stack_mem; /**< Static stack, or NULL.              */
    uint32_t     stack_size; /**< Stack in bytes, 0 for the default.  */
    osPriority_t priority;   /**< Its priority.                       */
    uint32_t     tz_module;  /**< TrustZone module, unused here.      */
    uint32_t     reserved;   /**< Must be 0.                          */

} osThreadAttr_t;

/*****************************************************************************************************/
/**
 * @brief Mutex handle.
 */
typedef void *osMutexId_t;

/*****************************************************************************************************/
/**
 * @brief Mutex attributes.
 */
typedef struct
{
    const char *name;      /**< Name, for a debugger.          */
    uint32_t   attr_bits;  /**< osMutexPrioInherit and so on.  */
    void       *cb_mem;    /**< Static control block, or NULL. */
    uint32_t   cb_size;    /**< Size of cb_mem.                */

} osMutexAttr_t;

/*
 * ****************************************************************************************************
 * Public function prototypes
 * ****************************************************************************************************
*/

/*****************************************************************************************************/
/**
 * @brief Create a mutex.
 */
osMutexId_t osMutexNew(const osMutexAttr_t *attr);

/*****************************************************************************************************/
/**
 * @brief Take a mutex, waiting up to timeout ticks.
 */
osStatus_t osMutexAcquire(osMutexId_t mutex_id, uint32_t timeout);

/*****************************************************************************************************/
/**
 * @brief Give a mutex back.
 */
osStatus_t osMutexRelease(osMutexId_t mutex_id);

/*****************************************************************************************************/
/**
 * @brief Let the threads of the same priority run.
 */
osStatus_t osThreadYield(void);

/*****************************************************************************************************/
/**
 * @brief Sleep for ticks.
 */
osStatus_t osDelay(uint32_t ticks);

/*****************************************************************************************************/
/**
 * @brief The kernel's state.
 */
osKernelState_t osKernelGetState(void);

/*****************************************************************************************************/
/**
 * @brief Hold the scheduler: 0 when this took the lock, 1 when it was held already, below 0 before the start.
 */
int32_t osKernelLock(void);

/*****************************************************************************************************/
/**
 * @brief Let the scheduler go again.
 */
int32_t osKernelUnlock(void);

/*****************************************************************************************************/
/**
 * @brief Create a thread and start it. NULL when it could not be made.
 */
osThreadId_t osThreadNew(osThreadFunc_t func, void *argument, const osThreadAttr_t *attr);

/*****************************************************************************************************/
/**
 * @brief The thread running.
 */
osThreadId_t osThreadGetId(void);

/*****************************************************************************************************/
/**
 * @brief Delete a thread.
 */
osStatus_t osThreadTerminate(osThreadId_t thread_id);

/*****************************************************************************************************/
/**
 * @brief Suspend a thread.
 */
osStatus_t osThreadSuspend(osThreadId_t thread_id);

/*****************************************************************************************************/
/**
 * @brief Resume a suspended thread.
 */
osStatus_t osThreadResume(osThreadId_t thread_id);

#endif /* CMSIS_OS2_H */

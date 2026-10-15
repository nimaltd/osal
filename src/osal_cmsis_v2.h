/**
 * @file        osal_cmsis_v2.h
 * @brief       The osal port for FreeRTOS through CMSIS-RTOS v2.
 * @version     1.2.0
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
 * @note        Included by osal.h when OSAL_RTOS is OSAL_RTOS_CMSIS_V2. Include
 *              osal.h, never this file. CMSIS-RTOS v2 takes ticks, and
 *              milliseconds become ticks through FreeRTOS's own
 *              pdMS_TO_TICKS(), which a project can define its own way in
 *              FreeRTOSConfig.h. The mutex is FreeRTOS's recursive one, as a
 *              ThreadX mutex is. A task and the heap come from the FreeRTOS
 *              heap, and a critical section is FreeRTOS's own.
 */

#ifndef OSAL_CMSIS_V2_H
#define OSAL_CMSIS_V2_H

/*
 * ****************************************************************************************************
 * Includes
 * ****************************************************************************************************
*/

#include <string.h>

#include "FreeRTOS.h"
#include "cmsis_os2.h"
#include "task.h"

/*
 * ****************************************************************************************************
 * Configuration checks
 * ****************************************************************************************************
*/

#if !defined(OSAL_H) || (OSAL_RTOS != OSAL_RTOS_CMSIS_V2)
#error "osal_cmsis_v2.h is included by osal.h when OSAL_RTOS is OSAL_RTOS_CMSIS_V2. Include osal.h"
#endif

/* Without recursive mutexes, the CMSIS-RTOS v2 layer makes no recursive mutex
   at all, and every osal_mutex_create() would fail. */
#if !defined(configUSE_RECURSIVE_MUTEXES) || (configUSE_RECURSIVE_MUTEXES != 1)
#error "osal needs configUSE_RECURSIVE_MUTEXES 1: in CubeMX, FreeRTOS, Config parameters, USE_RECURSIVE_MUTEXES Enabled"
#endif

/* osal takes every mutex and task from the FreeRTOS heap. Without it, the
   CMSIS-RTOS v2 layer makes neither, and osal_mutex_create() and
   osal_task_create() would fail. FreeRTOS.h makes it 1 when FreeRTOSConfig.h
   leaves it out. */
#if (configSUPPORT_DYNAMIC_ALLOCATION != 1)
#error "osal needs configSUPPORT_DYNAMIC_ALLOCATION 1: in CubeMX, FreeRTOS, Config parameters, Memory Allocation Dynamic"
#endif

#ifdef __cplusplus
extern "C"
{
#endif

/*
 * ****************************************************************************************************
 * Macros
 * ****************************************************************************************************
*/

/* The highest task priority. CMSIS-RTOS v2 numbers a task's priority from
   osPriorityIdle to osPriorityRealtime7, and ST's layer hands that number to
   FreeRTOS as it is. osal's 0 is osPriorityIdle, so its highest is
   osPriorityRealtime7, FreeRTOS's highest. osPriorityISR above it is the
   RTOS's own. */
#define OSAL_TASK_PRIORITY_MAX  ((uint32_t)osPriorityRealtime7 - (uint32_t)osPriorityIdle)

/* The smallest stack in bytes: FreeRTOS's own, its idle task's. */
#define OSAL_TASK_STACK_MIN     ((uint32_t)configMINIMAL_STACK_SIZE * (uint32_t)sizeof(StackType_t))

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
    osMutexId_t mutex; /**< The RTOS mutex. */

} osal_mutex_t;

/*****************************************************************************************************/
/**
 * @brief A task. Make it static or global: it lasts as long as the task does.
 */
typedef struct
{
    osThreadId_t     thread;    /**< The RTOS task, NULL while there is none. */
    osal_task_func_t function;  /**< What the task runs.                      */
    void             *argument; /**< Given to function.                       */

} osal_task_t;

/*
 * ****************************************************************************************************
 * Private function prototypes
 * ****************************************************************************************************
*/

static inline uint32_t osal_ticks(uint32_t ms);
static inline bool     osal_in_task(void);
static inline void     osal_task_run(void *argument);

/*
 * ****************************************************************************************************
 * Public function implementations
 * ****************************************************************************************************
*/

/*****************************************************************************************************/
/**
 * @brief Let the other threads of the same priority run, and come back at once if none is waiting.
 *
 * For a loop that keeps the CPU busy itself, such as a read by polling in
 * pieces: between two pieces the threads of the same priority get a turn, and
 * when none is waiting it costs a few microseconds. A thread of lower priority
 * never runs, so a loop that waits on hardware calls osal_pause() instead.
 */
static inline void osal_yield(void)
{
    if (!osal_in_interrupt())
    {
        (void)osThreadYield();
    }
}

/*****************************************************************************************************/
/**
 * @brief Pause a loop that waits on hardware, so every other thread runs meanwhile.
 *
 * Sleeps one tick, 1 ms at the usual 1 kHz, the shortest sleep the RTOS has,
 * so threads of every priority run. Call it between two questions to a chip or
 * a DMA that is still busy. A yield would let no thread of lower priority run
 * until the wait ends, which for an erase can be minutes.
 */
static inline void osal_pause(void)
{
    if (!osal_in_interrupt())
    {
        (void)osDelay(1U);
    }
}

/*****************************************************************************************************/
/**
 * @brief Mask the interrupts, so nothing else runs until osal_critical_exit().
 *
 * Works in a task, in an interrupt and before the RTOS starts, and nests: each
 * osal_critical_exit() puts back what its osal_critical_enter() found. FreeRTOS
 * masks the interrupts up to configMAX_SYSCALL_INTERRUPT_PRIORITY, as it always
 * does. Those above it never call the RTOS, and never call osal either.
 *
 * @return What to give osal_critical_exit().
 */
static inline uint32_t osal_critical_enter(void)
{
    uint32_t state = 0U;

    if (osal_in_task())
    {
        /* FreeRTOS's own. It counts how deep it is, so an RTOS call made
           inside, with a critical section of its own, leaves the interrupts
           masked on its way out. */
        taskENTER_CRITICAL();
    }
    else
    {
        /* An interrupt, or before the RTOS starts: the mask is kept and put
           back. Before the start FreeRTOS's count would leave the interrupts
           masked until the RTOS starts, where every other port gives them
           back. */
        state = (uint32_t)taskENTER_CRITICAL_FROM_ISR();
    }

    return state;
}

/*****************************************************************************************************/
/**
 * @brief Put the interrupts back as osal_critical_enter() found them.
 *
 * @param[in] state  What osal_critical_enter() returned.
 */
static inline void osal_critical_exit(uint32_t state)
{
    if (osal_in_task())
    {
        taskEXIT_CRITICAL();
    }
    else
    {
        taskEXIT_CRITICAL_FROM_ISR((UBaseType_t)state);
    }
}

/*****************************************************************************************************/
/**
 * @brief Create a mutex. A second call on the same one gives it back as new.
 *
 * @param[out] mutex  Mutex to create.
 * @return OSAL_ERR_NONE, OSAL_ERR_INVALID or OSAL_ERR_MUTEX.
 */
static inline osal_err_t osal_mutex_create(osal_mutex_t *mutex)
{
    osal_err_t err = OSAL_ERR_INVALID;

    if ((mutex != NULL) && osal_in_interrupt())
    {
        /* FreeRTOS makes no mutex in an interrupt. */
        err = OSAL_ERR_MUTEX;
    }
    else if (mutex != NULL)
    {
        /* Recursive, so the thread that holds it may take it again. Priority
           inheritance, so a low priority thread holding the mutex is not held
           up by a medium one while a high priority thread waits for it. From
           the RTOS heap, so a second call makes a new one. */
        const osMutexAttr_t attr = { "osal", osMutexRecursive | osMutexPrioInherit, NULL, 0U };

        mutex->mutex = osMutexNew(&attr);
        err          = OSAL_ERR_NONE;

        /* NULL most often means the RTOS heap is too small. */
        if (mutex->mutex == NULL)
        {
            err = OSAL_ERR_MUTEX;
        }
    }
    else
    {
        /* A NULL mutex. */
    }

    return err;
}

/*****************************************************************************************************/
/**
 * @brief Take a mutex, waiting up to timeout_ms for another thread to give it back.
 *
 * The thread that holds it may take it again, and gives it back once for each
 * time it took it.
 *
 * @param[in,out] mutex       Mutex from osal_mutex_create().
 * @param[in]     timeout_ms  How long to wait. 0 tries once, and HAL_MAX_DELAY waits for ever.
 * @return OSAL_ERR_NONE, OSAL_ERR_INVALID, OSAL_ERR_TIMEOUT or OSAL_ERR_MUTEX.
 */
static inline osal_err_t osal_mutex_lock(osal_mutex_t *mutex, uint32_t timeout_ms)
{
    osal_err_t err = OSAL_ERR_INVALID;

    if ((mutex != NULL) && osal_in_interrupt())
    {
        /* No mutex in an interrupt. */
        err = OSAL_ERR_MUTEX;
    }
    else if (mutex != NULL)
    {
        uint32_t   ticks  = osWaitForever;
        osStatus_t status = osError;

        /* HAL_MAX_DELAY waits for ever. Anything else becomes ticks. */
        if (timeout_ms != HAL_MAX_DELAY)
        {
            ticks = osal_ticks(timeout_ms);
        }

        status = osMutexAcquire(mutex->mutex, ticks);

        if (status == osOK)
        {
            /* Taken. */
            err = OSAL_ERR_NONE;
        }
        else if ((status == osErrorTimeout) || (status == osErrorResource))
        {
            /* Another thread kept it for the whole wait. */
            err = OSAL_ERR_TIMEOUT;
        }
        else
        {
            /* A mutex never created. */
            err = OSAL_ERR_MUTEX;
        }
    }
    else
    {
        /* A NULL mutex. */
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
    if ((mutex != NULL) && !osal_in_interrupt())
    {
        (void)osMutexRelease(mutex->mutex);
    }
}

/*****************************************************************************************************/
/**
 * @brief Wait ms milliseconds, letting other threads run.
 *
 * @param[in] ms  Milliseconds. 0 returns at once, and HAL_MAX_DELAY never returns.
 *                From an interrupt it returns at once, as on every port.
 */
static inline void osal_delay_ms(uint32_t ms)
{
    if (osal_in_interrupt())
    {
        /* No sleep in an interrupt. */
    }
    else if (ms == HAL_MAX_DELAY)
    {
        /* For ever, as HAL_MAX_DELAY is for a mutex. One sleep, even the
           longest, ends after 49 days, so the thread sleeps again. */
        for (;;)
        {
            (void)osDelay(osWaitForever);
        }
    }
    else if (ms > 0U)
    {
        (void)osDelay(osal_ticks(ms));
    }
    else
    {
        /* 0 returns at once, since a zero sleep is not the same on every RTOS. */
    }
}

/*****************************************************************************************************/
/**
 * @brief Take memory from the FreeRTOS heap.
 *
 * @param[in] size  Bytes. 0 gives NULL.
 * @return The memory, or NULL when the heap has not enough, or from an interrupt.
 */
static inline void *osal_malloc(size_t size)
{
    void *memory = NULL;

    /* FreeRTOS's heap is not for an interrupt. */
    if ((size > 0U) && !osal_in_interrupt())
    {
        memory = pvPortMalloc(size);
    }

    return memory;
}

/*****************************************************************************************************/
/**
 * @brief Give memory from osal_malloc() back.
 *
 * @param[in] memory  Memory from osal_malloc(). NULL does nothing, and so does a call from an interrupt.
 */
static inline void osal_free(void *memory)
{
    if ((memory != NULL) && !osal_in_interrupt())
    {
        vPortFree(memory);
    }
}

/*****************************************************************************************************/
/**
 * @brief Create a task and start it, its stack and control block from the FreeRTOS heap.
 *
 * The task runs function(argument). If function returns, the task stops for
 * good, as one that deletes itself does, and keeps its memory until another
 * task deletes it.
 *
 * @param[out] task        Task to create, static or global. One that has a task already is refused.
 * @param[in]  name        Name, for a debugger. NULL gives "osal".
 * @param[in]  function    What the task runs.
 * @param[in]  argument    Given to function.
 * @param[in]  stack_size  Stack in bytes, OSAL_TASK_STACK_MIN at least.
 * @param[in]  priority    0, the lowest, to OSAL_TASK_PRIORITY_MAX, the highest.
 * @return OSAL_ERR_NONE, OSAL_ERR_INVALID or OSAL_ERR_TASK.
 */
static inline osal_err_t osal_task_create(osal_task_t *task, const char *name, osal_task_func_t function,
                                          void *argument, uint32_t stack_size, uint32_t priority)
{
    osal_err_t err = OSAL_ERR_INVALID;

    /* ST's layer gives FreeRTOS the stack in words, in 16 bits. */
    if ((task == NULL) || (function == NULL) || (priority > OSAL_TASK_PRIORITY_MAX) ||
        (stack_size < OSAL_TASK_STACK_MIN) || ((stack_size / sizeof(StackType_t)) > 0xFFFFU))
    {
        /* Refused before the RTOS sees it. */
    }
    else if (osal_in_interrupt() || (task->thread != NULL))
    {
        /* FreeRTOS makes no task in an interrupt, and the task this one
           already has would be lost. */
        err = OSAL_ERR_TASK;
    }
    else
    {
        osThreadAttr_t attr;
        int32_t        lock = 0;

        /* Zeroed, so no control block or stack is given, and ST's layer
           takes both from the heap. */
        (void)memset(&attr, 0, sizeof(attr));

        attr.name       = (name != NULL) ? name : "osal";
        attr.stack_size = stack_size;
        attr.priority   = (osPriority_t)((uint32_t)osPriorityIdle + priority);

        /* The new task may have the higher priority and run at once. The
           scheduler waits until osal_task_t is filled in, so the task finds
           itself there. Only a lock taken here is let go: ST's
           osKernelRestoreLock() would lock a second time a scheduler that
           was locked already. Before the start there is no lock to take, and
           no task runs. */
        lock           = osKernelLock();
        task->function = function;
        task->argument = argument;
        task->thread   = osThreadNew(osal_task_run, task, &attr);

        if (lock == 0)
        {
            (void)osKernelUnlock();
        }

        /* NULL most often means the FreeRTOS heap is too small. */
        err = (task->thread != NULL) ? OSAL_ERR_NONE : OSAL_ERR_TASK;
    }

    return err;
}

/*****************************************************************************************************/
/**
 * @brief Delete a task. One that deletes itself stops for good, and keeps its memory.
 *
 * From another task, the task ends at once and its memory goes back to the
 * heap. A task that deletes itself never returns from here, and its memory
 * stays taken until another task deletes it, since ThreadX cannot free a task
 * from inside it. Never delete a task that holds a mutex.
 *
 * @param[in,out] task  Task from osal_task_create().
 * @return OSAL_ERR_NONE, OSAL_ERR_INVALID, or OSAL_ERR_TASK for no task, or from an interrupt.
 */
static inline osal_err_t osal_task_delete(osal_task_t *task)
{
    osal_err_t err = OSAL_ERR_INVALID;

    if ((task != NULL) && (osal_in_interrupt() || (task->thread == NULL)))
    {
        /* No task, or a call from an interrupt. */
        err = OSAL_ERR_TASK;
    }
    else if ((task != NULL) && (task->thread == osThreadGetId()))
    {
        /* Itself. FreeRTOS would free it later from its idle task, and
           ThreadX cannot at all, so on every port it only stops. Never
           returns. */
        osal_delay_ms(HAL_MAX_DELAY);
    }
    else if (task != NULL)
    {
        err = (osThreadTerminate(task->thread) == osOK) ? OSAL_ERR_NONE : OSAL_ERR_TASK;

        if (err == OSAL_ERR_NONE)
        {
            /* Gone, so a second delete is refused, and the task can be
               created again. */
            task->thread = NULL;
        }
    }
    else
    {
        /* A NULL task. */
    }

    return err;
}

/*****************************************************************************************************/
/**
 * @brief Suspend a task until osal_task_resume(). A task may suspend itself.
 *
 * @param[in,out] task  Task from osal_task_create().
 * @return OSAL_ERR_NONE, OSAL_ERR_INVALID, or OSAL_ERR_TASK for no task, or from an interrupt.
 */
static inline osal_err_t osal_task_suspend(osal_task_t *task)
{
    osal_err_t err = OSAL_ERR_INVALID;

    if ((task != NULL) && (osal_in_interrupt() || (task->thread == NULL)))
    {
        /* No task, or a call from an interrupt. */
        err = OSAL_ERR_TASK;
    }
    else if (task != NULL)
    {
        err = (osThreadSuspend(task->thread) == osOK) ? OSAL_ERR_NONE : OSAL_ERR_TASK;
    }
    else
    {
        /* A NULL task. */
    }

    return err;
}

/*****************************************************************************************************/
/**
 * @brief Resume a task suspended with osal_task_suspend(). Any other task is left as it is.
 *
 * @param[in,out] task  Task from osal_task_create().
 * @return OSAL_ERR_NONE, OSAL_ERR_INVALID, or OSAL_ERR_TASK for no task, or from an interrupt.
 */
static inline osal_err_t osal_task_resume(osal_task_t *task)
{
    osal_err_t err = OSAL_ERR_INVALID;

    if ((task != NULL) && (osal_in_interrupt() || (task->thread == NULL)))
    {
        /* No task, or a call from an interrupt. */
        err = OSAL_ERR_TASK;
    }
    else if (task != NULL)
    {
        err = (osThreadResume(task->thread) == osOK) ? OSAL_ERR_NONE : OSAL_ERR_TASK;
    }
    else
    {
        /* A NULL task. */
    }

    return err;
}

/*
 * ****************************************************************************************************
 * Private function implementations
 * ****************************************************************************************************
*/

/*****************************************************************************************************/
/**
 * @brief Turn milliseconds into ticks with FreeRTOS's pdMS_TO_TICKS(), and never a wait into none.
 *
 * @param[in] ms  Milliseconds.
 * @return Ticks: what pdMS_TO_TICKS() gives, and at least one for any wait.
 */
static inline uint32_t osal_ticks(uint32_t ms)
{
    uint32_t ticks = (uint32_t)pdMS_TO_TICKS(ms);

    /* pdMS_TO_TICKS() drops what is left of a tick, so on a 100 Hz tick 5 ms
       is no tick at all, and a sleep or a mutex wait would end at once. A
       wait that was asked for gets one tick at least. */
    if ((ticks == 0U) && (ms > 0U))
    {
        ticks = 1U;
    }

    return ticks;
}

/*****************************************************************************************************/
/**
 * @brief Whether a task runs, with the RTOS started, where FreeRTOS's own critical section works.
 *
 * @return false in an interrupt, and before the RTOS starts.
 */
static inline bool osal_in_task(void)
{
    bool in_task = false;

    if (!osal_in_interrupt())
    {
        osKernelState_t state = osKernelGetState();

        /* Locked is running, with the scheduler held. Before the start it is
           inactive or ready. */
        in_task = (state == osKernelRunning) || (state == osKernelLocked);
    }

    return in_task;
}

/*****************************************************************************************************/
/**
 * @brief Where every task starts. It runs the task's function, and stops the task for good if it returns.
 *
 * @param[in] argument  The task's osal_task_t.
 */
static inline void osal_task_run(void *argument)
{
    const osal_task_t *task = (const osal_task_t *)argument;

    task->function(task->argument);

    /* Returned. FreeRTOS would stop the CPU here and ThreadX ends the task,
       so on every port it stops for good, and keeps its memory until another
       task deletes it. */
    osal_delay_ms(HAL_MAX_DELAY);
}

#ifdef __cplusplus
}
#endif

#endif /* OSAL_CMSIS_V2_H */

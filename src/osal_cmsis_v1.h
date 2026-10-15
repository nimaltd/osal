/**
 * @file        osal_cmsis_v1.h
 * @brief       The osal port for FreeRTOS through CMSIS-RTOS v1.
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
 * @note        Included by osal.h when OSAL_RTOS is OSAL_RTOS_CMSIS_V1. Include
 *              osal.h, never this file. CMSIS-RTOS v1 takes milliseconds and
 *              turns them into ticks itself, so every time goes in as it is.
 *              The mutex is FreeRTOS's recursive one, through ST's
 *              osRecursiveMutex calls, as a ThreadX mutex is. A task and the
 *              heap come from the FreeRTOS heap, and a critical section is
 *              FreeRTOS's own.
 */

#ifndef OSAL_CMSIS_V1_H
#define OSAL_CMSIS_V1_H

/*
 * ****************************************************************************************************
 * Includes
 * ****************************************************************************************************
*/

#include <string.h>

#include "cmsis_os.h"

/*
 * ****************************************************************************************************
 * Configuration checks
 * ****************************************************************************************************
*/

#if !defined(OSAL_H) || (OSAL_RTOS != OSAL_RTOS_CMSIS_V1)
#error "osal_cmsis_v1.h is included by osal.h when OSAL_RTOS is OSAL_RTOS_CMSIS_V1. Include osal.h"
#endif

/* cmsis_os.h brings FreeRTOS.h. Without recursive mutexes, ST's layer makes
   no mutex at all, and every osal_mutex_create() would fail. */
#if !defined(configUSE_RECURSIVE_MUTEXES) || (configUSE_RECURSIVE_MUTEXES != 1)
#error "osal needs configUSE_RECURSIVE_MUTEXES 1: in CubeMX, FreeRTOS, Config parameters, USE_RECURSIVE_MUTEXES Enabled"
#endif

/* osal takes every mutex and task from the FreeRTOS heap. Without it, ST's
   layer makes neither, and osal_mutex_create() and osal_task_create() would
   fail. FreeRTOS.h makes it 1 when FreeRTOSConfig.h leaves it out. */
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

/* The highest task priority. 0 is the lowest, FreeRTOS's idle priority. ST's
   layer turns osPriorityIdle + n into FreeRTOS priority n, so osal gives every
   priority configMAX_PRIORITIES allows, not only the seven osPriority names. */
#define OSAL_TASK_PRIORITY_MAX  ((uint32_t)configMAX_PRIORITIES - 1U)

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
    osMutexId mutex; /**< The RTOS mutex. */

} osal_mutex_t;

/*****************************************************************************************************/
/**
 * @brief A task. Make it static or global: it lasts as long as the task does.
 */
typedef struct
{
    osThreadId       thread;    /**< The RTOS task, NULL while there is none. */
    osal_task_func_t function;  /**< What the task runs.                      */
    void             *argument; /**< Given to function.                       */

} osal_task_t;

/*
 * ****************************************************************************************************
 * Private function prototypes
 * ****************************************************************************************************
*/

static inline bool osal_in_task(void);
static inline void osal_task_run(void const *argument);

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
    /* 1 ms, which FreeRTOS's CMSIS-RTOS v1 layer makes one tick at least. ST's
       osDelay() does not check for an interrupt, so osal does. */
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
        /* With no control block in the definition, every call allocates a
           mutex of its own, from the RTOS heap, so a second call makes a new
           one. FreeRTOS gives every mutex priority inheritance. */
        osMutexDef(osal_mutex);

        mutex->mutex = osRecursiveMutexCreate(osMutex(osal_mutex));
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
    osal_err_t err    = OSAL_ERR_INVALID;
    osStatus   status = osOK;

    if ((mutex != NULL) && osal_in_interrupt())
    {
        /* ST's layer would try from an interrupt, where the other ports refuse. */
        err = OSAL_ERR_MUTEX;
    }
    else if (mutex != NULL)
    {
        /* CMSIS-RTOS v1 waits in milliseconds, and its osWaitForever is the
           same value as HAL_MAX_DELAY, so the timeout goes in as it is. */
        status = osRecursiveMutexWait(mutex->mutex, timeout_ms);

        if (status == osOK)
        {
            /* Taken. */
            err = OSAL_ERR_NONE;
        }
        else if (status == osErrorOS)
        {
            /* ST's layer says osErrorOS when another thread kept it for the
               whole wait, or for a wait of 0 held it at once. */
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
        (void)osRecursiveMutexRelease(mutex->mutex);
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
        /* No sleep in an interrupt. ST's osDelay() would try. */
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
        /* CMSIS-RTOS v1 takes milliseconds, and turns them into ticks itself. */
        (void)osDelay(ms);
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

    /* FreeRTOS counts the stack in words, in 16 bits. */
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
        osThreadDef_t def;

        /* Zeroed, so the stack and control block ST's layer has for static
           allocation stay NULL, and it takes both from the heap. */
        (void)memset(&def, 0, sizeof(def));

        /* ST's layer hands stacksize to FreeRTOS as it is, which counts
           words, whatever the comment beside it says. */
        def.name      = (char *)((name != NULL) ? name : "osal");
        def.pthread   = osal_task_run;
        def.tpriority = (osPriority)((int32_t)osPriorityIdle + (int32_t)priority);
        def.instances = 1U;
        def.stacksize = stack_size / (uint32_t)sizeof(StackType_t);

        /* The new task may have the higher priority and run at once. The
           scheduler waits until osal_task_t is filled in, so the task finds
           itself there. */
        (void)osThreadSuspendAll();
        task->function = function;
        task->argument = argument;
        task->thread   = osThreadCreate(&def, task);
        (void)osThreadResumeAll();

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
        /* osErrorResource when INCLUDE_vTaskSuspend is 0. */
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
        /* No task, or a call from an interrupt, which ST's layer would take
           and the other ports refuse. */
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
 * @brief Whether a task runs, with the RTOS started, where FreeRTOS's own critical section works.
 *
 * @return false in an interrupt, and before the RTOS starts.
 */
static inline bool osal_in_task(void)
{
    /* osKernelRunning() is 0 before the start, 1 after it, and -1 where
       FreeRTOS cannot tell, which is taken as started. */
    return (!osal_in_interrupt() && (osKernelRunning() != 0));
}

/*****************************************************************************************************/
/**
 * @brief Where every task starts. It runs the task's function, and stops the task for good if it returns.
 *
 * @param[in] argument  The task's osal_task_t.
 */
static inline void osal_task_run(void const *argument)
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

#endif /* OSAL_CMSIS_V1_H */

/**
 * @file        osal_threadx.h
 * @brief       The osal port for ThreadX.
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
 * @note        Included by osal.h when OSAL_RTOS is OSAL_RTOS_THREADX. Include
 *              osal.h, never this file. ThreadX takes ticks, and has no macro
 *              of its own to turn milliseconds into them, only its tick rate,
 *              TX_TIMER_TICKS_PER_SECOND. So osal does the sum FreeRTOS's
 *              pdMS_TO_TICKS() does. ThreadX has no heap, only byte pools,
 *              so osal's heap is the first pool created, which in a CubeMX
 *              project is CubeMX's own. A task's stack comes from it.
 */

#ifndef OSAL_THREADX_H
#define OSAL_THREADX_H

/*
 * ****************************************************************************************************
 * Includes
 * ****************************************************************************************************
*/

#include "tx_api.h"
#include "tx_byte_pool.h"

/*
 * ****************************************************************************************************
 * Configuration checks
 * ****************************************************************************************************
*/

#if !defined(OSAL_H) || (OSAL_RTOS != OSAL_RTOS_THREADX)
#error "osal_threadx.h is included by osal.h when OSAL_RTOS is OSAL_RTOS_THREADX. Include osal.h"
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

/* The highest task priority. ThreadX counts the other way, 0 its highest, so
   osal's priority p is ThreadX's TX_MAX_PRIORITIES - 1 - p, and osal's 0 is
   ThreadX's lowest. */
#define OSAL_TASK_PRIORITY_MAX  ((uint32_t)TX_MAX_PRIORITIES - 1U)

/* The smallest stack in bytes: ThreadX's own. */
#define OSAL_TASK_STACK_MIN     ((uint32_t)TX_MINIMUM_STACK)

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
    TX_MUTEX mutex; /**< The RTOS mutex, kept here so no heap is needed. */

} osal_mutex_t;

/*****************************************************************************************************/
/**
 * @brief A task. Make it static or global: it lasts as long as the task does.
 */
typedef struct
{
    TX_THREAD        thread;    /**< The RTOS task. First, so a task finds itself from it. */
    osal_task_func_t function;  /**< What the task runs.                                   */
    void             *argument; /**< Given to function.                                    */
    void             *stack;    /**< From osal_malloc(), NULL while there is no task.      */

} osal_task_t;

/*
 * ****************************************************************************************************
 * Private function prototypes
 * ****************************************************************************************************
*/

static inline ULONG osal_ticks(uint32_t ms);
static inline VOID  osal_task_run(ULONG input);

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
        tx_thread_relinquish();
    }
}

/*****************************************************************************************************/
/**
 * @brief Pause a loop that waits on hardware, so every other thread runs meanwhile.
 *
 * Sleeps one tick, the shortest sleep the RTOS has, so threads of every
 * priority run. Call it between two questions to a chip or a DMA that is still
 * busy. A yield would let no thread of lower priority run until the wait ends,
 * which for an erase can be minutes.
 */
static inline void osal_pause(void)
{
    if (!osal_in_interrupt())
    {
        (void)tx_thread_sleep(1U);
    }
}

/*****************************************************************************************************/
/**
 * @brief Mask the interrupts, so nothing else runs until osal_critical_exit().
 *
 * Works in a task, in an interrupt and before the RTOS starts, and nests: each
 * osal_critical_exit() puts back what its osal_critical_enter() found.
 *
 * @return What to give osal_critical_exit().
 */
static inline uint32_t osal_critical_enter(void)
{
    /* ThreadX's own. It masks, and returns what was masked before, in a
       thread, in an interrupt and before the start alike. */
    return (uint32_t)tx_interrupt_control(TX_INT_DISABLE);
}

/*****************************************************************************************************/
/**
 * @brief Put the interrupts back as osal_critical_enter() found them.
 *
 * @param[in] state  What osal_critical_enter() returned.
 */
static inline void osal_critical_exit(uint32_t state)
{
    (void)tx_interrupt_control((UINT)state);
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
    osal_err_t err    = OSAL_ERR_INVALID;
    UINT       status = TX_SUCCESS;

    if ((mutex != NULL) && osal_in_interrupt())
    {
        /* ThreadX makes no mutex in an interrupt. */
        err = OSAL_ERR_MUTEX;
    }
    else if (mutex != NULL)
    {
        /* Inside osal_mutex_t, so no heap is needed. Priority inheritance, so
           a low priority thread holding the mutex is not held up by a medium
           one while a high priority thread waits for it. A ThreadX mutex is
           recursive by nature. */
        status = tx_mutex_create(&mutex->mutex, (CHAR *)"osal", TX_INHERIT);

        if (status == TX_MUTEX_ERROR)
        {
            /* Already created. FreeRTOS makes a new mutex each time, so here
               the old one goes and a new one takes its place. */
            status = tx_mutex_delete(&mutex->mutex);

            if (status == TX_SUCCESS)
            {
                status = tx_mutex_create(&mutex->mutex, (CHAR *)"osal", TX_INHERIT);
            }
        }

        err = (status == TX_SUCCESS) ? OSAL_ERR_NONE : OSAL_ERR_MUTEX;
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
        ULONG ticks  = TX_WAIT_FOREVER;
        UINT  status = TX_SUCCESS;

        /* HAL_MAX_DELAY waits for ever. Anything else becomes ticks. */
        if (timeout_ms != HAL_MAX_DELAY)
        {
            ticks = osal_ticks(timeout_ms);
        }

        status = tx_mutex_get(&mutex->mutex, ticks);

        if (status == TX_SUCCESS)
        {
            /* Taken. */
            err = OSAL_ERR_NONE;
        }
        else if (status == TX_NOT_AVAILABLE)
        {
            /* Another thread kept it for the whole wait. */
            err = OSAL_ERR_TIMEOUT;
        }
        else
        {
            /* A mutex never created, or a call from outside a thread. */
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
        (void)tx_mutex_put(&mutex->mutex);
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
        /* For ever, as HAL_MAX_DELAY is for a mutex. ThreadX has no sleep
           without end, and its longest ends after 49 days at 1 kHz, so the
           thread sleeps again. */
        for (;;)
        {
            (void)tx_thread_sleep(TX_WAIT_FOREVER);
        }
    }
    else if (ms > 0U)
    {
        (void)tx_thread_sleep(osal_ticks(ms));
    }
    else
    {
        /* 0 returns at once, since a zero sleep is not the same on every RTOS. */
    }
}

/*****************************************************************************************************/
/**
 * @brief Take memory from the first ThreadX byte pool created.
 *
 * In a CubeMX project that is its own Tx App memory pool, of
 * TX_APP_MEM_POOL_SIZE bytes, so make that big enough for osal too.
 *
 * @param[in] size  Bytes. 0 gives NULL.
 * @return The memory, or NULL when the pool has not enough or there is none, or from an interrupt.
 */
static inline void *osal_malloc(size_t size)
{
    void         *memory = NULL;
    TX_BYTE_POOL *pool   = _tx_byte_pool_created_ptr;

    /* ThreadX keeps its pools in a list that starts with the first one
       created, and takes no memory from one in an interrupt. */
    if ((size > 0U) && (pool != NULL) && !osal_in_interrupt())
    {
        if (tx_byte_allocate(pool, &memory, (ULONG)size, TX_NO_WAIT) != TX_SUCCESS)
        {
            /* Not enough in the pool. */
            memory = NULL;
        }
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
        /* ThreadX finds the pool from the memory itself. */
        (void)tx_byte_release(memory);
    }
}

/*****************************************************************************************************/
/**
 * @brief Create a task and start it, its stack from osal_malloc().
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

    if ((task == NULL) || (function == NULL) || (priority > OSAL_TASK_PRIORITY_MAX) ||
        (stack_size < OSAL_TASK_STACK_MIN))
    {
        /* Refused before the RTOS sees it. */
    }
    else if (osal_in_interrupt() || (task->stack != NULL))
    {
        /* ThreadX makes no thread in an interrupt, and the task this one
           already has would be lost. */
        err = OSAL_ERR_TASK;
    }
    else
    {
        UINT tx_priority = (UINT)(OSAL_TASK_PRIORITY_MAX - priority);
        UINT status      = TX_SUCCESS;

        /* All set first: a task of higher priority runs before
           tx_thread_create() returns. */
        task->function = function;
        task->argument = argument;
        task->stack    = osal_malloc(stack_size);

        if (task->stack == NULL)
        {
            /* The pool is too small, or there is none. */
            err = OSAL_ERR_TASK;
        }
        else
        {
            /* A time slice of one tick, so tasks of the same priority take
               turns every tick, as FreeRTOS's do. A preemption threshold of
               the priority itself, which turns it off, since FreeRTOS has
               none. */
            status = tx_thread_create(&task->thread, (CHAR *)((name != NULL) ? name : "osal"), osal_task_run,
                                      0U, task->stack, (ULONG)stack_size, tx_priority, tx_priority, 1U,
                                      TX_AUTO_START);

            err = (status == TX_SUCCESS) ? OSAL_ERR_NONE : OSAL_ERR_TASK;

            if (err != OSAL_ERR_NONE)
            {
                osal_free(task->stack);
                task->stack = NULL;
            }
        }
    }

    return err;
}

/*****************************************************************************************************/
/**
 * @brief Delete a task. One that deletes itself stops for good, and keeps its memory.
 *
 * From another task, the task ends at once and its memory goes back to the
 * pool. A task that deletes itself never returns from here, and its memory
 * stays taken until another task deletes it, since ThreadX cannot delete a
 * thread from inside it. Never delete a task that holds a mutex.
 *
 * @param[in,out] task  Task from osal_task_create().
 * @return OSAL_ERR_NONE, OSAL_ERR_INVALID, or OSAL_ERR_TASK for no task, or from an interrupt.
 */
static inline osal_err_t osal_task_delete(osal_task_t *task)
{
    osal_err_t err    = OSAL_ERR_INVALID;
    UINT       status = TX_SUCCESS;

    if ((task != NULL) && (osal_in_interrupt() || (task->stack == NULL)))
    {
        /* No task, or a call from an interrupt. */
        err = OSAL_ERR_TASK;
    }
    else if (task != NULL)
    {
        /* Ended first, since ThreadX deletes only a thread that has. One that
           has ended already is ended again without a word, and a task that
           ends itself here never returns. */
        status = tx_thread_terminate(&task->thread);

        if (status == TX_SUCCESS)
        {
            status = tx_thread_delete(&task->thread);
        }

        err = (status == TX_SUCCESS) ? OSAL_ERR_NONE : OSAL_ERR_TASK;

        if (err == OSAL_ERR_NONE)
        {
            /* Gone, so a second delete is refused, and the task can be
               created again. */
            osal_free(task->stack);
            task->stack = NULL;
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
    osal_err_t err    = OSAL_ERR_INVALID;
    UINT       status = TX_SUCCESS;

    if ((task != NULL) && (osal_in_interrupt() || (task->stack == NULL)))
    {
        /* No task, or a call from an interrupt, which ThreadX would take and
           FreeRTOS refuses. */
        err = OSAL_ERR_TASK;
    }
    else if (task != NULL)
    {
        status = tx_thread_suspend(&task->thread);

        /* TX_SUSPEND_ERROR is a task that has ended, and stays ended, as
           FreeRTOS's does. */
        err = ((status == TX_SUCCESS) || (status == TX_SUSPEND_ERROR)) ? OSAL_ERR_NONE : OSAL_ERR_TASK;
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
    osal_err_t err    = OSAL_ERR_INVALID;
    UINT       status = TX_SUCCESS;

    if ((task != NULL) && (osal_in_interrupt() || (task->stack == NULL)))
    {
        /* No task, or a call from an interrupt, which ThreadX would take and
           FreeRTOS refuses. */
        err = OSAL_ERR_TASK;
    }
    else if (task != NULL)
    {
        status = tx_thread_resume(&task->thread);

        /* TX_RESUME_ERROR is a task that was not suspended, left as it is,
           as FreeRTOS leaves it. TX_SUSPEND_LIFTED is a suspend that was
           waiting for a sleep to end, now called off. */
        err = ((status == TX_SUCCESS) || (status == TX_RESUME_ERROR) || (status == TX_SUSPEND_LIFTED))
                  ? OSAL_ERR_NONE
                  : OSAL_ERR_TASK;
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
 * @brief Turn milliseconds into ticks, as FreeRTOS's pdMS_TO_TICKS() does, and never a wait into none.
 *
 * @param[in] ms  Milliseconds.
 * @return Ticks: ms times TX_TIMER_TICKS_PER_SECOND over 1000, and at least one for any wait.
 */
static inline ULONG osal_ticks(uint32_t ms)
{
    ULONG ticks = (ULONG)(((uint64_t)ms * (uint64_t)TX_TIMER_TICKS_PER_SECOND) / 1000U);

    /* What is left of a tick is dropped, so on ThreadX's default 100 Hz tick
       5 ms is no tick at all, and a sleep or a mutex wait would end at once. A
       wait that was asked for gets one tick at least. */
    if ((ticks == 0U) && (ms > 0U))
    {
        ticks = 1U;
    }

    return ticks;
}

/*****************************************************************************************************/
/**
 * @brief Where every task starts. It runs the task's function, and ThreadX ends the task if it returns.
 *
 * @param[in] input  Unused: the task is found from the running thread.
 */
static inline VOID osal_task_run(ULONG input)
{
    /* The thread is the first member of osal_task_t, so the thread running
       is its task. ULONG is not wide enough for a pointer on every port. */
    const osal_task_t *task = (const osal_task_t *)tx_thread_identify();

    (void)input;

    task->function(task->argument);

    /* Returned. ThreadX ends the task here, and its memory stays taken until
       another task deletes it, as on every port. */
}

#ifdef __cplusplus
}
#endif

#endif /* OSAL_THREADX_H */

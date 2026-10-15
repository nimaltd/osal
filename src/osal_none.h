/**
 * @file        osal_none.h
 * @brief       The osal port for bare metal, with no RTOS.
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
 * @note        Included by osal.h when OSAL_RTOS is OSAL_RTOS_NONE. Include
 *              osal.h, never this file.
 */

#ifndef OSAL_NONE_H
#define OSAL_NONE_H

/*
 * ****************************************************************************************************
 * Includes
 * ****************************************************************************************************
*/

#include <stdlib.h>

/*
 * ****************************************************************************************************
 * Configuration checks
 * ****************************************************************************************************
*/

#if !defined(OSAL_H) || (OSAL_RTOS != OSAL_RTOS_NONE)
#error "osal_none.h is included by osal.h when OSAL_RTOS is OSAL_RTOS_NONE. Include osal.h"
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

/* No task runs without an RTOS, so there is one priority and no stack. */
#define OSAL_TASK_PRIORITY_MAX  0U
#define OSAL_TASK_STACK_MIN     0U

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
    uint8_t unused; /**< Bare metal needs none, and C wants a member. */

} osal_mutex_t;

/*****************************************************************************************************/
/**
 * @brief A task. Bare metal has none, and osal_task_create() says so.
 */
typedef struct
{
    uint8_t unused; /**< C wants a member. */

} osal_task_t;

/*
 * ****************************************************************************************************
 * Public function implementations
 * ****************************************************************************************************
*/

/*****************************************************************************************************/
/**
 * @brief Let the other threads of the same priority run, and come back at once if none is waiting.
 *
 * Bare metal there is no other thread, so it returns at once.
 */
static inline void osal_yield(void)
{
    /* Nothing else could run. */
}

/*****************************************************************************************************/
/**
 * @brief Pause a loop that waits on hardware, so every other thread runs meanwhile.
 *
 * Bare metal it returns at once, and the caller asks the hardware again
 * straight away: nothing else could run meanwhile, so a sleep would only make
 * the wait end later.
 */
static inline void osal_pause(void)
{
    /* Nothing else could run meanwhile. */
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
    /* PRIMASK masks every interrupt. Kept first, so an inner exit leaves the
       interrupts masked for the outer one. */
    uint32_t state = __get_PRIMASK();

    __disable_irq();

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
    __set_PRIMASK(state);
}

/*****************************************************************************************************/
/**
 * @brief Create a mutex. A second call on the same one gives it back as new.
 *
 * @param[out] mutex  Mutex to create.
 * @return OSAL_ERR_NONE, OSAL_ERR_INVALID, or OSAL_ERR_MUTEX from an interrupt.
 */
static inline osal_err_t osal_mutex_create(osal_mutex_t *mutex)
{
    osal_err_t err = OSAL_ERR_INVALID;

    if ((mutex != NULL) && osal_in_interrupt())
    {
        /* Refused, as every RTOS port refuses it. */
        err = OSAL_ERR_MUTEX;
    }
    else if (mutex != NULL)
    {
        /* Bare metal has no other thread to keep out. */
        mutex->unused = 0U;
        err           = OSAL_ERR_NONE;
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
 * @return OSAL_ERR_NONE, OSAL_ERR_INVALID, or OSAL_ERR_MUTEX from an interrupt.
 */
static inline osal_err_t osal_mutex_lock(osal_mutex_t *mutex, uint32_t timeout_ms)
{
    osal_err_t err = OSAL_ERR_INVALID;

    /* Bare metal has no other thread to keep out, so there is never a wait. */
    (void)timeout_ms;

    if ((mutex != NULL) && osal_in_interrupt())
    {
        /* Refused, as every RTOS port refuses it. */
        err = OSAL_ERR_MUTEX;
    }
    else if (mutex != NULL)
    {
        err = OSAL_ERR_NONE;
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
    /* Nothing was taken, so there is nothing to give back. */
    (void)mutex;
}

/*****************************************************************************************************/
/**
 * @brief Wait ms milliseconds.
 *
 * @param[in] ms  Milliseconds. 0 returns at once, and HAL_MAX_DELAY never returns.
 *                From an interrupt it returns at once, as on every RTOS port.
 */
static inline void osal_delay_ms(uint32_t ms)
{
    if (osal_in_interrupt())
    {
        /* No wait in an interrupt: an RTOS cannot sleep there either. */
    }
    else if (ms == HAL_MAX_DELAY)
    {
        /* For ever, as HAL_MAX_DELAY is for a mutex. HAL_Delay() itself ends
           after 49 days, so it waits again. */
        for (;;)
        {
            HAL_Delay(HAL_MAX_DELAY);
        }
    }
    else if (ms > 0U)
    {
        HAL_Delay(ms);
    }
    else
    {
        /* 0 returns at once, since HAL_Delay(0) still waits a tick. */
    }
}

/*****************************************************************************************************/
/**
 * @brief Take memory from the heap, the C library's here.
 *
 * @param[in] size  Bytes. 0 gives NULL.
 * @return The memory, or NULL when there is not enough, or from an interrupt.
 */
static inline void *osal_malloc(size_t size)
{
    void *memory = NULL;

    /* Not from an interrupt: the code it cut into may be inside malloc(). */
    if ((size > 0U) && !osal_in_interrupt())
    {
        memory = malloc(size);
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
        free(memory);
    }
}

/*****************************************************************************************************/
/**
 * @brief Create a task and start it. Bare metal has no task, so it is refused.
 *
 * @param[out] task        Task to create.
 * @param[in]  name        Name, for a debugger.
 * @param[in]  function    What the task runs.
 * @param[in]  argument    Given to function.
 * @param[in]  stack_size  Stack in bytes.
 * @param[in]  priority    0, the only one.
 * @return OSAL_ERR_INVALID for a NULL task or function, and OSAL_ERR_TASK otherwise.
 */
static inline osal_err_t osal_task_create(osal_task_t *task, const char *name, osal_task_func_t function,
                                          void *argument, uint32_t stack_size, uint32_t priority)
{
    osal_err_t err = OSAL_ERR_INVALID;

    (void)name;
    (void)argument;
    (void)stack_size;
    (void)priority;

    if ((task != NULL) && (function != NULL))
    {
        /* No RTOS to run it. */
        err = OSAL_ERR_TASK;
    }

    return err;
}

/*****************************************************************************************************/
/**
 * @brief Delete a task. Bare metal has none.
 *
 * @param[in,out] task  Task from osal_task_create().
 * @return OSAL_ERR_INVALID for a NULL task, and OSAL_ERR_TASK otherwise.
 */
static inline osal_err_t osal_task_delete(osal_task_t *task)
{
    return (task != NULL) ? OSAL_ERR_TASK : OSAL_ERR_INVALID;
}

/*****************************************************************************************************/
/**
 * @brief Suspend a task. Bare metal has none.
 *
 * @param[in,out] task  Task from osal_task_create().
 * @return OSAL_ERR_INVALID for a NULL task, and OSAL_ERR_TASK otherwise.
 */
static inline osal_err_t osal_task_suspend(osal_task_t *task)
{
    return (task != NULL) ? OSAL_ERR_TASK : OSAL_ERR_INVALID;
}

/*****************************************************************************************************/
/**
 * @brief Resume a task. Bare metal has none.
 *
 * @param[in,out] task  Task from osal_task_create().
 * @return OSAL_ERR_INVALID for a NULL task, and OSAL_ERR_TASK otherwise.
 */
static inline osal_err_t osal_task_resume(osal_task_t *task)
{
    return (task != NULL) ? OSAL_ERR_TASK : OSAL_ERR_INVALID;
}

#ifdef __cplusplus
}
#endif

#endif /* OSAL_NONE_H */

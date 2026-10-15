/**
 * @file        tx_api.h
 * @brief       Host stub for the ThreadX API.
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

#ifndef TX_API_H
#define TX_API_H

/*
 * ****************************************************************************************************
 * Macros
 * ****************************************************************************************************
*/

#define VOID                        void

#define TX_SUCCESS                  ((UINT)0x00)
#define TX_WAIT_ERROR               ((UINT)0x04)
#define TX_THREAD_ERROR             ((UINT)0x0E)
#define TX_NO_MEMORY                ((UINT)0x10)
#define TX_DELETE_ERROR             ((UINT)0x11)
#define TX_RESUME_ERROR             ((UINT)0x12)
#define TX_CALLER_ERROR             ((UINT)0x13)
#define TX_SUSPEND_ERROR            ((UINT)0x14)
#define TX_SUSPEND_LIFTED           ((UINT)0x19)
#define TX_MUTEX_ERROR              ((UINT)0x1C)
#define TX_NOT_AVAILABLE            ((UINT)0x1D)

#define TX_INHERIT                  ((UINT)1)
#define TX_AUTO_START               ((UINT)1)
#define TX_NO_WAIT                  ((ULONG)0)
#define TX_WAIT_FOREVER             ((ULONG)0xFFFFFFFFUL)

/* tx_port.h's defaults for Cortex-M. */
#define TX_MAX_PRIORITIES           32
#define TX_MINIMUM_STACK            200
#define TX_INT_DISABLE              1

/* The tick rate, a function here so the tests can change it. In a project it
   is a number in tx_user.h or tx_port.h, 100 unless set. */
#define TX_TIMER_TICKS_PER_SECOND   ((ULONG)test_tick_rate())

/*
 * ****************************************************************************************************
 * Types
 * ****************************************************************************************************
*/

/*****************************************************************************************************/
/**
 * @brief ThreadX's basic types, as its Cortex-M ports define them.
 */
typedef char          CHAR;
typedef unsigned int  UINT;
typedef unsigned long ULONG;
typedef unsigned char UCHAR;

/*****************************************************************************************************/
/**
 * @brief A mutex. The real one is a control block; this one is enough to count with.
 */
typedef struct
{
    ULONG tx_mutex_id;     /**< Set once created.            */
    UINT  tx_mutex_count;  /**< How many times it is held.   */

} TX_MUTEX;

/*****************************************************************************************************/
/**
 * @brief A thread. The real one is a control block; this one keeps what the tests check.
 */
typedef struct
{
    ULONG tx_thread_id;                    /**< Set once created.          */
    CHAR  *name;                           /**< Its name.                  */
    VOID  (*entry)(ULONG entry_input);     /**< Its function.              */
    ULONG input;                           /**< Given to entry.            */
    VOID  *stack;                          /**< Its stack.                 */
    ULONG stack_size;                      /**< In bytes.                  */
    UINT  priority;                        /**< 0 the highest.             */
    UINT  threshold;                       /**< Preemption threshold.      */
    ULONG time_slice;                      /**< Ticks, 0 for none.         */
    UINT  auto_start;                      /**< TX_AUTO_START or not.      */
    UINT  ended;                           /**< Terminated, so deletable.  */

} TX_THREAD;

/*****************************************************************************************************/
/**
 * @brief A byte pool. Only its address matters to osal.
 */
typedef struct
{
    ULONG tx_byte_pool_id; /**< Set once created. */

} TX_BYTE_POOL;

/*
 * ****************************************************************************************************
 * Public function prototypes
 * ****************************************************************************************************
*/

/*****************************************************************************************************/
/**
 * @brief The tick rate the tests have set.
 */
unsigned long test_tick_rate(void);

/*****************************************************************************************************/
/**
 * @brief Create a mutex.
 */
UINT tx_mutex_create(TX_MUTEX *mutex_ptr, CHAR *name_ptr, UINT inherit);

/*****************************************************************************************************/
/**
 * @brief Delete a mutex.
 */
UINT tx_mutex_delete(TX_MUTEX *mutex_ptr);

/*****************************************************************************************************/
/**
 * @brief Take a mutex, waiting up to wait_option ticks.
 */
UINT tx_mutex_get(TX_MUTEX *mutex_ptr, ULONG wait_option);

/*****************************************************************************************************/
/**
 * @brief Give a mutex back.
 */
UINT tx_mutex_put(TX_MUTEX *mutex_ptr);

/*****************************************************************************************************/
/**
 * @brief Let the threads of the same priority run.
 */
VOID tx_thread_relinquish(VOID);

/*****************************************************************************************************/
/**
 * @brief Sleep for timer_ticks.
 */
UINT tx_thread_sleep(ULONG timer_ticks);

/*****************************************************************************************************/
/**
 * @brief Mask or unmask the interrupts, and return what was set before.
 */
UINT tx_interrupt_control(UINT new_posture);

/*****************************************************************************************************/
/**
 * @brief Create a thread.
 */
UINT tx_thread_create(TX_THREAD *thread_ptr, CHAR *name_ptr, VOID (*entry_function)(ULONG entry_input),
                      ULONG entry_input, VOID *stack_start, ULONG stack_size, UINT priority,
                      UINT preempt_threshold, ULONG time_slice, UINT auto_start);

/*****************************************************************************************************/
/**
 * @brief End a thread. One that ends itself never returns.
 */
UINT tx_thread_terminate(TX_THREAD *thread_ptr);

/*****************************************************************************************************/
/**
 * @brief Delete a thread that has ended.
 */
UINT tx_thread_delete(TX_THREAD *thread_ptr);

/*****************************************************************************************************/
/**
 * @brief Suspend a thread.
 */
UINT tx_thread_suspend(TX_THREAD *thread_ptr);

/*****************************************************************************************************/
/**
 * @brief Resume a suspended thread.
 */
UINT tx_thread_resume(TX_THREAD *thread_ptr);

/*****************************************************************************************************/
/**
 * @brief The thread running, NULL outside one.
 */
TX_THREAD *tx_thread_identify(VOID);

/*****************************************************************************************************/
/**
 * @brief Take memory from a byte pool.
 */
UINT tx_byte_allocate(TX_BYTE_POOL *pool_ptr, VOID **memory_ptr, ULONG memory_size, ULONG wait_option);

/*****************************************************************************************************/
/**
 * @brief Give memory back to the pool it came from.
 */
UINT tx_byte_release(VOID *memory_ptr);

#endif /* TX_API_H */

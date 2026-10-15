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
#define TX_CALLER_ERROR             ((UINT)0x13)
#define TX_MUTEX_ERROR              ((UINT)0x1C)
#define TX_NOT_AVAILABLE            ((UINT)0x1D)

#define TX_INHERIT                  ((UINT)1)
#define TX_WAIT_FOREVER             ((ULONG)0xFFFFFFFFUL)

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

/*****************************************************************************************************/
/**
 * @brief A mutex. The real one is a control block; this one is enough to count with.
 */
typedef struct
{
    ULONG tx_mutex_id;     /**< Set once created.            */
    UINT  tx_mutex_count;  /**< How many times it is held.   */

} TX_MUTEX;

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

#endif /* TX_API_H */

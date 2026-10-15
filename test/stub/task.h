/**
 * @file        task.h
 * @brief       Host stub for the part of FreeRTOS's task.h osal uses.
 * @version     1.2.0
 *
 * @author      Nima Askari (NimaLTD)
 * @email       nima.askari@gmail.com
 * @github      https://www.github.com/nimaltd
 *
 * @copyright   (c) 2026 Nima Askari (NimaLTD)
 *              SPDX-License-Identifier: Apache-2.0
 *              See LICENSE.md in the project root for the full license text.
 *
 * @note        Only what osal uses, with the real names. The critical section
 *              macros are calls here, implemented by the tests over a fake
 *              CPU that keeps FreeRTOS's count.
 */

#ifndef INC_TASK_H
#define INC_TASK_H

/*
 * ****************************************************************************************************
 * Includes
 * ****************************************************************************************************
*/

#include "FreeRTOS.h"

/*
 * ****************************************************************************************************
 * Macros
 * ****************************************************************************************************
*/

#define taskENTER_CRITICAL()            test_enter_critical()
#define taskEXIT_CRITICAL()             test_exit_critical()
#define taskENTER_CRITICAL_FROM_ISR()   test_enter_critical_from_isr()
#define taskEXIT_CRITICAL_FROM_ISR(x)   test_exit_critical_from_isr(x)

/*
 * ****************************************************************************************************
 * Public function prototypes
 * ****************************************************************************************************
*/

/*****************************************************************************************************/
/**
 * @brief FreeRTOS's critical section for a task: masks, and counts how deep it is.
 */
void test_enter_critical(void);

/*****************************************************************************************************/
/**
 * @brief Leave it: unmasks when the count is back to 0, and never before the RTOS starts.
 */
void test_exit_critical(void);

/*****************************************************************************************************/
/**
 * @brief FreeRTOS's critical section for an interrupt: masks, and returns what was masked before.
 */
UBaseType_t test_enter_critical_from_isr(void);

/*****************************************************************************************************/
/**
 * @brief Leave it, putting back what test_enter_critical_from_isr() returned.
 */
void test_exit_critical_from_isr(UBaseType_t state);

#endif /* INC_TASK_H */

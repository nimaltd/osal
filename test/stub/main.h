/**
 * @file        main.h
 * @brief       Host stub standing in for the CubeMX generated main.h and the HAL.
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
 * @note        This file exists only so the library can be compiled and tested
 *              on a PC. It is not part of the shipped library, and it is never
 *              on the include path of a real STM32 build.
 */

#ifndef MAIN_H
#define MAIN_H

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

#define HAL_MAX_DELAY       0xFFFFFFFFU

/*
 * ****************************************************************************************************
 * Public function prototypes
 * ****************************************************************************************************
*/

/*****************************************************************************************************/
/**
 * @brief Return the current tick, from a clock the tests control.
 */
uint32_t HAL_GetTick(void);

/*****************************************************************************************************/
/**
 * @brief Wait, by moving the tests' clock on.
 */
void HAL_Delay(uint32_t Delay);

#endif /* MAIN_H */

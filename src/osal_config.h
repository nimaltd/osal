/**
 * @file        osal_config.h
 * @brief       Build time configuration for the osal library.
 * @version     1.0.0
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
 * @note        Your copy of this file is yours. The installer creates it once
 *              and never touches it again, so updating the library cannot
 *              overwrite a setting you changed.
 */

#ifndef OSAL_CONFIG_H
#define OSAL_CONFIG_H

/*
 * ****************************************************************************************************
 * Configuration
 * ****************************************************************************************************
*/

/* USER CODE BEGIN OSAL_CONFIGURATION */

/* The RTOS your project runs, if any. One of:
     OSAL_RTOS_NONE      no RTOS, bare metal
     OSAL_RTOS_CMSIS_V1  FreeRTOS through CMSIS-RTOS v1, cmsis_os.h
     OSAL_RTOS_CMSIS_V2  FreeRTOS through CMSIS-RTOS v2, cmsis_os2.h
     OSAL_RTOS_THREADX   ThreadX, tx_api.h
   Every NimaLTD library that uses osal follows this one setting. */
#define OSAL_RTOS           OSAL_RTOS_NONE

/* USER CODE END OSAL_CONFIGURATION */

#endif /* OSAL_CONFIG_H */

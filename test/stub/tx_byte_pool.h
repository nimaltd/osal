/**
 * @file        tx_byte_pool.h
 * @brief       Host stub for the part of ThreadX's tx_byte_pool.h osal uses.
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
 * @note        Only what osal uses, with the real name. Like the real one, it
 *              needs tx_api.h first. The list head is defined by the tests.
 */

#ifndef TX_BYTE_POOL_H
#define TX_BYTE_POOL_H

/*
 * ****************************************************************************************************
 * Global variables
 * ****************************************************************************************************
*/

/* The first byte pool created, NULL while there is none. */
extern TX_BYTE_POOL *_tx_byte_pool_created_ptr;

#endif /* TX_BYTE_POOL_H */

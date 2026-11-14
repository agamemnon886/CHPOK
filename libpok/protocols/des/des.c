/*
 *                               POK header
 * 
 * The following file is a part of the POK project. Any modification should
 * made according to the POK licence. You CANNOT use this file or a part of
 * this file is this part of a file for your own project
 *
 * For more information on the POK licence, please see our LICENCE FILE
 *
 * Please follow the coding guidelines described in doc/CODING_GUIDELINES
 *
 *                                      Copyright (c) 2007-2009 POK team 
 *
 * Created by julien on Mon Dec  7 15:16:48 2009 
 *
 * Copyright (C) 2026 Kirill Brazhnikov <agamemnon886@gmail.com>
 * Modified by Kirill Brazhnikov, 2026-11-09: Doxygen headers for the file
 * and its functions.
 * Modified by Kirill Brazhnikov, 2026-11-14: The function headers no longer
 * have a step for the declaration of a local variable; the @locals table
 * describes it, and an initialized one is set in a step.
 * The modifications are distributed under the POK licence
 * (BSD-2-Clause, see POK-LICENSE).
 */


/**
 * @file
 * DES crypto protocol: a wrapper that interfaces with OpenSSL functions.
 * It sets the crypto key, initialisation string and calls OpenSSL
 * function to crypts data (written by Julien Delange for POK, 2009).
 */

#include <config.h>

#include <protocols/des.h>
#include <string.h>
#include <types.h>
#include "des.h"

#ifdef POK_NEEDS_PROTOCOLS_DES

unsigned char initVector[8] = POK_PROTOCOLS_DES_INIT;

static unsigned char cbc_key [8]= POK_PROTOCOLS_DES_KEY;

int pok_protocols_des_is_init = 0;

/**
 * @funcname `pok_protocols_des_init`
 *
 * @description
 * Initializes the DES protocol (once).
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `pok_protocols_des_is_init` | `int` | Whether the protocol is initialized |
 *
 * @locals
 * None.
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. If `pok_protocols_des_is_init` is equal to `1`:
 *    1. Return.
 * 2. Set `pok_protocols_des_is_init` to `1`.
 *
 * @calls
 * None.
 */
void pok_protocols_des_init ()
{
   if (pok_protocols_des_is_init == 1)
   {
      return;
   }

   pok_protocols_des_is_init = 1;

}

/**
 * @funcname `pok_protocols_des_marshall`
 *
 * @description
 * Encrypts data with DES in CBC mode (fixed key and vector).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `uncrypted_data` | `void *` | [in] The data. |
 * | `uncrypted_size` | `pok_size_t` | [in] Its size. |
 * | `crypted_data` | `void *` | [out] Where the encrypted data is stored. |
 * | `crypted_size` | `size_t *` | [out] Where its size (8) is stored. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `cbc_key` | `unsigned char[8]` | The fixed key |
 * | `initVector` | `unsigned char[8]` | The fixed initialization vector |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ivec` | `DES_cblock` | The initialization vector |
 * | `schedule` | `DES_key_schedule` | The key schedule |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `DES_set_key_checked()` with the arguments the address of `cbc_key`
 *    and the address of `schedule`.
 * 2. Call `memcpy()` with the arguments `ivec`, `initVector` and
 *    `sizeof(initVector)`.
 * 3. Call `DES_ncbc_encrypt()` with the arguments `uncrypted_data`,
 *    `crypted_data`, `uncrypted_size`, the address of `schedule`, the address
 *    of `ivec` and `DES_ENCRYPT`.
 * 4. Set `*crypted_size` to `8`.
 *
 * @calls
 * - `DES_set_key_checked()`
 * - `memcpy()`
 * - `DES_ncbc_encrypt()`
 */
void pok_protocols_des_marshall (void* uncrypted_data, pok_size_t uncrypted_size, void* crypted_data, size_t* crypted_size)
{
   DES_cblock ivec;
   DES_key_schedule schedule;

   DES_set_key_checked (&cbc_key, &schedule);

   memcpy(ivec,initVector,sizeof(initVector));

   DES_ncbc_encrypt(uncrypted_data, crypted_data, uncrypted_size, &schedule, &ivec, DES_ENCRYPT);
   *crypted_size = 8;
}


/**
 * @funcname `pok_protocols_des_unmarshall`
 *
 * @description
 * Decrypts data with DES in CBC mode (fixed key and vector).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `crypted_data` | `void *` | [in] The encrypted data. |
 * | `crypted_size` | `pok_size_t` | [in] Its size. |
 * | `uncrypted_data` | `void *` | [out] Where the data is stored. |
 * | `uncrypted_size` | `size_t *` | [out] Where its size (8) is stored. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `cbc_key` | `unsigned char[8]` | The fixed key |
 * | `initVector` | `unsigned char[8]` | The fixed initialization vector |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ivec` | `DES_cblock` | The initialization vector |
 * | `schedule` | `DES_key_schedule` | The key schedule |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `DES_set_key_checked()` with the arguments the address of `cbc_key`
 *    and the address of `schedule`.
 * 2. Call `memcpy()` with the arguments `ivec`, `initVector` and
 *    `sizeof(initVector)`.
 * 3. Call `DES_ncbc_encrypt()` with the arguments `crypted_data`,
 *    `uncrypted_data`, `crypted_size`, the address of `schedule`, the address
 *    of `ivec` and `DES_DECRYPT`.
 * 4. Set `*uncrypted_size` to `8`.
 *
 * @calls
 * - `DES_set_key_checked()`
 * - `memcpy()`
 * - `DES_ncbc_encrypt()`
 */
void pok_protocols_des_unmarshall (void* crypted_data, pok_size_t crypted_size, void* uncrypted_data, size_t* uncrypted_size)
{
   DES_cblock ivec;
   DES_key_schedule schedule;

   DES_set_key_checked (&cbc_key, &schedule);

   memcpy(ivec,initVector,sizeof(initVector));
   DES_ncbc_encrypt(crypted_data, uncrypted_data, crypted_size, &schedule, &ivec, DES_DECRYPT);

   *uncrypted_size = 8;
}

#endif


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
 * The Caesar marshalling of libpok's protocols: functions to crypt and
 * uncrypt data using the Caesar cipher, every byte shifted by 4 (written
 * by Julien Delange for POK).
 */

#include <config.h>

#include <protocols/caesar.h>
#include <string.h>
#include <types.h>

#ifdef POK_NEEDS_PROTOCOLS_CAESAR

/**
 * @funcname `pok_protocols_caesar_marshall`
 *
 * @description
 * Marshalls data: every byte is shifted by 4 (modulo 255); the crypted data has
 * the same size as the data.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `uncrypted_data` | `void *` | [in] The data. |
 * | `uncrypted_size` | `pok_size_t` | [in] Its size. |
 * | `crypted_data` | `void *` | [out] Where the crypted data is stored. |
 * | `crypted_size` | `size_t *` | [out] Where its size is stored. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `crypted` | `uint8_t *` | The crypted data, as bytes |
 * | `tmp` | `size_t` | The index of the byte |
 * | `uncrypted` | `uint8_t *` | The data, as bytes |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `uncrypted` to `(uint8_t*) uncrypted_data`.
 * 2. Set `crypted` to `(uint8_t*) crypted_data`.
 * 3. Set `tmp` to `0`; then while `tmp` is less than `uncrypted_size`, repeat
 *    (after each pass: increment `tmp`):
 *    1. Set `crypted[tmp]` to `(uncrypted[tmp] + 4) % 255`.
 * 4. Set `*crypted_size` to `uncrypted_size`.
 *
 * @calls
 * None.
 */
void pok_protocols_caesar_marshall (void* uncrypted_data, pok_size_t uncrypted_size, void* crypted_data, size_t* crypted_size)
{
   uint8_t* uncrypted;
   uint8_t* crypted;
   size_t   tmp;

   uncrypted   = (uint8_t*) uncrypted_data;
   crypted     = (uint8_t*) crypted_data;

   for (tmp = 0 ; tmp < uncrypted_size ; tmp++)
   {
         crypted[tmp] = (uncrypted[tmp] + 4) % 255;
   }

   *crypted_size = uncrypted_size;
}


/**
 * @funcname `pok_protocols_caesar_unmarshall`
 *
 * @description
 * Unmarshalls data: every byte is shifted back by 4 (modulo 255); the data has
 * the same size as the crypted data.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `crypted_data` | `void *` | [in] The crypted data. |
 * | `crypted_size` | `pok_size_t` | [in] Its size. |
 * | `uncrypted_data` | `void *` | [out] Where the data is stored. |
 * | `uncrypted_size` | `size_t *` | [out] Where its size is stored. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `crypted` | `uint8_t *` | The crypted data, as bytes |
 * | `tmp` | `size_t` | The index of the byte |
 * | `uncrypted` | `uint8_t *` | The data, as bytes |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `uncrypted` to `(uint8_t*) uncrypted_data`.
 * 2. Set `crypted` to `(uint8_t*) crypted_data`.
 * 3. Set `tmp` to `0`; then while `tmp` is less than `crypted_size`, repeat
 *    (after each pass: increment `tmp`):
 *    1. Set `uncrypted[tmp]` to `(crypted[tmp] - 4) % 255`.
 * 4. Set `*uncrypted_size` to `crypted_size`.
 *
 * @calls
 * None.
 */
void pok_protocols_caesar_unmarshall (void* crypted_data, pok_size_t crypted_size, void* uncrypted_data, size_t* uncrypted_size)
{
   uint8_t* uncrypted;
   uint8_t* crypted;
   size_t   tmp;

   uncrypted   = (uint8_t*) uncrypted_data;
   crypted     = (uint8_t*) crypted_data;

   for (tmp = 0 ; tmp < crypted_size ; tmp++)
   {
         uncrypted[tmp] = (crypted[tmp] - 4) % 255;
   }

   *uncrypted_size = crypted_size;
}

#endif


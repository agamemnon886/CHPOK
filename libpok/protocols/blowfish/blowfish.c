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
 * Created by julien on Fri Dec 11 16:32:31 2009 
 *
 * Copyright (C) 2026 Kirill Brazhnikov <agamemnon886@gmail.com>
 * Modified by Kirill Brazhnikov, 2026-11-09: Doxygen headers for the file
 * and its functions.
 * Modified by Kirill Brazhnikov, 2026-11-11: the Doxygen comments that don't
 * document a function definition are plain comments, so that only the function
 * headers are in the function documentation.
 * Modified by Kirill Brazhnikov, 2026-11-14: The function headers no longer
 * have a step for the declaration of a local variable; the @locals table
 * describes it, and an initialized one is set in a step.
 * The modifications are distributed under the POK licence
 * (BSD-2-Clause, see POK-LICENSE).
 */

/**
 * @file
 * The Blowfish marshalling of libpok's protocols: 8-byte blocks with a
 * fixed key.
 */

/*
 * Remember boys, blowfish encrypts data on 8 bytes !
 */

#include <config.h>

#ifdef POK_NEEDS_PROTOCOLS_BLOWFISH

#include "blowfish.h"

#include <types.h>
#include <libc/string.h>
#include <libc/stdio.h>

#include <protocols/blowfish.h>

static unsigned char cbc_key [16]=POK_BLOWFISH_KEY;

/*
static unsigned char cbc_iv [8]={0xfe,0xdc,0xba,0x98,0x76,0x54,0x32,0x10};
#define MYIV {0xfe,0xdc,0xba,0x98,0x76,0x54,0x32,0x10};
*/



/**
 * @funcname `pok_protocols_blowfish_marshall`
 *
 * @description
 * Encrypts one 8-byte block of data with Blowfish (ECB, fixed key).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `uncrypted_data` | `void *` | [in] The data. |
 * | `uncrypted_size` | `size_t` | [in] Its size (not used: 8 bytes are encrypted). |
 * | `crypted_data` | `void *` | [out] Where the encrypted data is stored. |
 * | `crypted_size` | `size_t *` | [out] Where its size (8) is stored. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `cbc_key` | `unsigned char[16]` | The fixed key |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `key` | `BF_KEY` | The key schedule |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Mark `uncrypted_size` as not used.
 * 2. Call `BF_set_key()` with the arguments the address of `key`, `16` and
 *    `cbc_key`.
 * 3. Call `BF_ecb_encrypt()` with the arguments `uncrypted_data`,
 *    `crypted_data`, the address of `key` and `BF_ENCRYPT`.
 * 4. Set `*crypted_size` to `8`.
 *
 * @calls
 * - `BF_set_key()`
 * - `BF_ecb_encrypt()`
 */
void pok_protocols_blowfish_marshall (void* uncrypted_data, size_t uncrypted_size, void* crypted_data, size_t* crypted_size)
{
   (void) uncrypted_size;
   BF_KEY key;
/*   unsigned char iv[8] = POK_BLOWFISH_INIT; */
   BF_set_key(&key,16,cbc_key);

/*   memcpy(iv,cbc_iv,8); */
   BF_ecb_encrypt(uncrypted_data,crypted_data,&key,BF_ENCRYPT);

   *crypted_size = 8;
}

/**
 * @funcname `pok_protocols_blowfish_unmarshall`
 *
 * @description
 * Decrypts one 8-byte block of data with Blowfish (ECB, fixed key).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `crypted_data` | `void *` | [in] The encrypted data. |
 * | `crypted_size` | `size_t` | [in] Its size (not used: 8 bytes are decrypted). |
 * | `uncrypted_data` | `void *` | [out] Where the data is stored. |
 * | `uncrypted_size` | `size_t *` | [out] Where its size (8) is stored. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `cbc_key` | `unsigned char[16]` | The fixed key |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `key` | `BF_KEY` | The key schedule |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Mark `crypted_size` as not used.
 * 2. Call `BF_set_key()` with the arguments the address of `key`, `16` and
 *    `cbc_key`.
 * 3. Call `BF_ecb_encrypt()` with the arguments `crypted_data`,
 *    `uncrypted_data`, the address of `key` and `BF_DECRYPT`.
 * 4. Set `*uncrypted_size` to `8`.
 *
 * @calls
 * - `BF_set_key()`
 * - `BF_ecb_encrypt()`
 */
void pok_protocols_blowfish_unmarshall (void* crypted_data, size_t crypted_size, void* uncrypted_data, size_t* uncrypted_size)
{
/*   unsigned char iv[8] = POK_BLOWFISH_INIT; */
   (void) crypted_size;

   BF_KEY key;

   BF_set_key(&key,16,cbc_key);

/*   memcpy (iv,cbc_iv,8); */

   BF_ecb_encrypt(crypted_data,uncrypted_data, &key,BF_DECRYPT);

   *uncrypted_size = 8;
}

#endif

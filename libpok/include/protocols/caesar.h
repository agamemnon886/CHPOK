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
 * Created by julien on Mon Nov 30 20:42:30 2009 
 *
 * Copyright (C) 2026 Kirill Brazhnikov <agamemnon886@gmail.com>
 * Modified by Kirill Brazhnikov, 2026-11-11: the Doxygen comments that don't
 * document a function definition are plain comments, so that only the function
 * headers are in the function documentation.
 * Modified by Kirill Brazhnikov, 2026-11-13: the function declarations are
 * hidden from Doxygen (\cond): only the definitions of the functions are
 * documented.
 * The modifications are distributed under the POK licence
 * (BSD-2-Clause, see POK-LICENSE).
 */


#ifndef __LIBPOK_PROTOCOLS_CAESAR_H__
#define __LIBPOK_PROTOCOLS_CAESAR_H__

/**
 * \file    libpok/include/protocols/caesar.h
 * \author  Julien Delange
 * \date    2009
 * \brief   Caesar crypto protocol.
 *
 * This is a very basic crypto protocol that just
 * change the order of bytes in data. There is no
 * public/private key, the algorithm is known
 * by the attacker so that it's a very weak crypto
 * protocol.
 * Interested people can gather more information
 * about this protocol on:
 * http://en.wikipedia.org/wiki/Caesar_cipher
 * 
 * We don't provide an associated marshalling type
 * for the Caesar protocol since the crypted size
 * is the same than the uncrypted size.
 */

#include <config.h>

#include <types.h>

#ifdef POK_NEEDS_PROTOCOLS_CAESAR

/*
 * Function that uncrypts data
 */
/** \cond */
void pok_protocols_caesar_unmarshall (void* crypted_data, pok_size_t crypted_size, void* uncrypted_data, size_t* uncrypted_size);


/*
 * Function that encrypts data
 */
void pok_protocols_caesar_marshall (void* uncrypted_data, pok_size_t uncrypted_size, void* crypted_data, size_t* crypted_size);
/** \endcond */

#endif

#endif

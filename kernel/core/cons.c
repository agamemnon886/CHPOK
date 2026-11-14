/*
 * Institute for System Programming of the Russian Academy of Sciences
 * Copyright (C) 2016 ISPRAS
 * Copyright (C) 2026 Kirill Brazhnikov <agamemnon886@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, Version 3.
 *
 * This program is distributed in the hope # that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License version 3 for more details.
 *
 * Modified by Kirill Brazhnikov, 2026-10-31: built-in integer types (int,
 * unsigned, short, long, ...) are replaced with the fixed-width types
 * from <stdint.h>.
 *
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: __user/__kuser
 * are renamed JET_USER/JET_KUSER; braces around all if/else/loop bodies;
 * essentially Boolean conditions; U suffixes for unsigned constants;
 * discarded results are cast to void; internal linkage for file-local
 * definitions; a read character is stored as 0..255 so that it can't look
 * like the -1 "nothing read" marker.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: MISRA C:2025: unused
 * parameters of the null stream are cast to void; deviations are
 * documented where a guideline can't be followed.
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: the structures and enums are used
 * by their typedef names: jet_console_t, jet_iostream_t.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the enum types are named with _e:
 * pok_ret_e (was pok_ret_t).
 */

/**
 * @file
 * The kernel's consoles: the main console and the debug console (the GDB
 * server's), each a read and a write stream that the board provides (or a
 * null stream), and the old serial helpers built on them.
 */

#include <config.h>

#if defined (POK_NEEDS_CONSOLE) || defined (POK_NEEDS_DEBUG) || defined (POK_NEEDS_INSTRUMENTATION) || defined (POK_NEEDS_COVERAGE_INFOS)

#include <errno.h>
#include <cons.h>
#include <core/uaccess.h>
#include <asp/entries.h>

/*
 * MISRA C:2025 Rule 11.5 deviation (this file): the jet_user_to_kernel*()
 * helpers return the kernel address of user memory as a pointer to void,
 * which is converted to the type of the object the user passed.
 * MISRA C:2025 Rule 18.4 deviation (this file): the part of the string not
 * written yet starts after the bytes already written.
 */

/**
 * @funcname `iostream_init`
 *
 * @description
 * Initializes a stream, if it has an init operation.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `stream` | `jet_iostream_t *` | [in] The stream. |
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. If `stream->init` is not NULL:
 *    1. Call the function `stream->init`.
 *
 * @calls
 * - `stream->init()` (indirect)
 */
static void iostream_init(jet_iostream_t* stream)
{
   if(stream->init != NULL) {
      stream->init();
   }
}

/**
 * @funcname `iostream_write_null`
 *
 * @description
 * The write operation of the null stream: drops the bytes.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `s` | `const char *` | [in] The bytes (not used). |
 * | `length` | `size_t` | [in] The number of bytes. |
 *
 * @return length: every byte counts as written.
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Mark `s` as not used.
 * 2. Return `length`.
 *
 * @calls
 * None.
 */
static size_t iostream_write_null(const char* s, size_t length)
{
   (void) s;
   return length;
}

/**
 * @funcname `iostream_read_null`
 *
 * @description
 * The read operation of the null stream: nothing comes.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `s` | `char *` | [out] Where the bytes would go (not used). |
 * | `length` | `size_t` | [in] The maximum number of bytes (not used). |
 *
 * @return 0.
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Mark `s` as not used.
 * 2. Mark `length` as not used.
 * 3. Return `0`.
 *
 * @calls
 * None.
 */
static size_t iostream_read_null(char* s, size_t length)
{
   (void) s;
   (void) length;
   return 0;
}

jet_iostream_t jet_stream_null =
{
   .write = &iostream_write_null,
   .read = &iostream_read_null
};

jet_console_t jet_console_main;
jet_console_t jet_console_debug;

/**
 * @funcname `iostream_get_default`
 *
 * @description
 * Returns the stream that the board provides, or the null stream if it provides
 * none.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `arch_default` | `jet_iostream_t *` | [in] The board's stream, or NULL. |
 *
 * @return The stream to use.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `jet_stream_null` | `jet_iostream_t` | The null stream |
 *
 * @locals
 * None.
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. If `arch_default` is not NULL:
 *    1. Return `arch_default`.
 * 2. Otherwise:
 *    1. Return the address of `jet_stream_null`.
 *
 * @calls
 * None.
 */
static jet_iostream_t* iostream_get_default(jet_iostream_t* arch_default)
{
   if(arch_default != NULL) {
      return arch_default;
   } else {
      return &jet_stream_null;
   }
}

/**
 * @funcname `jet_console_init_all`
 *
 * @description
 * Sets up the main and debug consoles with the board's streams and initializes
 * the streams. Called by the board at boot.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ja_stream_default_read` | `jet_iostream_t *` | The board's main read stream |
 * | `ja_stream_default_read_debug` | `jet_iostream_t *` | The board's debug read stream |
 * | `ja_stream_default_write` | `jet_iostream_t *` | The board's main write stream |
 * | `ja_stream_default_write_debug` | `jet_iostream_t *` | The board's debug write stream |
 * | `jet_console_debug` | `jet_console_t` | The debug console |
 * | `jet_console_main` | `jet_console_t` | The main console |
 *
 * @locals
 * None.
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `iostream_get_default()` with the argument `ja_stream_default_read`,
 *    and store the result in `jet_console_main.read_stream`.
 * 2. Call `iostream_get_default()` with the argument `ja_stream_default_write`,
 *    and store the result in `jet_console_main.write_stream`.
 * 3. Call `iostream_get_default()` with the argument
 *    `ja_stream_default_read_debug`, and store the result in
 *    `jet_console_debug.read_stream`.
 * 4. Call `iostream_get_default()` with the argument
 *    `ja_stream_default_write_debug`, and store the result in
 *    `jet_console_debug.write_stream`.
 * 5. Call `iostream_init()` with the argument `jet_console_main.read_stream`.
 * 6. Call `iostream_init()` with the argument `jet_console_main.write_stream`.
 * 7. Call `iostream_init()` with the argument `jet_console_debug.read_stream`.
 * 8. Call `iostream_init()` with the argument `jet_console_debug.write_stream`.
 *
 * @calls
 * - `iostream_get_default()`
 * - `iostream_init()`
 */
void jet_console_init_all(void)
{
   jet_console_main.read_stream = iostream_get_default(ja_stream_default_read);
   jet_console_main.write_stream = iostream_get_default(ja_stream_default_write);
   jet_console_debug.read_stream = iostream_get_default(ja_stream_default_read_debug);
   jet_console_debug.write_stream = iostream_get_default(ja_stream_default_write_debug);

   iostream_init(jet_console_main.read_stream);
   iostream_init(jet_console_main.write_stream);
   iostream_init(jet_console_debug.read_stream);
   iostream_init(jet_console_debug.write_stream);
}

/**
 * @funcname `jet_console_read_common`
 *
 * @description
 * Reads from a console what has arrived, up to a length, without waiting for
 * more.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `console` | `jet_console_t *` | [in] The console. |
 * | `s` | `char *` | [out] Where the bytes are stored. |
 * | `length` | `size_t` | [in] The maximum number of bytes. |
 *
 * @return The number of bytes read.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `bytes` | `size_t` | The number of bytes of one read |
 * | `length_rest` | `size_t` | The number of bytes still wanted |
 * | `res` | `size_t` | The number of bytes read so far |
 * | `s_rest` | `char *` | Where the next bytes go |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `res` to `0`.
 * 2. Set `length_rest` to `length`.
 * 3. Set `s_rest` to `s`.
 * 4. While `length_rest` is greater than `0U`:
 *    1. Call the function `console->read_stream->read` with the arguments
 *       `s_rest` and `length_rest`, and store the result in `bytes`.
 *    2. If `bytes` is equal to `0U`:
 *       1. Leave the loop.
 *    3. Add `bytes` to `res`.
 *    4. Subtract `bytes` from `length_rest`.
 *    5. Add `bytes` to `s_rest`.
 * 5. Return `res`.
 *
 * @calls
 * - `console->read_stream->read()` (indirect)
 */
static size_t jet_console_read_common(jet_console_t* console, char* s,
    size_t length)
{
   size_t res = 0;
   size_t length_rest = length;
   char* s_rest = s;

   while(length_rest > 0U)
   {
      size_t bytes = console->read_stream->read(s_rest, length_rest);
      if(bytes == 0U) {
         break;
      }

      res+= bytes;
      length_rest -= bytes;
      s_rest += bytes;
   }

   return res;
}
/**
 * @funcname `jet_console_read`
 *
 * @description
 * Reads from the main console what has arrived.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `s` | `char *` | [out] Where the bytes are stored. |
 * | `length` | `size_t` | [in] The maximum number of bytes. |
 *
 * @return The number of bytes read.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `jet_console_main` | `jet_console_t` | The main console |
 *
 * @locals
 * None.
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `jet_console_read_common()` with the arguments the address of
 *    `jet_console_main`, `s` and `length`, and return its result.
 *
 * @calls
 * - `jet_console_read_common()`
 */
size_t jet_console_read(char* s, size_t length)
{
   return jet_console_read_common(&jet_console_main, s, length);
}
/**
 * @funcname `jet_console_read_debug`
 *
 * @description
 * Reads from the debug console (the GDB server's) what has arrived.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `s` | `char *` | [out] Where the bytes are stored. |
 * | `length` | `size_t` | [in] The maximum number of bytes. |
 *
 * @return The number of bytes read.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `jet_console_debug` | `jet_console_t` | The debug console |
 *
 * @locals
 * None.
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `jet_console_read_common()` with the arguments the address of
 *    `jet_console_debug`, `s` and `length`, and return its result.
 *
 * @calls
 * - `jet_console_read_common()`
 */
size_t jet_console_read_debug(char* s, size_t length)
{
   return jet_console_read_common(&jet_console_debug, s, length);
}

/**
 * @funcname `jet_console_write_common`
 *
 * @description
 * Writes bytes to a console, until all are written.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `console` | `jet_console_t *` | [in] The console. |
 * | `s` | `const char *` | [in] The bytes. |
 * | `length` | `size_t` | [in] The number of bytes. |
 *
 * @return The number of bytes written (length).
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `bytes` | `size_t` | The number of bytes of one write |
 * | `length_rest` | `size_t` | The number of bytes left |
 * | `res` | `size_t` | The number of bytes written so far |
 * | `s_rest` | `const char *` | The next bytes to write |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `res` to `0`.
 * 2. Set `length_rest` to `length`.
 * 3. Set `s_rest` to `s`.
 * 4. While `length_rest` is greater than `0U`:
 *    1. Call the function `console->write_stream->write` with the arguments
 *       `s_rest` and `length_rest`, and store the result in `bytes`.
 *    2. Add `bytes` to `res`.
 *    3. Subtract `bytes` from `length_rest`.
 *    4. Add `bytes` to `s_rest`.
 * 5. Return `res`.
 *
 * @calls
 * - `console->write_stream->write()` (indirect)
 */
static size_t jet_console_write_common(jet_console_t* console,
   const char* s, size_t length)
{
   size_t res = 0;
   size_t length_rest = length;
   const char* s_rest = s;

   while(length_rest > 0U)
   {
      size_t bytes = console->write_stream->write(s_rest, length_rest);

      res+= bytes;
      length_rest -= bytes;
      s_rest += bytes;
   }

   return res;
}

/**
 * @funcname `jet_console_write`
 *
 * @description
 * Writes bytes to the main console.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `s` | `const char *` | [in] The bytes. |
 * | `length` | `size_t` | [in] The number of bytes. |
 *
 * @return The number of bytes written.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `jet_console_main` | `jet_console_t` | The main console |
 *
 * @locals
 * None.
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `jet_console_write_common()` with the arguments the address of
 *    `jet_console_main`, `s` and `length`, and return its result.
 *
 * @calls
 * - `jet_console_write_common()`
 */
size_t jet_console_write(const char* s, size_t length)
{
   return jet_console_write_common(&jet_console_main, s, length);
}

/**
 * @funcname `jet_console_write_debug`
 *
 * @description
 * Writes bytes to the debug console (the GDB server's).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `s` | `const char *` | [in] The bytes. |
 * | `length` | `size_t` | [in] The number of bytes. |
 *
 * @return The number of bytes written.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `jet_console_debug` | `jet_console_t` | The debug console |
 *
 * @locals
 * None.
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `jet_console_write_common()` with the arguments the address of
 *    `jet_console_debug`, `s` and `length`, and return its result.
 *
 * @calls
 * - `jet_console_write_common()`
 */
size_t jet_console_write_debug(const char* s, size_t length)
{
   return jet_console_write_common(&jet_console_debug, s, length);
}


/**
 * @funcname `jet_console_write_user`
 *
 * @description
 * Writes a partition's bytes to the main console (the console write syscall).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `s` | `const char * JET_USER` | [in] Partition address of the bytes. |
 * | `length` | `size_t` | [in] The number of bytes. |
 *
 * @return POK_ERRNO_OK, or POK_ERRNO_EFAULT if the bytes aren't readable by the
 *         partition.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `k_s` | `const char *` | Kernel address of the bytes |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. If `length` is not equal to `0U`:
 *    1. Call `jet_user_to_kernel_ro()` with the arguments `s` and `length`, and
 *       store the result in `k_s`.
 *    2. If `k_s` is NULL:
 *       1. Return `POK_ERRNO_EFAULT`.
 *    3. Call `jet_console_write()` with the arguments `k_s` and `length`.
 * 2. Return `POK_ERRNO_OK`.
 *
 * @calls
 * - `jet_user_to_kernel_ro()`
 * - `jet_console_write()`
 */
pok_ret_e jet_console_write_user(const char* JET_USER s, size_t length)
{
   if(length != 0U) {
      const char* JET_KUSER k_s = jet_user_to_kernel_ro(s, length);
      if(k_s == NULL) {
         return POK_ERRNO_EFAULT;
      }

      (void) jet_console_write(k_s, length);
   }
   return POK_ERRNO_OK;
}


/**
 * @funcname `pok_cons_write`
 *
 * @description
 * Writes bytes to the main console (kept for the old callers).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `s` | `const char *` | [in] The bytes. |
 * | `length` | `size_t` | [in] The number of bytes. |
 *
 * @return TRUE.
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `jet_console_write()` with the arguments `s` and `length`.
 * 2. Return `TRUE`.
 *
 * @calls
 * - `jet_console_write()`
 */
pok_bool_t pok_cons_write (const char* s,
                       size_t length)
{
   (void) jet_console_write(s, length);

   return TRUE;
}
/**
 * @funcname `pok_cons_write_1`
 *
 * @description
 * Writes bytes to the debug console (kept for the old callers).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `s` | `const char *` | [in] The bytes. |
 * | `length` | `size_t` | [in] The number of bytes. |
 *
 * @return TRUE.
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `jet_console_write_debug()` with the arguments `s` and `length`.
 * 2. Return `TRUE`.
 *
 * @calls
 * - `jet_console_write_debug()`
 */
pok_bool_t pok_cons_write_1 (const char* s,
                       size_t length)
{
   (void) jet_console_write_debug(s, length);

   return TRUE;
}

/*
 * Contains character which has been read in 'data_to_read_*'
 *
 * If no character has been read in the last `data_to_read_*` call,
 * or character is already consumed by `read_serial_*`, this is -1.
 */
static int32_t char_to_read_main = -1;
static int32_t char_to_read_debug = -1;

/**
 * @funcname `data_to_read_0`
 *
 * @description
 * Tells whether the main console has a character to read, reading it ahead if
 * needed (the old serial interface).
 *
 * @return 1 if a character is there, 0 otherwise.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `char_to_read_main` | `int32_t` | The character read ahead from the main console, or -1 |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ch` | `char` | The character read |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. If `char_to_read_main` is not equal to `-1`:
 *    1. Return `1`.
 * 2. If `jet_console_read(&ch, 1)` is not equal to `0U`:
 *    1. Set `char_to_read_main` to `(int32_t)(uint8_t)ch`.
 *    2. Return `1`.
 * 3. Return `0`.
 *
 * @calls
 * - `jet_console_read()`
 */
int32_t data_to_read_0(void)
{
   if(char_to_read_main != -1) {
      return 1;
   }

   char ch;
   if(jet_console_read(&ch, 1) != 0U)
   {
      char_to_read_main = (int32_t)(uint8_t)ch;
      return 1;
   }

   return 0;
}

/**
 * @funcname `read_serial_0`
 *
 * @description
 * Reads a character of the main console (the old serial interface).
 *
 * @return The character, or -1 if there is none.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `char_to_read_main` | `int32_t` | The character read ahead from the main console, or -1 |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `res` | `int32_t` | The result |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `data_to_read_0()`, and store the result in `res`.
 * 2. If `res` is not equal to `-1`:
 *    1. Set `res` to `char_to_read_main`.
 *    2. Set `char_to_read_main` to `-1`.
 * 3. Return `res`.
 *
 * @calls
 * - `data_to_read_0()`
 */
int32_t read_serial_0(void)
{
   int32_t res = data_to_read_0();
   if(res != -1)
   {
      res = char_to_read_main;
      char_to_read_main = -1;
   }

   return res;
}

/**
 * @funcname `data_to_read_1`
 *
 * @description
 * Tells whether the debug console has a character to read, reading it ahead if
 * needed (the old serial interface).
 *
 * @return 1 if a character is there, 0 otherwise.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `char_to_read_debug` | `int32_t` | The character read ahead from the debug console, or -1 |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ch` | `char` | The character read |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. If `char_to_read_debug` is not equal to `-1`:
 *    1. Return `1`.
 * 2. If `jet_console_read_debug(&ch, 1)` is not equal to `0U`:
 *    1. Set `char_to_read_debug` to `(int32_t)(uint8_t)ch`.
 *    2. Return `1`.
 * 3. Return `0`.
 *
 * @calls
 * - `jet_console_read_debug()`
 */
int32_t data_to_read_1(void)
{
   if(char_to_read_debug != -1) {
      return 1;
   }

   char ch;
   if(jet_console_read_debug(&ch, 1) != 0U)
   {
      char_to_read_debug = (int32_t)(uint8_t)ch;
      return 1;
   }

   return 0;
}
/**
 * @funcname `read_serial_1`
 *
 * @description
 * Reads a character of the debug console (the old serial interface).
 *
 * @return The character, or -1 if there is none.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `char_to_read_debug` | `int32_t` | The character read ahead from the debug console, or -1 |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `res` | `int32_t` | The result |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `data_to_read_1()`, and store the result in `res`.
 * 2. If `res` is not equal to `-1`:
 *    1. Set `res` to `char_to_read_debug`.
 *    2. Set `char_to_read_debug` to `-1`.
 * 3. Return `res`.
 *
 * @calls
 * - `data_to_read_1()`
 */
int32_t read_serial_1(void)
{
   int32_t res = data_to_read_1();
   if(res != -1)
   {
      res = char_to_read_debug;
      char_to_read_debug = -1;
   }

   return res;
}

#endif /* POK_NEEDS_CONSOLE, POK_NEEDS_DEBUG */

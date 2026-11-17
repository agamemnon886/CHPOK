/*
 * Modified by Kirill Brazhnikov, 2026-10-31: built-in integer types (int,
 * unsigned, short, long, ...) are replaced with the fixed-width types
 * from <stdint.h>.
 *
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: braces around
 * all if/else/loop bodies; essentially Boolean conditions; discarded
 * results are cast to void; internal linkage for file-local definitions;
 * the character read is compared as char.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: MISRA C:2025: the parameter
 * of update_console() no longer hides the buffer buf; the characters read
 * are local to the loop; increments and assignments are separate
 * statements; the line buffer and the history state are defined in
 * readline(); operands of mixed precedence are parenthesized.
 * Long lines are wrapped at 80 columns.
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: the macros and the type are moved
 * to kernel/include/libc.h; struct buff is the typedef buff_t.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: The function headers no longer
 * have a step for the declaration of a local variable; the @locals table
 * describes it, and an initialized one is set in a step.
 *
 * Modified by Kirill Brazhnikov, 2026-11-15: moved from kernel/libc/readline.c
 * (kernel/libc is split up: the GDB server, the monitor and readline are in
 * kernel/debug).
 *
 * Modified by Kirill Brazhnikov, 2026-11-16: high and low level requirements of
 * the functions, traced to the system requirements (docs/SRQs), and the steps
 * of their design linked to the LLRs.
 *
 * Modified by Kirill Brazhnikov, 2026-11-17: low level requirements of one
 * behaviour each; code that compiles to nothing (assertions, (void) x;,
 * empty branches) is left out of the design.
 */

/**
 * @file
 * readline() of the kernel monitor: a line from the main console, with
 * backspace and a history of the last 10 lines (up and down arrows).
 */
#include <libc.h>



/*
 * TRUE if we have more than 10 written commands
 */


static buff_t buffers[BUFMEM];



/**
 * @funcname `buf_transfer`
 *
 * @description
 * Copies the first j + 1 characters of a line into a history buffer (cleared
 * first).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `buf1` | `char *` | [in] The source. |
 * | `buf2` | `char *` | [out] The destination (BUFLEN bytes). |
 * | `j` | `int32_t` | [in] The index of the last character to copy. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `i` | `int32_t` | The index of the character |
 *
 * @hlr
 * - \hlrid{HLR_READLINE_BUF_TRANSFER_01}: `buf_transfer()` shall copy a line
 *   into a buffer of the history, the rest of the buffer being cleared.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_009}
 *
 * @llr
 * - \llrid{LLR_READLINE_BUF_TRANSFER_01}: `buf_transfer()` shall set the
 *   `BUFLEN` bytes at `buf2` to 0 with `memset()` and then copy the bytes 0 to
 *   `j` of `buf1` to `buf2`.
 *   <br>Traces to:
 *   - \hlrref{HLR_READLINE_BUF_TRANSFER_01}
 *
 * @design
 * 1. Call `memset()` with the arguments `buf2`, `0` and `BUFLEN`.
 *    \llrref{LLR_READLINE_BUF_TRANSFER_01}
 * 2. Set `i` to `0`; then while `i` is less than or equal to `j`, repeat (after
 *    each pass: increment `i`): \llrref{LLR_READLINE_BUF_TRANSFER_01}
 *    1. Set `buf2[i]` to `buf1[i]`.
 *
 * @calls
 * - `memset()`
 */
static void buf_transfer(char *buf1,char *buf2,int32_t j){
    (void) memset(buf2, 0, BUFLEN);
    for (int32_t i = 0; i <= j; i++){
        buf2[i] = buf1[i];
    }
}

/**
 * @funcname `upper_number_in_mem`
 *
 * @description
 * Returns the history buffer before a buffer (older), staying at the oldest.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `mem` | `int32_t` | [in] The buffer's number. |
 *
 * @return The older buffer's number.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `buffers` | `buff_t[10]` | The history of lines |
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_READLINE_UPPER_NUMBER_IN_MEM_01}: `upper_number_in_mem()` shall
 *   give the history buffer of the line before a line, staying at the oldest
 *   one.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_009}
 *
 * @llr
 * - \llrid{LLR_READLINE_UPPER_NUMBER_IN_MEM_01}: If `tail` of `buffers[mem]` is
 *   true, `upper_number_in_mem()` shall return `mem`.
 *   <br>Traces to:
 *   - \hlrref{HLR_READLINE_UPPER_NUMBER_IN_MEM_01}
 * - \llrid{LLR_READLINE_UPPER_NUMBER_IN_MEM_02}: If `tail` of `buffers[mem]` is
 *   false and `mem` is greater than 0, `upper_number_in_mem()` shall return
 *   `mem` minus 1.
 *   <br>Traces to:
 *   - \hlrref{HLR_READLINE_UPPER_NUMBER_IN_MEM_01}
 * - \llrid{LLR_READLINE_UPPER_NUMBER_IN_MEM_03}: If `tail` of `buffers[mem]` is
 *   false and `mem` is not greater than 0, `upper_number_in_mem()` shall return
 *   `BUFMEM - 1`.
 *   <br>Traces to:
 *   - \hlrref{HLR_READLINE_UPPER_NUMBER_IN_MEM_01}
 *
 * @design
 * 1. If `buffers[mem].tail` is true:
 *    \llrref{LLR_READLINE_UPPER_NUMBER_IN_MEM_01}
 *    1. Return `mem`.
 * 2. If `mem` is greater than `0`: \llrref{LLR_READLINE_UPPER_NUMBER_IN_MEM_02}
 *    1. Return `mem - 1`.
 * 3. Return `BUFMEM - 1`. \llrref{LLR_READLINE_UPPER_NUMBER_IN_MEM_03}
 *
 * @calls
 * None.
 */
static int32_t upper_number_in_mem(int32_t mem){
    if (buffers[mem].tail) { 
        return mem;
    }
    if (mem > 0) {
        return mem - 1;
    }
    return BUFMEM - 1;
}

/**
 * @funcname `lower_number_in_mem`
 *
 * @description
 * Returns the history buffer after a buffer (newer), staying at the newest.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `mem` | `int32_t` | [in] The buffer's number. |
 *
 * @return The newer buffer's number.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `buffers` | `buff_t[10]` | The history of lines |
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_READLINE_LOWER_NUMBER_IN_MEM_01}: `lower_number_in_mem()` shall
 *   give the history buffer of the line after a line, staying at the newest
 *   one.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_009}
 *
 * @llr
 * - \llrid{LLR_READLINE_LOWER_NUMBER_IN_MEM_01}: If `head` of `buffers[mem]` is
 *   true, `lower_number_in_mem()` shall return `mem`.
 *   <br>Traces to:
 *   - \hlrref{HLR_READLINE_LOWER_NUMBER_IN_MEM_01}
 * - \llrid{LLR_READLINE_LOWER_NUMBER_IN_MEM_02}: If `head` of `buffers[mem]` is
 *   false, `lower_number_in_mem()` shall return `(mem + 1) % BUFMEM`.
 *   <br>Traces to:
 *   - \hlrref{HLR_READLINE_LOWER_NUMBER_IN_MEM_01}
 *
 * @design
 * 1. If `buffers[mem].head` is true:
 *    \llrref{LLR_READLINE_LOWER_NUMBER_IN_MEM_01}
 *    1. Return `mem`.
 * 2. Return `(mem + 1) % BUFMEM`. \llrref{LLR_READLINE_LOWER_NUMBER_IN_MEM_02}
 *
 * @calls
 * None.
 */
static int32_t lower_number_in_mem(int32_t mem){
    if (buffers[mem].head) {
        return mem;
    }
    return (mem + 1) % BUFMEM;
}

/**
 * @funcname `clear_console`
 *
 * @description
 * Clears the current console line: a carriage return and spaces.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `lenght` | `int32_t` | [in] The length of the line. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `j` | `int32_t` | The number of spaces written |
 *
 * @hlr
 * - \hlrid{HLR_READLINE_CLEAR_CONSOLE_01}: `clear_console()` shall blank the
 *   current line of the main console.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_009}
 *
 * @llr
 * - \llrid{LLR_READLINE_CLEAR_CONSOLE_01}: `clear_console()` shall call
 *   `printf()` with `"%%c"` and `'\r'`, and then `printf()` with `" "` as many
 *   times as `lenght` plus 2.
 *   <br>Traces to:
 *   - \hlrref{HLR_READLINE_CLEAR_CONSOLE_01}
 *
 * @design
 * 1. Call `printf()` with the arguments `"%%c"` and `'\r'`.
 *    \llrref{LLR_READLINE_CLEAR_CONSOLE_01}
 * 2. Set `j` to `0`; then while `j` is less than or equal to `(lenght + 1)`,
 *    repeat (after each pass: increment `j`):
 *    \llrref{LLR_READLINE_CLEAR_CONSOLE_01}
 *    1. Call `printf()` with the argument `" "`.
 *
 * @calls
 * - `printf()`
 */
static void clear_console(int32_t lenght){
    (void) printf("%c", '\r'); 
    for (int32_t j = 0; j <= (lenght + 1); j++) {
        (void) printf(" ");
    }
}
/**
 * @funcname `update_console`
 *
 * @description
 * Writes the prompt and a line again on the current console line.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `line` | `const char *` | [in] The line. |
 * | `end` | `int32_t` | [in] The length of the line. |
 * | `prompt` | `const char *` | [in] The prompt. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `j` | `int32_t` | The index of the character |
 *
 * @hlr
 * - \hlrid{HLR_READLINE_UPDATE_CONSOLE_01}: `update_console()` shall show the
 *   prompt and the line being edited again on the current line of the main
 *   console.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_009}
 *
 * @llr
 * - \llrid{LLR_READLINE_UPDATE_CONSOLE_01}: `update_console()` shall call, in
 *   this order, `clear_console()` with `end` plus the result of `strlen()` for
 *   `prompt`, `printf()` with `"%%c"` and `'\r'`, `printf()` with `"%%s"` and
 *   `prompt`, and `printf()` with `"%%c"` and each of the first `end`
 *   characters of `line`.
 *   <br>Traces to:
 *   - \hlrref{HLR_READLINE_UPDATE_CONSOLE_01}
 *
 * @design
 * 1. Call `clear_console()` with the argument `end + (int32_t) strlen(prompt)`.
 *    \llrref{LLR_READLINE_UPDATE_CONSOLE_01}
 * 2. Call `printf()` with the arguments `"%%c"` and `'\r'`.
 *    \llrref{LLR_READLINE_UPDATE_CONSOLE_01}
 * 3. Call `printf()` with the arguments `"%%s"` and `prompt`.
 *    \llrref{LLR_READLINE_UPDATE_CONSOLE_01}
 * 4. Set `j` to `0`; then while `j` is less than `end`, repeat (after each
 *    pass: increment `j`): \llrref{LLR_READLINE_UPDATE_CONSOLE_01}
 *    1. Call `printf()` with the arguments `"%%c"` and `line[j]`.
 *
 * @calls
 * - `clear_console()`
 * - `strlen()`
 * - `printf()`
 */
static void update_console(const char * line,int32_t end,const char * prompt){
    clear_console(end + (int32_t) strlen(prompt));
    (void) printf("%c", '\r');
    (void) printf("%s", prompt);
    for (int32_t j = 0; j < end; j++){
        (void) printf("%c", line[j]);
    }
}



/**
 * @funcname `readline`
 *
 * @description
 * Reads a line from the main console, after a prompt: printable characters are
 * echoed, backspace erases, the arrow keys (ESC [ A, ESC [ B) walk the history,
 * Enter ends the line, which goes into the history.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `prompt` | `const char *` | [in] The prompt, or NULL. |
 *
 * @return The line (in a static buffer), or NULL on a read error.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `buffers` | `buff_t[10]` | The history of lines |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `buf` | `static char[1024]` | The line being edited |
 * | `c` | `int32_t` | The character read |
 * | `ch` | `char` | The character, as char |
 * | `current_buf_number` | `static int32_t` | The history line shown, BUFMEM for the new one, -1 for none |
 * | `i` | `int32_t` | The length of the line |
 * | `more_than_2_in_MEM` | `static pok_bool_t` | Whether the history holds more than 2 lines |
 * | `number_of_last_buf` | `static int32_t` | The newest history line, or -1 |
 * | `prev` | `int32_t` | The previous newest history line |
 *
 * @hlr
 * - \hlrid{HLR_READLINE_READLINE_01}: `readline()` shall read a line from the
 *   main console after a prompt, with echo of the printable characters, erasing
 *   with backspace, the history of the lines typed through the up and down
 *   arrow keys, and an end at a carriage return or line feed, and give the
 *   line.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_009}
 *
 * @llr
 * - \llrid{LLR_READLINE_READLINE_01}: If and only if `prompt` is not NULL,
 *   `readline()` shall first call `printf()` with `"%%s"` and `prompt`.
 *   <br>Traces to:
 *   - \hlrref{HLR_READLINE_READLINE_01}
 * - \llrid{LLR_READLINE_READLINE_02}: If `getchar()` returns a value less than
 *   0, `readline()` shall call `printf()` with `"read error\n"` and return
 *   NULL.
 *   <br>Traces to:
 *   - \hlrref{HLR_READLINE_READLINE_01}
 * - \llrid{LLR_READLINE_READLINE_03}: If the character read is `'\x7f'` or
 *   `'\b'`, `readline()` shall decrement the length of the line if and only if
 *   it is greater than 0, and then call `update_console()` with `buf`, the
 *   length and `prompt`.
 *   <br>Traces to:
 *   - \hlrref{HLR_READLINE_READLINE_01}
 * - \llrid{LLR_READLINE_READLINE_04}: If the character read is `'['`,
 *   `readline()` shall store it at the end of the line in `buf`, call
 *   `printf()` with `"%%c"` and it, and read a second character with
 *   `getchar()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_READLINE_READLINE_01}
 * - \llrid{LLR_READLINE_READLINE_05}: If the second character is none of `'A'`,
 *   `'B'`, `'\r'` and `'\n'`, `readline()` shall keep the `'['` in the line,
 *   add the second character to the line, call `printf()` with `"%%c"` and it,
 *   and read the next character.
 *   <br>Traces to:
 *   - \hlrref{HLR_READLINE_READLINE_01}
 * - \llrid{LLR_READLINE_READLINE_06}: If the second character is `'\r'` or
 *   `'\n'`, `readline()` shall keep the `'['` in the line and end the line.
 *   <br>Traces to:
 *   - \hlrref{HLR_READLINE_READLINE_01}
 * - \llrid{LLR_READLINE_READLINE_07}: If the second character is `'A'` and
 *   `current_buf_number` is `BUFMEM`, `readline()` shall copy the newest line
 *   of the history (`buffers[number_of_last_buf]`, its `size`) to `buf` with
 *   `buf_transfer()`, call `clear_console()` with the length of the line before
 *   plus the result of `strlen()` for `prompt`, set the length to that `size`
 *   plus 1, call `update_console()` with `buf`, the length and `prompt`, and
 *   set `current_buf_number` to `number_of_last_buf`.
 *   <br>Traces to:
 *   - \hlrref{HLR_READLINE_READLINE_01}
 * - \llrid{LLR_READLINE_READLINE_08}: If the second character is `'A'` and
 *   `current_buf_number` is neither -1 nor `BUFMEM`, `readline()` shall do the
 *   same with the line of the history whose number is the result of
 *   `upper_number_in_mem()` for `current_buf_number`, and set
 *   `current_buf_number` to that number.
 *   <br>Traces to:
 *   - \hlrref{HLR_READLINE_READLINE_01}
 * - \llrid{LLR_READLINE_READLINE_09}: If the second character is `'B'` and
 *   `current_buf_number` is neither -1 nor `BUFMEM`, `readline()` shall do the
 *   same with the line of the history whose number is the result of
 *   `lower_number_in_mem()` for `current_buf_number`, and set
 *   `current_buf_number` to that number.
 *   <br>Traces to:
 *   - \hlrref{HLR_READLINE_READLINE_01}
 * - \llrid{LLR_READLINE_READLINE_10}: If the second character is `'A'` and
 *   `current_buf_number` is -1, or the second character is `'B'` and
 *   `current_buf_number` is -1 or `BUFMEM`, `readline()` shall leave the line
 *   and `current_buf_number` as they are.
 *   <br>Traces to:
 *   - \hlrref{HLR_READLINE_READLINE_01}
 * - \llrid{LLR_READLINE_READLINE_11}: After a second character `'A'` or `'B'`,
 *   `readline()` shall call `clear_console()` with the length of the line plus
 *   the result of `strlen()` for `prompt`, and then `update_console()` with
 *   `buf`, the length and `prompt`.
 *   <br>Traces to:
 *   - \hlrref{HLR_READLINE_READLINE_01}
 * - \llrid{LLR_READLINE_READLINE_12}: If the character read is not negative, is
 *   none of `'\x7f'`, `'\b'` and `'['`, is `0x20` or more, and the length of
 *   the line is less than `BUFLEN - 1`, `readline()` shall call `printf()` with
 *   `"%%c"` and the character, and add it to the line.
 *   <br>Traces to:
 *   - \hlrref{HLR_READLINE_READLINE_01}
 * - \llrid{LLR_READLINE_READLINE_13}: If the character read is `'\r'` or
 *   `'\n'`, `readline()` shall end the line.
 *   <br>Traces to:
 *   - \hlrref{HLR_READLINE_READLINE_01}
 * - \llrid{LLR_READLINE_READLINE_14}: If the character read is less than `0x20`
 *   or the line has `BUFLEN - 1` characters, and the character is not negative
 *   and is none of `'\x7f'`, `'\b'`, `'['`, `'\r'` and `'\n'`, `readline()`
 *   shall leave the line as it is and read the next character.
 *   <br>Traces to:
 *   - \hlrref{HLR_READLINE_READLINE_01}
 * - \llrid{LLR_READLINE_READLINE_15}: To end the line, `readline()` shall call
 *   `printf()` with `"\n"`, store `'\0'` after the line in `buf`, copy the line
 *   to the new newest buffer of the history (`buffers[number_of_last_buf]`)
 *   with `buf_transfer()` for the length minus 1, set `size` of that buffer to
 *   the length minus 1 and its `head` to `TRUE`, and return `buf`.
 *   <br>Traces to:
 *   - \hlrref{HLR_READLINE_READLINE_01}
 * - \llrid{LLR_READLINE_READLINE_16}: If `number_of_last_buf` is -1 when the
 *   line ends, `readline()` shall set `number_of_last_buf` to 0,
 *   `current_buf_number` to `BUFMEM` and `tail` of `buffers[0]` to `TRUE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_READLINE_READLINE_01}
 * - \llrid{LLR_READLINE_READLINE_17}: If `number_of_last_buf` is not -1 when
 *   the line ends, `readline()` shall set `number_of_last_buf` to the next
 *   number modulo `BUFMEM`, `current_buf_number` to `BUFMEM` and `head` of the
 *   buffer that was the newest to `FALSE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_READLINE_READLINE_01}
 * - \llrid{LLR_READLINE_READLINE_18}: If and only if `number_of_last_buf` is
 *   not -1 when the line ends and `more_than_2_in_MEM` is true, `readline()`
 *   shall set `tail` of the buffer after the new newest one (modulo `BUFMEM`)
 *   to true if `tail` of the new newest buffer or its own `tail` is true, and
 *   then set `tail` of the buffer that was the newest and of the new newest
 *   buffer to `FALSE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_READLINE_READLINE_01}
 * - \llrid{LLR_READLINE_READLINE_19}: If and only if `number_of_last_buf` is
 *   not -1 when the line ends and its new value is greater than 1, `readline()`
 *   shall set `more_than_2_in_MEM` to `TRUE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_READLINE_READLINE_01}
 *
 * @design
 * 1. If `prompt` is not NULL: \llrref{LLR_READLINE_READLINE_01}
 *    1. Call `printf()` with the arguments `"%%s"` and `prompt`.
 * 2. Set `i` to `0`. \llrref{LLR_READLINE_READLINE_12}
 * 3. While `true` is true: \llrref{LLR_READLINE_READLINE_02},
 *    \llrref{LLR_READLINE_READLINE_03}, \llrref{LLR_READLINE_READLINE_04},
 *    \llrref{LLR_READLINE_READLINE_05}, \llrref{LLR_READLINE_READLINE_06},
 *    \llrref{LLR_READLINE_READLINE_07}, \llrref{LLR_READLINE_READLINE_08},
 *    \llrref{LLR_READLINE_READLINE_09}, \llrref{LLR_READLINE_READLINE_10},
 *    \llrref{LLR_READLINE_READLINE_11}, \llrref{LLR_READLINE_READLINE_12},
 *    \llrref{LLR_READLINE_READLINE_13}, \llrref{LLR_READLINE_READLINE_14},
 *    \llrref{LLR_READLINE_READLINE_15}, \llrref{LLR_READLINE_READLINE_16},
 *    \llrref{LLR_READLINE_READLINE_17}, \llrref{LLR_READLINE_READLINE_18},
 *    \llrref{LLR_READLINE_READLINE_19}
 *    1. Call `getchar()`, and store the result in `c`.
 *    2. Set `ch` to `(char) c`.
 *    3. If `c` is less than `0`: \llrref{LLR_READLINE_READLINE_02}
 *       1. Call `printf()` with the argument `"read error\n"`.
 *       2. Return `NULL`.
 *    4. Otherwise, if `ch` is equal to `'\x7f'` or `ch` is equal to `'\b'`:
 *       \llrref{LLR_READLINE_READLINE_03}
 *       1. If `i` is greater than `0`:
 *          1. Decrement `i`.
 *       2. Call `update_console()` with the arguments `buf`, `i` and `prompt`.
 *    5. Otherwise, if `ch` is equal to `'['`:
 *       \llrref{LLR_READLINE_READLINE_04}, \llrref{LLR_READLINE_READLINE_05},
 *       \llrref{LLR_READLINE_READLINE_06}, \llrref{LLR_READLINE_READLINE_07},
 *       \llrref{LLR_READLINE_READLINE_08}, \llrref{LLR_READLINE_READLINE_09},
 *       \llrref{LLR_READLINE_READLINE_10}, \llrref{LLR_READLINE_READLINE_11}
 *       1. Set `buf[i]` to `ch`. \llrref{LLR_READLINE_READLINE_04}
 *       2. Call `printf()` with the arguments `"%%c"` and `buf[i]`.
 *          \llrref{LLR_READLINE_READLINE_04}
 *       3. Set `ch` to `(char) getchar()`. \llrref{LLR_READLINE_READLINE_04}
 *       4. If `ch` is not equal to `'A'` and `ch` is not equal to `'B'`:
 *          \llrref{LLR_READLINE_READLINE_05}, \llrref{LLR_READLINE_READLINE_06}
 *          1. Increment `i`.
 *          2. If `ch` is equal to `'\r'` or `ch` is equal to `'\n'`:
 *             \llrref{LLR_READLINE_READLINE_06},
 *             \llrref{LLR_READLINE_READLINE_15},
 *             \llrref{LLR_READLINE_READLINE_16},
 *             \llrref{LLR_READLINE_READLINE_17},
 *             \llrref{LLR_READLINE_READLINE_18},
 *             \llrref{LLR_READLINE_READLINE_19}
 *             1. Call `printf()` with the argument `"\n"`.
 *                \llrref{LLR_READLINE_READLINE_15}
 *             2. If `number_of_last_buf` is not equal to `-1`:
 *                \llrref{LLR_READLINE_READLINE_17},
 *                \llrref{LLR_READLINE_READLINE_18},
 *                \llrref{LLR_READLINE_READLINE_19}
 *                1. Set `prev` to `number_of_last_buf`.
 *                2. Set `number_of_last_buf` to
 *                   `(number_of_last_buf + 1) % BUFMEM`.
 *                3. Set `current_buf_number` to `BUFMEM`.
 *                4. If `more_than_2_in_MEM` is true:
 *                   \llrref{LLR_READLINE_READLINE_18}
 *                   1. Set `buffers[(number_of_last_buf + 1) % BUFMEM].tail` to
 *                      `buffers[number_of_last_buf].tail ||`
 *                      `buffers[(number_of_last_buf + 1) % BUFMEM].tail`.
 *                   2. Set `buffers[prev].tail` to `FALSE`.
 *                   3. Set `buffers[number_of_last_buf].tail` to `FALSE`.
 *                5. Set `buffers[prev].head` to `FALSE`.
 *                6. If `number_of_last_buf` is greater than `1`:
 *                   \llrref{LLR_READLINE_READLINE_19}
 *                   1. Set `more_than_2_in_MEM` to `TRUE`.
 *             3. Otherwise: \llrref{LLR_READLINE_READLINE_16}
 *                1. Set `number_of_last_buf` to
 *                   `(number_of_last_buf + 1) % BUFMEM`.
 *                2. Set `current_buf_number` to `BUFMEM`.
 *                3. Set `buffers[0].tail` to `TRUE`.
 *             4. Set `buf[i]` to `'\0'`. \llrref{LLR_READLINE_READLINE_15}
 *             5. Call `buf_transfer()` with the arguments `buf`,
 *                `buffers[number_of_last_buf].buf` and `i - 1`.
 *                \llrref{LLR_READLINE_READLINE_15}
 *             6. Set `buffers[number_of_last_buf].size` to `i - 1`.
 *                \llrref{LLR_READLINE_READLINE_15}
 *             7. Set `buffers[number_of_last_buf].head` to `TRUE`.
 *                \llrref{LLR_READLINE_READLINE_15}
 *             8. Return `buf`. \llrref{LLR_READLINE_READLINE_15}
 *          3. Set `buf[i]` to `ch`. \llrref{LLR_READLINE_READLINE_05}
 *          4. Increment `i`. \llrref{LLR_READLINE_READLINE_05}
 *          5. Call `printf()` with the arguments `"%%c"` and `buf[i - 1]`.
 *             \llrref{LLR_READLINE_READLINE_05}
 *          6. Go on with the next pass of the loop.
 *             \llrref{LLR_READLINE_READLINE_05}
 *       5. If `ch` is equal to `'A'` and `current_buf_number` is not equal to
 *          `-1`: \llrref{LLR_READLINE_READLINE_07},
 *          \llrref{LLR_READLINE_READLINE_08}, \llrref{LLR_READLINE_READLINE_10}
 *          1. If `current_buf_number` is equal to `BUFMEM`:
 *             \llrref{LLR_READLINE_READLINE_07}
 *             1. Call `buf_transfer()` with the arguments
 *                `buffers[number_of_last_buf].buf`, `buf` and
 *                `buffers[number_of_last_buf].size`.
 *             2. Call `clear_console()` with the argument
 *                `i + (int32_t) strlen(prompt)`.
 *             3. Set `i` to `buffers[number_of_last_buf].size + 1`.
 *             4. Call `update_console()` with the arguments `buf`, `i` and
 *                `prompt`.
 *             5. Set `current_buf_number` to `number_of_last_buf`.
 *          2. Otherwise: \llrref{LLR_READLINE_READLINE_08}
 *             1. Call `buf_transfer()` with the arguments
 *                `buffers[upper_number_in_mem(current_buf_number)].buf`, `buf`
 *                and `buffers[upper_number_in_mem(current_buf_number)].size`.
 *             2. Call `clear_console()` with the argument
 *                `i + (int32_t) strlen(prompt)`.
 *             3. Set `i` to
 *                `buffers[upper_number_in_mem(current_buf_number)].size + 1`.
 *             4. Call `update_console()` with the arguments `buf`, `i` and
 *                `prompt`.
 *             5. Call `upper_number_in_mem()` with the argument
 *                `current_buf_number`, and store the result in
 *                `current_buf_number`.
 *       6. Otherwise, if `ch` is equal to `'B'` and `current_buf_number` is not
 *          equal to `-1` and `current_buf_number` is not equal to `BUFMEM`:
 *          \llrref{LLR_READLINE_READLINE_09}, \llrref{LLR_READLINE_READLINE_10}
 *          1. Call `buf_transfer()` with the arguments
 *             `buffers[lower_number_in_mem(current_buf_number)].buf`, `buf` and
 *             `buffers[lower_number_in_mem(current_buf_number)].size`.
 *          2. Call `clear_console()` with the argument
 *             `i + (int32_t) strlen(prompt)`.
 *          3. Set `i` to
 *             `buffers[lower_number_in_mem(current_buf_number)].size + 1`.
 *          4. Call `update_console()` with the arguments `buf`, `i` and
 *             `prompt`.
 *          5. Call `lower_number_in_mem()` with the argument
 *             `current_buf_number`, and store the result in
 *             `current_buf_number`.
 *       7. Call `clear_console()` with the argument
 *          `i + (int32_t) strlen(prompt)`. \llrref{LLR_READLINE_READLINE_11}
 *       8. Call `update_console()` with the arguments `buf`, `i` and `prompt`.
 *          \llrref{LLR_READLINE_READLINE_11}
 *    6. Otherwise, if `c` is greater than or equal to `0x20` and `i` is less
 *       than `(BUFLEN - 1)`: \llrref{LLR_READLINE_READLINE_12}
 *       1. Call `printf()` with the arguments `"%%c"` and `ch`.
 *       2. Set `buf[i]` to `ch`.
 *       3. Increment `i`.
 *    7. Otherwise, if `ch` is equal to `'\r'` or `ch` is equal to `'\n'`:
 *       \llrref{LLR_READLINE_READLINE_13}, \llrref{LLR_READLINE_READLINE_14},
 *       \llrref{LLR_READLINE_READLINE_15}, \llrref{LLR_READLINE_READLINE_16},
 *       \llrref{LLR_READLINE_READLINE_17}, \llrref{LLR_READLINE_READLINE_18},
 *       \llrref{LLR_READLINE_READLINE_19}
 *       1. Call `printf()` with the argument `"\n"`.
 *          \llrref{LLR_READLINE_READLINE_15}
 *       2. If `number_of_last_buf` is not equal to `-1`:
 *          \llrref{LLR_READLINE_READLINE_17},
 *          \llrref{LLR_READLINE_READLINE_18}, \llrref{LLR_READLINE_READLINE_19}
 *          1. Set `prev` to `number_of_last_buf`.
 *          2. Set `number_of_last_buf` to `(number_of_last_buf + 1) % BUFMEM`.
 *          3. Set `current_buf_number` to `BUFMEM`.
 *          4. If `more_than_2_in_MEM` is true:
 *             \llrref{LLR_READLINE_READLINE_18}
 *             1. Set `buffers[(number_of_last_buf + 1) % BUFMEM].tail` to
 *                `buffers[number_of_last_buf].tail ||`
 *                `buffers[(number_of_last_buf + 1) % BUFMEM].tail`.
 *             2. Set `buffers[prev].tail` to `FALSE`.
 *             3. Set `buffers[number_of_last_buf].tail` to `FALSE`.
 *          5. Set `buffers[prev].head` to `FALSE`.
 *          6. If `number_of_last_buf` is greater than `1`:
 *             \llrref{LLR_READLINE_READLINE_19}
 *             1. Set `more_than_2_in_MEM` to `TRUE`.
 *       3. Otherwise: \llrref{LLR_READLINE_READLINE_16}
 *          1. Set `number_of_last_buf` to `(number_of_last_buf + 1) % BUFMEM`.
 *          2. Set `current_buf_number` to `BUFMEM`.
 *          3. Set `buffers[0].tail` to `TRUE`.
 *       4. Set `buf[i]` to `'\0'`. \llrref{LLR_READLINE_READLINE_15}
 *       5. Call `buf_transfer()` with the arguments `buf`,
 *          `buffers[number_of_last_buf].buf` and `i - 1`.
 *          \llrref{LLR_READLINE_READLINE_15}
 *       6. Set `buffers[number_of_last_buf].size` to `i - 1`.
 *          \llrref{LLR_READLINE_READLINE_15}
 *       7. Set `buffers[number_of_last_buf].head` to `TRUE`.
 *          \llrref{LLR_READLINE_READLINE_15}
 *       8. Return `buf`. \llrref{LLR_READLINE_READLINE_15}
 *
 * @calls
 * - `printf()`
 * - `getchar()`
 * - `update_console()`
 * - `buf_transfer()`
 * - `clear_console()`
 * - `strlen()`
 * - `upper_number_in_mem()`
 * - `lower_number_in_mem()`
 */
char *
readline(const char *prompt)
{
    static char buf[BUFLEN];
    static pok_bool_t more_than_2_in_MEM = FALSE;
    // Current number in buffers that we have printed on the screen
    static int32_t current_buf_number = -1;
    // Position in buf memory of the last written buf
    static int32_t number_of_last_buf = -1;

    if (prompt != NULL) {
        (void) printf("%s", prompt);
    }
    int32_t i = 0;
    while (true) {
        int32_t c = getchar();
        char ch = (char) c;
        if (c < 0) {
            (void) printf("read error\n");
            return NULL;
        }  else if ((ch == '\x7f') || (ch == '\b')) {//if c=backspace
            if (i > 0) {
                i--;         
            }
            update_console(buf,i,prompt);
        // if c=up arrow it consist of 2 simbols - '[A' or down arrow -'[B'
        }  else if (ch == '[') {
            buf[i] = ch;               
            (void) printf("%c",buf[i]);
            ch = (char) getchar();
            // Check that second simbol != 'A' or 'B'
            if ((ch != 'A') && (ch != 'B')) {
                    i++;
                    // if c=Enter or end of string
                    if ((ch == '\r') || (ch == '\n')) {
                        (void) printf("\n");
                        if (number_of_last_buf != -1){
                            int32_t prev=number_of_last_buf;
                            number_of_last_buf = (number_of_last_buf + 1) % BUFMEM;
                            current_buf_number = BUFMEM;
                            if (more_than_2_in_MEM){
                                buffers[(number_of_last_buf + 1)
                                        % BUFMEM].tail =
                                    buffers[number_of_last_buf].tail
                                    || buffers[(number_of_last_buf + 1)
                                               % BUFMEM].tail;
                                buffers[prev].tail=FALSE;
                                buffers[number_of_last_buf].tail=FALSE;
                            }
                            buffers[prev].head=FALSE;
                            if (number_of_last_buf > 1) {
                                more_than_2_in_MEM=TRUE;
                            }
                            }else{
                                number_of_last_buf = (number_of_last_buf + 1) % BUFMEM;
                                current_buf_number = BUFMEM;
                                buffers[0].tail=TRUE;
                            }
                        buf[i] = '\0';
                        buf_transfer(buf,buffers[number_of_last_buf].buf,i - 1);
                        buffers[number_of_last_buf].size = i - 1;
                        buffers[number_of_last_buf].head=TRUE;
                        return buf;
                    }
                    buf[i] = ch;
                    i++;
                    (void) printf("%c",buf[i - 1]);
                    continue;
            }
            if ((ch == 'A') && (current_buf_number != -1)){
                if (current_buf_number == BUFMEM){
                    buf_transfer(buffers[number_of_last_buf].buf,buf,buffers[number_of_last_buf].size);
                    clear_console(i + (int32_t) strlen(prompt));
                    i = buffers[number_of_last_buf].size + 1;
                    update_console(buf,i,prompt);
                    current_buf_number = number_of_last_buf;
                }else{    
                    buf_transfer(buffers[upper_number_in_mem(current_buf_number)].buf,buf,buffers[upper_number_in_mem(current_buf_number)].size);
                    clear_console(i + (int32_t) strlen(prompt));
                    i = buffers[upper_number_in_mem(current_buf_number)].size + 1;
                    update_console(buf,i,prompt);
                    current_buf_number = upper_number_in_mem(current_buf_number);
            }
            }else if ((ch == 'B') && (current_buf_number != -1)
                      && (current_buf_number != BUFMEM)){
                buf_transfer(buffers[lower_number_in_mem(current_buf_number)].buf,buf,buffers[lower_number_in_mem(current_buf_number)].size);
                clear_console(i + (int32_t) strlen(prompt));
                i = buffers[lower_number_in_mem(current_buf_number)].size + 1;
                update_console(buf,i,prompt);
                current_buf_number = lower_number_in_mem(current_buf_number);
            } else {
                // No history to move to.
            }
            clear_console(i + (int32_t) strlen(prompt));
            update_console(buf,i,prompt);
        // if c=simbol (' ' or above)
        }  else if ((c >= 0x20) && (i < (BUFLEN - 1))) {
            (void) printf("%c",ch);
            buf[i] = ch;
            i++;
        }  else if ((ch == '\r') || (ch == '\n')) {//if c=Enter or end of string
            (void) printf("\n");
            if (number_of_last_buf != -1){
                int32_t prev=number_of_last_buf;
                number_of_last_buf = (number_of_last_buf + 1) % BUFMEM;
                current_buf_number = BUFMEM;
                if (more_than_2_in_MEM){
                    buffers[(number_of_last_buf + 1) % BUFMEM].tail =
                        buffers[number_of_last_buf].tail
                        || buffers[(number_of_last_buf + 1)
                                   % BUFMEM].tail;
                    buffers[prev].tail=FALSE;
                    buffers[number_of_last_buf].tail=FALSE;
                }
                buffers[prev].head=FALSE;
                if (number_of_last_buf > 1) {
                    more_than_2_in_MEM=TRUE;
                }
            }else{
                number_of_last_buf = (number_of_last_buf + 1) % BUFMEM;
                current_buf_number = BUFMEM;
                buffers[0].tail=TRUE;
            }
            buf[i] = '\0';
            buf_transfer(buf,buffers[number_of_last_buf].buf,i - 1);
            buffers[number_of_last_buf].size = i - 1;
            buffers[number_of_last_buf].head=TRUE;
            return buf;
        } else {
            // Other control characters are ignored.
        }
    }
}


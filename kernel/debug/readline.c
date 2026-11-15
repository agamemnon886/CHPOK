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
 *
 * @llr
 *
 * @design
 * 1. Call `memset()` with the arguments `buf2`, `0` and `BUFLEN`.
 * 2. Set `i` to `0`; then while `i` is less than or equal to `j`, repeat (after
 *    each pass: increment `i`):
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
 *
 * @llr
 *
 * @design
 * 1. If `buffers[mem].tail` is true:
 *    1. Return `mem`.
 * 2. If `mem` is greater than `0`:
 *    1. Return `mem - 1`.
 * 3. Return `BUFMEM - 1`.
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
 *
 * @llr
 *
 * @design
 * 1. If `buffers[mem].head` is true:
 *    1. Return `mem`.
 * 2. Return `(mem + 1) % BUFMEM`.
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
 *
 * @llr
 *
 * @design
 * 1. Call `printf()` with the arguments `"%%c"` and `'\r'`.
 * 2. Set `j` to `0`; then while `j` is less than or equal to `(lenght + 1)`,
 *    repeat (after each pass: increment `j`):
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
 *
 * @llr
 *
 * @design
 * 1. Call `clear_console()` with the argument `end + (int32_t) strlen(prompt)`.
 * 2. Call `printf()` with the arguments `"%%c"` and `'\r'`.
 * 3. Call `printf()` with the arguments `"%%s"` and `prompt`.
 * 4. Set `j` to `0`; then while `j` is less than `end`, repeat (after each
 *    pass: increment `j`):
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
 *
 * @llr
 *
 * @design
 * 1. If `prompt` is not NULL:
 *    1. Call `printf()` with the arguments `"%%s"` and `prompt`.
 * 2. Set `i` to `0`.
 * 3. While `true` is true:
 *    1. Call `getchar()`, and store the result in `c`.
 *    2. Set `ch` to `(char) c`.
 *    3. If `c` is less than `0`:
 *       1. Call `printf()` with the argument `"read error\n"`.
 *       2. Return `NULL`.
 *    4. Otherwise, if `ch` is equal to `'\x7f'` or `ch` is equal to `'\b'`:
 *       1. If `i` is greater than `0`:
 *          1. Decrement `i`.
 *       2. Call `update_console()` with the arguments `buf`, `i` and `prompt`.
 *    5. Otherwise, if `ch` is equal to `'['`:
 *       1. Set `buf[i]` to `ch`.
 *       2. Call `printf()` with the arguments `"%%c"` and `buf[i]`.
 *       3. Set `ch` to `(char) getchar()`.
 *       4. If `ch` is not equal to `'A'` and `ch` is not equal to `'B'`:
 *          1. Increment `i`.
 *          2. If `ch` is equal to `'\r'` or `ch` is equal to `'\n'`:
 *             1. Call `printf()` with the argument `"\n"`.
 *             2. If `number_of_last_buf` is not equal to `-1`:
 *                1. Set `prev` to `number_of_last_buf`.
 *                2. Set `number_of_last_buf` to
 *                   `(number_of_last_buf + 1) % BUFMEM`.
 *                3. Set `current_buf_number` to `BUFMEM`.
 *                4. If `more_than_2_in_MEM` is true:
 *                   1. Set `buffers[(number_of_last_buf + 1) % BUFMEM].tail` to
 *                      `buffers[number_of_last_buf].tail ||`
 *                      `buffers[(number_of_last_buf + 1) % BUFMEM].tail`.
 *                   2. Set `buffers[prev].tail` to `FALSE`.
 *                   3. Set `buffers[number_of_last_buf].tail` to `FALSE`.
 *                5. Set `buffers[prev].head` to `FALSE`.
 *                6. If `number_of_last_buf` is greater than `1`:
 *                   1. Set `more_than_2_in_MEM` to `TRUE`.
 *             3. Otherwise:
 *                1. Set `number_of_last_buf` to
 *                   `(number_of_last_buf + 1) % BUFMEM`.
 *                2. Set `current_buf_number` to `BUFMEM`.
 *                3. Set `buffers[0].tail` to `TRUE`.
 *             4. Set `buf[i]` to `'\0'`.
 *             5. Call `buf_transfer()` with the arguments `buf`,
 *                `buffers[number_of_last_buf].buf` and `i - 1`.
 *             6. Set `buffers[number_of_last_buf].size` to `i - 1`.
 *             7. Set `buffers[number_of_last_buf].head` to `TRUE`.
 *             8. Return `buf`.
 *          3. Set `buf[i]` to `ch`.
 *          4. Increment `i`.
 *          5. Call `printf()` with the arguments `"%%c"` and `buf[i - 1]`.
 *          6. Go on with the next pass of the loop.
 *       5. If `ch` is equal to `'A'` and `current_buf_number` is not equal to
 *          `-1`:
 *          1. If `current_buf_number` is equal to `BUFMEM`:
 *             1. Call `buf_transfer()` with the arguments
 *                `buffers[number_of_last_buf].buf`, `buf` and
 *                `buffers[number_of_last_buf].size`.
 *             2. Call `clear_console()` with the argument
 *                `i + (int32_t) strlen(prompt)`.
 *             3. Set `i` to `buffers[number_of_last_buf].size + 1`.
 *             4. Call `update_console()` with the arguments `buf`, `i` and
 *                `prompt`.
 *             5. Set `current_buf_number` to `number_of_last_buf`.
 *          2. Otherwise:
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
 *       7. Otherwise:
 *          1. Do nothing.
 *       8. Call `clear_console()` with the argument
 *          `i + (int32_t) strlen(prompt)`.
 *       9. Call `update_console()` with the arguments `buf`, `i` and `prompt`.
 *    6. Otherwise, if `c` is greater than or equal to `0x20` and `i` is less
 *       than `(BUFLEN - 1)`:
 *       1. Call `printf()` with the arguments `"%%c"` and `ch`.
 *       2. Set `buf[i]` to `ch`.
 *       3. Increment `i`.
 *    7. Otherwise, if `ch` is equal to `'\r'` or `ch` is equal to `'\n'`:
 *       1. Call `printf()` with the argument `"\n"`.
 *       2. If `number_of_last_buf` is not equal to `-1`:
 *          1. Set `prev` to `number_of_last_buf`.
 *          2. Set `number_of_last_buf` to `(number_of_last_buf + 1) % BUFMEM`.
 *          3. Set `current_buf_number` to `BUFMEM`.
 *          4. If `more_than_2_in_MEM` is true:
 *             1. Set `buffers[(number_of_last_buf + 1) % BUFMEM].tail` to
 *                `buffers[number_of_last_buf].tail ||`
 *                `buffers[(number_of_last_buf + 1) % BUFMEM].tail`.
 *             2. Set `buffers[prev].tail` to `FALSE`.
 *             3. Set `buffers[number_of_last_buf].tail` to `FALSE`.
 *          5. Set `buffers[prev].head` to `FALSE`.
 *          6. If `number_of_last_buf` is greater than `1`:
 *             1. Set `more_than_2_in_MEM` to `TRUE`.
 *       3. Otherwise:
 *          1. Set `number_of_last_buf` to `(number_of_last_buf + 1) % BUFMEM`.
 *          2. Set `current_buf_number` to `BUFMEM`.
 *          3. Set `buffers[0].tail` to `TRUE`.
 *       4. Set `buf[i]` to `'\0'`.
 *       5. Call `buf_transfer()` with the arguments `buf`,
 *          `buffers[number_of_last_buf].buf` and `i - 1`.
 *       6. Set `buffers[number_of_last_buf].size` to `i - 1`.
 *       7. Set `buffers[number_of_last_buf].head` to `TRUE`.
 *       8. Return `buf`.
 *    8. Otherwise:
 *       1. Do nothing.
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


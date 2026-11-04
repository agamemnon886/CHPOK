/* Modified by Kirill Brazhnikov, 2026-10-31: built-in integer types (int,
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
 */
#include <libc.h>


#define BUFMEM 10
#define BUFLEN 1024


struct buff {
    char buf[BUFLEN];
    int32_t size;
    pok_bool_t head;
    pok_bool_t tail;
};

/*
 * TRUE if we have more than 10 written commands
 */


static struct buff buffers[BUFMEM];



static void buf_transfer(char *buf1,char *buf2,int32_t j){
    (void) memset(buf2, 0, BUFLEN);
    for (int32_t i = 0; i <= j; i++){
        buf2[i] = buf1[i];
    }
}

static int32_t upper_number_in_mem(int32_t mem){
    if (buffers[mem].tail) { 
        return mem;
    }
    if (mem > 0) {
        return mem - 1;
    }
    return BUFMEM - 1;
}

static int32_t lower_number_in_mem(int32_t mem){
    if (buffers[mem].head) {
        return mem;
    }
    return (mem + 1) % BUFMEM;
}

static void clear_console(int32_t lenght){
    (void) printf("%c", '\r'); 
    for (int32_t j = 0; j <= (lenght + 1); j++) {
        (void) printf(" ");
    }
}
static void update_console(const char * line,int32_t end,const char * prompt){
    clear_console(end + (int32_t) strlen(prompt));
    (void) printf("%c", '\r');
    (void) printf("%s", prompt);
    for (int32_t j = 0; j < end; j++){
        (void) printf("%c", line[j]);
    }
}



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


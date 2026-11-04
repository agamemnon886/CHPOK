/*
 * Copyright (C) 2026 Kirill Brazhnikov <agamemnon886@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * libc test, partition P1.
 *
 * Checks libpok's C library: string and memory functions, snprintf()
 * formats, ctype and the time functions. The comparisons use local helpers,
 * not the functions under test. The last line is "P1 DONE: <n> failure(s)"
 * (see tests/README).
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
/*
 * MISRA C:2025 Rule 21.10 deviation: the test checks libpok's <time.h>
 * functions, so it has to use them.
 */
#include <time.h>
#include <stdint.h>
#include <stddef.h>
#include <arinc653/types.h>
#include <arinc653/process.h>

static int32_t fails;

static pok_bool_t same(const char *a, const char *b)
{
    size_t i = 0U;

    while ((a[i] != '\0') && (a[i] == b[i])) {
        i++;
    }
    return a[i] == b[i];
}

static void check_int(const char *name, int64_t got, int64_t want)
{
    if (got == want) {
        (void) printf("TEST %s: PASS\n", name);
    } else {
        (void) printf("TEST %s: FAIL (got %lld, want %lld)\n", name,
                      (long long) got, (long long) want);
        fails++;
    }
}

static void check_true(const char *name, pok_bool_t ok)
{
    if (ok) {
        (void) printf("TEST %s: PASS\n", name);
    } else {
        (void) printf("TEST %s: FAIL\n", name);
        fails++;
    }
}

static void check_str(const char *name, const char *got, const char *want)
{
    if ((got != NULL) && same(got, want)) {
        (void) printf("TEST %s: PASS\n", name);
    } else {
        (void) printf("TEST %s: FAIL (got \"%s\", want \"%s\")\n", name,
                      (got != NULL) ? got : "(null)", want);
        fails++;
    }
}

/* -1, 0 or 1 for the sign of v. */
static int64_t sign(int64_t v)
{
    return (v < 0) ? -1 : ((v > 0) ? 1 : 0);
}

static void test_string(void)
{
    char buf[32];
    char tok[] = "a,b;;c";
    const char *s = "hello world";
    const char *high = "a\xe9" "b\xe9";
    const char *t;

    check_int("strlen empty", (int64_t) strlen(""), 0);
    check_int("strlen", (int64_t) strlen("abc"), 3);
    check_int("strcmp equal", strcmp("abc", "abc"), 0);
    check_int("strcmp less", sign(strcmp("abc", "abd")), -1);
    check_int("strcmp greater", sign(strcmp("b", "a")), 1);
    check_int("strcmp prefix", sign(strcmp("ab", "abc")), -1);
    check_int("strncmp equal prefix", strncmp("abcx", "abcy", 3), 0);
    check_int("strncmp differs", sign(strncmp("abcx", "abcy", 4)), -1);
    check_int("strncasecmp equal", strncasecmp("HeLLo", "hello", 5), 0);
    check_int("strncasecmp less", sign(strncasecmp("abc", "ABD", 3)), -1);
    check_int("strncasecmp greater", sign(strncasecmp("b", "A", 1)), 1);
    check_int("strncasecmp length", strncasecmp("Zeta", "zeta1", 4), 0);
    check_int("strncasecmp prefix", sign(strncasecmp("ab", "ABC", 3)), -1);

    (void) memset(buf, (int) 'x', sizeof(buf));
    (void) strncpy(buf, "ab", 5);
    check_true("strncpy pads with NUL",
               (buf[2] == '\0') && (buf[4] == '\0') && (buf[5] == 'x'));
    (void) memset(buf, (int) 'x', sizeof(buf));
    (void) strncpy(buf, "abcdef", 3);
    check_true("strncpy truncates without NUL",
               (buf[2] == 'c') && (buf[3] == 'x'));
    (void) strcpy(buf, "foo");
    (void) strcat(buf, "bar");
    check_str("strcpy+strcat", buf, "foobar");
    (void) strncat(buf, "bazqux", 3);
    check_str("strncat", buf, "foobarbaz");

    check_true("strchr", strchr(s, (int) 'l') == &s[2]);
    check_true("strrchr", strrchr(s, (int) 'o') == &s[7]);
    check_true("strchr NUL", strchr(s, (int) '\0') == &s[11]);
    check_true("strchr missing", strchr(s, (int) 'z') == NULL);
    check_true("strchr byte above 0x7f", strchr(high, 0xe9) == &high[1]);
    check_true("strrchr byte above 0x7f", strrchr(high, 0xe9) == &high[3]);
    check_true("strrchr NUL", strrchr(high, (int) '\0') == &high[4]);
    check_true("strpbrk byte above 0x7f", strpbrk(high, "\xe9") == &high[1]);
    check_true("strstr", strstr(s, "o w") == &s[4]);
    check_true("strstr empty", strstr(s, "") == s);
    check_true("strstr missing", strstr(s, "xyz") == NULL);
    check_int("strspn", (int64_t) strspn("abcde", "abc"), 3);
    check_int("strcspn", (int64_t) strcspn("abcde", "dc"), 2);
    check_true("strpbrk", strpbrk(s, "ow") == &s[4]);

    t = strtok(tok, ",;");
    check_str("strtok 1", t, "a");
    t = strtok(NULL, ",;");
    check_str("strtok 2", t, "b");
    t = strtok(NULL, ",;");
    check_str("strtok 3", t, "c");
    check_true("strtok end", strtok(NULL, ",;") == NULL);
}

static void test_memory(void)
{
    char buf[16];
    const unsigned char bytes[4] = { 1U, 2U, 0xabU, 4U };
    /* "abc", "ab\x01", "ab\x02", "\x80" and "\x01" as bytes. */
    const uint8_t abc1[3] = { 0x61U, 0x62U, 0x63U };
    const uint8_t abc2[3] = { 0x61U, 0x62U, 0x63U };
    const uint8_t ab1[3] = { 0x61U, 0x62U, 0x01U };
    const uint8_t ab2[3] = { 0x61U, 0x62U, 0x02U };
    const uint8_t b80[1] = { 0x80U };
    const uint8_t b01[1] = { 0x01U };

    (void) memcpy(buf, "0123456789", 11);
    (void) memmove(&buf[2], buf, 5);           /* overlapping, forward */
    check_str("memmove forward", buf, "0101234789");
    (void) memcpy(buf, "0123456789", 11);
    (void) memmove(buf, &buf[3], 5);           /* overlapping, backward */
    check_str("memmove backward", buf, "3456756789");
    (void) memset(buf, 0xab, 3);
    check_int("memset value", (unsigned char) buf[1], 0xab);
    check_true("memset length", buf[3] == '6');
    check_int("memcmp equal", memcmp(abc1, abc2, 3), 0);
    check_int("memcmp less", sign(memcmp(ab1, ab2, 3)), -1);
    check_int("memcmp unsigned bytes", sign(memcmp(b80, b01, 1)), 1);
    check_true("memchr", memchr(bytes, 0xab, 4) == &bytes[2]);
    check_true("memchr missing", memchr(bytes, 5, 4) == NULL);
    (void) memcpy(buf, "0123456789", 11);
    (void) memmove(&buf[4], buf, 4);           /* adjacent, no overlap */
    check_str("memmove adjacent", buf, "0123012389");
    (void) memmove(buf, buf, 10);              /* same area */
    check_str("memmove same", buf, "0123012389");
}

static void test_printf(void)
{
    char buf[64];
    int n;

    (void) snprintf(buf, sizeof(buf), "%d", 0);
    check_str("printf %d 0", buf, "0");
    (void) snprintf(buf, sizeof(buf), "%d|%i", -1, 2147483647);
    check_str("printf %d %i", buf, "-1|2147483647");
    (void) snprintf(buf, sizeof(buf), "%d", (int) INT32_MIN);
    check_str("printf %d min", buf, "-2147483648");
    (void) snprintf(buf, sizeof(buf), "%5d|%05d", 42, 42);
    check_str("printf width", buf, "   42|00042");
    (void) snprintf(buf, sizeof(buf), "%u", 4000000000U);
    check_str("printf %u", buf, "4000000000");
    (void) snprintf(buf, sizeof(buf), "%x|%lx", 255U, 0xdeadbeefcafeUL);
    check_str("printf %x %lx", buf, "ff|deadbeefcafe");
    (void) snprintf(buf, sizeof(buf), "%lld|%llu", -123456789012LL,
                    18446744073709551615ULL);
    check_str("printf %lld %llu", buf, "-123456789012|18446744073709551615");
    (void) snprintf(buf, sizeof(buf), "%zu", (size_t) 123);
    check_str("printf %zu", buf, "123");
    (void) snprintf(buf, sizeof(buf), "%s|%.2s|%c|%%", "abc", "abc", 'x');
    check_str("printf %s %.2s %c %%", buf, "abc|ab|x|%");
    /* MISRA C:2025 Rule 11.6 deviation: %p is checked with a known
     * address. */
    (void) snprintf(buf, sizeof(buf), "%p", (void *) 0x1234);
    check_str("printf %p", buf, "0x1234");
    (void) snprintf(buf, sizeof(buf), "%f|%f", 3.25, -0.5);
    check_str("printf %f", buf, "3.250000|-0.500000");
    n = snprintf(buf, 4, "abcdef");
    check_str("snprintf truncates", buf, "abc");
    check_int("snprintf result when truncated", n, 6);
    n = snprintf(buf, sizeof(buf), "abc%d", 12);
    check_int("snprintf result", n, 5);
}

static void test_ctype(void)
{
    check_true("isalpha",
               (isalpha((int) 'a') != 0) && (isalpha((int) '1') == 0));
    check_true("isdigit",
               (isdigit((int) '5') != 0) && (isdigit((int) 'a') == 0));
    check_true("isalnum",
               (isalnum((int) 'Z') != 0) && (isalnum((int) '_') == 0));
    check_true("isspace",
               (isspace((int) ' ') != 0) && (isspace((int) '\n') != 0)
               && (isspace((int) 'x') == 0));
    check_true("isblank",
               (isblank((int) '\t') != 0) && (isblank((int) '\n') == 0));
    check_true("isupper/islower",
               (isupper((int) 'A') != 0) && (islower((int) 'A') == 0)
               && (islower((int) 'a') != 0));
    check_true("isxdigit",
               (isxdigit((int) 'f') != 0) && (isxdigit((int) 'g') == 0));
    check_true("ispunct",
               (ispunct((int) '!') != 0) && (ispunct((int) 'a') == 0));
    check_true("isprint/isgraph",
               (isprint((int) ' ') != 0) && (isgraph((int) ' ') == 0)
               && (isgraph((int) '~') != 0));
    check_true("iscntrl",
               (iscntrl((int) '\n') != 0) && (iscntrl((int) 'a') == 0));
    check_int("toupper", toupper((int) 'a'), (int) 'A');
    check_int("tolower", tolower((int) 'Z'), (int) 'z');
    check_int("toupper non-letter", toupper((int) '1'), (int) '1');
    check_int("isalpha EOF", isalpha(EOF), 0);
}

static void test_time(void)
{
    time_t t0 = 0;
    time_t leap = 951827696;                    /* 2000-02-29 12:34:56 UTC */
    struct tm tm;
    char buf[64];

    (void) gmtime_r(&t0, &tm);
    check_true("gmtime epoch",
               (tm.tm_year == 70) && (tm.tm_mon == 0) && (tm.tm_mday == 1)
               && (tm.tm_wday == 4) && (tm.tm_yday == 0));
    (void) gmtime_r(&leap, &tm);
    check_true("gmtime leap day",
               (tm.tm_year == 100) && (tm.tm_mon == 1) && (tm.tm_mday == 29)
               && (tm.tm_hour == 12) && (tm.tm_min == 34)
               && (tm.tm_sec == 56) && (tm.tm_wday == 2)
               && (tm.tm_yday == 59));
    check_str("asctime", asctime(&tm), "Tue Feb 29 12:34:56 2000\n");
    (void) strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S %a %b %j %%", &tm);
    check_str("strftime basic", buf, "2000-02-29 12:34:56 Tue Feb 060 %");
    (void) strftime(buf, sizeof(buf), "%F %T|%D|%e|%y|%C|%u|%w", &tm);
    check_str("strftime compound", buf,
              "2000-02-29 12:34:56|02/29/00|29|00|20|2|2");
    (void) strftime(buf, sizeof(buf), "%I %p|%A %B", &tm);
    check_str("strftime 12h, names", buf, "12 PM|Tuesday February");
    (void) strftime(buf, sizeof(buf), "%G-W%V-%u|%U|%W", &tm);
    check_str("strftime ISO week", buf, "2000-W09-2|09|09");
    check_int("strftime too small",
              (int64_t) strftime(buf, 4, "%Y-%m-%d", &tm), 0);
    check_int("strftime text too long",
              (int64_t) strftime(buf, 4, "abcdef", &tm), 0);
    check_int("strftime result",
              (int64_t) strftime(buf, sizeof(buf), "%Y!", &tm), 5);
    (void) strftime(buf, sizeof(buf), "%z|%Z|%n|%t", &tm);
    check_str("strftime %z %Z %n %t", buf, "+0000||\n|\t");

    t0 = 1609632309;                    /* 2021-01-03 00:05:09 UTC, Sunday */
    (void) gmtime_r(&t0, &tm);
    (void) strftime(buf, sizeof(buf), "%G-W%V-%u|%g|%U|%W|%e|%I %p", &tm);
    check_str("strftime ISO year before", buf,
              "2020-W53-7|20|01|00| 3|12 AM");
    (void) strftime(buf, sizeof(buf), "%c|%r|%x|%X", &tm);
    check_str("strftime %c %r %x %X", buf,
              "Sun Jan  3 00:05:09 2021|12:05:09 AM|01/03/21|00:05:09");
    t0 = 1230595141;                    /* 2008-12-29 23:59:01 UTC, Monday */
    (void) gmtime_r(&t0, &tm);
    (void) strftime(buf, sizeof(buf), "%G-W%V-%u|%g|%U|%W|%e|%I %p", &tm);
    check_str("strftime ISO year after", buf,
              "2009-W01-1|09|52|52|29|11 PM");
    check_int("difftime", (int64_t) difftime(10, 4), 6);
}

void main(void)
{
    test_string();
    test_memory();
    test_printf();
    test_ctype();
    test_time();
    (void) printf("P1 DONE: %d failure(s)\n", (int) fails);
    STOP_SELF();
}

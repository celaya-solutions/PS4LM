/* libcprobe: calls one locale or wide-char function of the console's libc,
 * the ones llama.cpp's tools import, and prints the result.
 *
 *   scripts/run.sh libcprobe mbtowc
 *   scripts/run.sh libcprobe list
 *
 * One function per run: a function that crashes takes the run with it, and
 * the kernel log (port 3232) shows the exception. docs/PORTING.md has the
 * results.
 *
 * Wide strings are built from wchar_t arrays, not L"" literals: the compiler's
 * PS4 target makes L"" 16-bit while the SDK's C headers say wchar_t is int.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>

static const char *const steps[] = {
    "setlocale", "mb_cur_max", "mbrtowc", "mbtowc", "mbtowc_reset", "mbrlen",
    "mbsrtowcs", "wcrtomb", "wcsrtombs", "localeconv", "iswspace", "strcoll",
    "strxfrm", "wcscoll", "wcsxfrm", "wcstol", "wcstod", "swprintf", "strtod",
};

static long run(const char *step) {
    mbstate_t st;
    wchar_t wb[8];
    char cb[8];
    const char *cs = "Ab";
    static const wchar_t w_ab[] = {'A', 'b', 0}, w_a[] = {'a', 0}, w_b[] = {'b', 0};
    static const wchar_t w_42[] = {'4', '2', 0}, w_45[] = {'4', '.', '5', 0};
    static const wchar_t w_fmt[] = {'%', 'd', 0};
    const wchar_t *ws = w_ab;
    wchar_t *end;

    memset(&st, 0, sizeof st);
    if (!strcmp(step, "setlocale"))
        return setlocale(LC_ALL, NULL) != NULL;
    if (!strcmp(step, "mb_cur_max"))
        return (long)MB_CUR_MAX;
    if (!strcmp(step, "mbrtowc"))
        return (long)mbrtowc(wb, "A", 1, &st);
    if (!strcmp(step, "mbtowc"))
        return mbtowc(wb, "A", 1);
    if (!strcmp(step, "mbtowc_reset"))
        return mbtowc(NULL, NULL, 0);
    if (!strcmp(step, "mbrlen"))
        return (long)mbrlen("A", 1, &st);
    if (!strcmp(step, "mbsrtowcs"))
        return (long)mbsrtowcs(wb, &cs, 8, &st);
    if (!strcmp(step, "wcrtomb"))
        return (long)wcrtomb(cb, 'A', &st);
    if (!strcmp(step, "wcsrtombs"))
        return (long)wcsrtombs(cb, &ws, 8, &st);
    if (!strcmp(step, "localeconv"))
        return *localeconv()->decimal_point;
    if (!strcmp(step, "iswspace"))
        return iswspace(' ');
    if (!strcmp(step, "strcoll"))
        return strcoll("a", "b");
    if (!strcmp(step, "strxfrm"))
        return (long)strxfrm(cb, "ab", sizeof cb);
    if (!strcmp(step, "wcscoll"))
        return wcscoll(w_a, w_b);
    if (!strcmp(step, "wcsxfrm"))
        return (long)wcsxfrm(wb, w_ab, 8);
    if (!strcmp(step, "wcstol"))
        return wcstol(w_42, &end, 10);
    if (!strcmp(step, "wcstod"))
        return (long)(wcstod(w_45, &end) * 10);
    if (!strcmp(step, "swprintf"))
        return swprintf(wb, 8, w_fmt, 7);
    if (!strcmp(step, "strtod"))
        return (long)(strtod("4.5", NULL) * 10);
    return -9999;
}

int main(int argc, char **argv) {
    if (argc < 2 || !strcmp(argv[1], "list")) {
        for (size_t i = 0; i < sizeof steps / sizeof steps[0]; i++)
            printf("%s\n", steps[i]);
        return 0;
    }
    printf("libcprobe: %s ...\n", argv[1]);
    fflush(stdout);
    printf("libcprobe: %s = %ld\n", argv[1], run(argv[1]));
    return 0;
}

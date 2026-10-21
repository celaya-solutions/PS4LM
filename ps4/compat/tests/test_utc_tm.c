/* Mac check for ps4/compat/utc_tm.h against the host's gmtime_r:
 *
 *   cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
 *      -o /tmp/test_utc_tm ps4/compat/tests/test_utc_tm.c && /tmp/test_utc_tm
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#define _DEFAULT_SOURCE
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../utc_tm.h"

static void check(time_t t) {
    struct tm want, got;
    assert(gmtime_r(&t, &want));
    ps4lm_utc_tm(t, &got);
    if (got.tm_year != want.tm_year || got.tm_mon != want.tm_mon || got.tm_mday != want.tm_mday ||
        got.tm_hour != want.tm_hour || got.tm_min != want.tm_min || got.tm_sec != want.tm_sec ||
        got.tm_wday != want.tm_wday || got.tm_yday != want.tm_yday || got.tm_isdst != 0 ||
        got.tm_gmtoff != 0 || strcmp(got.tm_zone, "UTC") != 0) {
        fprintf(stderr, "t=%lld: got %d-%d-%d %d:%d:%d wd%d yd%d, want %d-%d-%d %d:%d:%d wd%d yd%d\n",
                (long long)t, got.tm_year, got.tm_mon, got.tm_mday, got.tm_hour, got.tm_min, got.tm_sec,
                got.tm_wday, got.tm_yday, want.tm_year, want.tm_mon, want.tm_mday, want.tm_hour,
                want.tm_min, want.tm_sec, want.tm_wday, want.tm_yday);
        abort();
    }
}

int main(void) {
    const time_t fixed[] = {0, -1, 1, 59, 86399, 86400, 951782400 /* 2000-02-29 */, 951868800,
                            1792591935 /* a console log name */, 4107542400LL /* 2100-03-01 */,
                            -2208988800LL /* 1900-01-01 */, 253402300799LL /* 9999-12-31 */};
    for (size_t i = 0; i < sizeof fixed / sizeof fixed[0]; i++)
        check(fixed[i]);
    srand(1);
    for (int i = 0; i < 200000; i++)
        check((time_t)(((long long)rand() << 20 ^ rand()) % 20000000000LL) - 5000000000LL);
    puts("test_utc_tm: all PASS");
    return 0;
}

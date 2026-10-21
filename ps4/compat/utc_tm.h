/* Seconds since 1970 to a UTC struct tm, by arithmetic only (no libc calls),
 * for compat.c's localtime. Days to civil date: Howard Hinnant's
 * civil_from_days (public domain). Checked against the host's gmtime_r by
 * ps4/compat/tests/test_utc_tm.c.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef PS4LM_UTC_TM_H
#define PS4LM_UTC_TM_H

#include <string.h>
#include <time.h>

static inline void ps4lm_utc_tm(time_t t, struct tm *tm) {
    long long days = (long long)t / 86400, secs = (long long)t % 86400;
    if (secs < 0) {
        secs += 86400;
        days--;
    }
    memset(tm, 0, sizeof *tm);
    tm->tm_zone = (char *)"UTC"; /* tm_gmtoff stays 0; strftime's %Z reads tm_zone */
    tm->tm_hour = (int)(secs / 3600);
    tm->tm_min = (int)(secs / 60 % 60);
    tm->tm_sec = (int)(secs % 60);
    tm->tm_wday = (int)((days % 7 + 11) % 7); /* 1970-01-01 was a Thursday */

    long long z = days + 719468;
    long long era = (z >= 0 ? z : z - 146096) / 146097;
    long long doe = z - era * 146097;                                     /* [0, 146096] */
    long long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365; /* [0, 399] */
    long long doy = doe - (365 * yoe + yoe / 4 - yoe / 100);               /* [0, 365], from Mar 1 */
    long long mp = (5 * doy + 2) / 153;                                    /* [0, 11], Mar = 0 */
    long long mon = mp < 10 ? mp + 3 : mp - 9;                             /* [1, 12] */
    long long year = yoe + era * 400 + (mon <= 2);
    int leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
    static const short before[12] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};

    tm->tm_mday = (int)(doy - (153 * mp + 2) / 5 + 1);
    tm->tm_mon = (int)(mon - 1);
    tm->tm_year = (int)(year - 1900);
    tm->tm_yday = before[mon - 1] + tm->tm_mday - 1 + (leap && mon > 2);
}

#endif

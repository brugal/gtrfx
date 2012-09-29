#include "common-string.h"
#include <string.h>
#include <time.h>
#include <sys/time.h>

int str_matches (const char *s1, const char *s2)
{
    int n;

    n = strlen(s2);
    if (n != strlen(s1))
        return FALSE;
    if (!strncmp(s1, s2, n))
        return TRUE;

    return FALSE;
}

int str_begins_with (const char *s1, const char *s2)
{
    int n;

    n = strlen(s2);
    if (n > strlen(s1))
        return FALSE;
    if (!strncmp(s1, s2, n))
        return TRUE;

    return FALSE;
}


void parse_filename (char *sin, char *sout, int maxSz)
{
    int i;
    int ln;
    int szOut;
    char c;
    int r;
    struct timeval tv;
    struct tm *tp;

    ln = strlen(sin);
    szOut = 0;
    for (i = 0;  i < ln;  i++) {
        if (szOut >= maxSz)
            break;

        if (sin[i] == '%') {
            if (i >= ln) {
                printf("%s:  warning, stray '%%' in filename\n", __func__);
                break;
            }
            i++;
            c = sin[i];
            switch (c) {
            case 'd':  {  // date stamp
                gettimeofday(&tv, NULL);
                tp = localtime(&tv.tv_sec);
                r = snprintf(sout + szOut, maxSz - szOut, "%04d-%02d-%02d-%02d:%02d:%02d--%d", 1900 + tp->tm_year, 1 + tp->tm_mon, tp->tm_mday, tp->tm_hour, tp->tm_min, tp->tm_sec, (int)tv.tv_usec);
                szOut += r;

                break;
            }
            default:
                // not known, just skip
                printf("%s:  warning unknown option '%%%c'\n", __func__, c);
            }
        } else {
            sout[szOut] = sin[i];
            szOut++;
        }
    }

    if (szOut >= maxSz) {
        sout[maxSz - 1] = '\0';
    } else {
        sout[szOut] = '\0';
    }
    //printf("parsed filename: %s\n", sout);
}

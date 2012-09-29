#ifndef common_string_h_included
#define common_string_h_included

#ifdef __cplusplus
extern "C" {
#endif

#include "common.h"

int str_matches (const char *s1, const char *s2);
int str_begins_with (const char *s1, const char *s2);

/* options:
     %d  replaced with date:   2009-07-24-03:31:16--4234234  (year, month, day, hour, min, sec, nano/micro sec)

*/
void parse_filename (char *sin, char *sout, int maxSz);

#ifdef __cplusplus
}
#endif
#endif  // common_string_h_included

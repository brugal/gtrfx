#ifndef common_h_included
#define common_h_included

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>

#ifndef FALSE
#define FALSE 0
#define TRUE 1
#endif

#define MAX_CHARS 1024

#define xerror(format, ...) { fprintf(stderr, "ERROR %s: %s()  line %d\n", __FILE__, __FUNCTION__, __LINE__); fprintf(stderr, format "\n", ## __VA_ARGS__); exit(1); }

#ifdef __cplusplus
}
#endif
#endif  // common_h_included

#include "android.h"

#include <string.h>

char *strchrnul(const char *s, int c)
{
   char * matched_char = strchr(s, c);
   if (matched_char == NULL) {
       matched_char = (char*) s + strlen(s);
   }
   return matched_char;
}

int MIN(int a, int b) {
    return a < b ? a : b;
}



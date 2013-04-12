/*
 * Android's libc lacks fmemopen, so here is our implementation.
 */

#include <stdio.h>

/**
 * fmemopen expects this function
 * defined in android.c
 */
int MIN(int a, int b);


/*
 * Android's libc does not provide strchrnul, so here
 * is our own implementation. strchrnul behaves like
 * strchr except instead of returning NULL if c is not
 * in s, strchrnul returns a pointer to the \0 byte at
 * the end of s.
 * defined in android.c
 */
char *strchrnul(const char *s, int c);

/* defined in fmemopen.c */
FILE *fmemopen(void *buf, size_t size, const char *mode);

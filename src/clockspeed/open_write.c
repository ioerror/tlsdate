#include <sys/types.h>
#include <fcntl.h>
#include "open.h"

int open_write(fn) char *fn;
{ return open(fn,O_WRONLY | O_NDELAY); }

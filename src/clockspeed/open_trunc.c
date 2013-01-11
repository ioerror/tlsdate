#include <sys/types.h>
#include <fcntl.h>
#include "open.h"

int open_trunc(fn) char *fn;
{ return open(fn,O_WRONLY | O_NDELAY | O_TRUNC | O_CREAT,0644); }

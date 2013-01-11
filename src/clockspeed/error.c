#include <errno.h>
#include "error.h"

/* warning: as coverage improves here, should update error_{str,temp} */

int error_intr =
#ifdef EINTR
EINTR;
#else
-1;
#endif

int error_nomem =
#ifdef ENOMEM
ENOMEM;
#else
-2;
#endif

int error_noent = 
#ifdef ENOENT
ENOENT;
#else
-3;
#endif

int error_txtbsy =
#ifdef ETXTBSY
ETXTBSY;
#else
-4;
#endif

int error_io =
#ifdef EIO
EIO;
#else
-5;
#endif

int error_exist =
#ifdef EEXIST
EEXIST;
#else
-6;
#endif

int error_timeout =
#ifdef ETIMEDOUT
ETIMEDOUT;
#else
-7;
#endif

int error_inprogress =
#ifdef EINPROGRESS
EINPROGRESS;
#else
-8;
#endif

int error_wouldblock =
#ifdef EWOULDBLOCK
EWOULDBLOCK;
#else
-9;
#endif

int error_again =
#ifdef EAGAIN
EAGAIN;
#else
-10;
#endif

int error_pipe =
#ifdef EPIPE
EPIPE;
#else
-11;
#endif

int error_perm =
#ifdef EPERM
EPERM;
#else
-12;
#endif

int error_acces =
#ifdef EACCES
EACCES;
#else
-13;
#endif

#ifndef ERROR_H
#define ERROR_H

extern int errno;

extern int error_intr;
extern int error_nomem;
extern int error_noent;
extern int error_txtbsy;
extern int error_io;
extern int error_exist;
extern int error_timeout;
extern int error_inprogress;
extern int error_wouldblock;
extern int error_again;
extern int error_pipe;
extern int error_perm;
extern int error_acces;

extern char *error_str();
extern int error_temp();

#endif

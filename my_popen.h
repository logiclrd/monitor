#ifndef __MY_POPEN_H__
#define __MY_POPEN_H__

#ifdef __cplusplus
# define EXTERNC extern "C"
#else
# define EXTERNC
#endif

EXTERNC
FILE *
my_popen(const char *command, const char *type);

EXTERNC
int
my_pclose(FILE *iop);

EXTERNC
int
my_pkill(FILE *iop);

#endif // __MY_POPEN_H__


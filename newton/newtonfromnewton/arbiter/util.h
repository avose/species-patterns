#ifndef UTIL_H
#define UTIL_H

extern void Die    (char *e);
extern void Error  (const char *fmt, ...);
extern void Warn   (const char *fmt, ...);
extern void Log    (const char *fmt, ...);

extern long minl   (long a, long b);

extern int  System (const char *fmt, ...);

extern int  Connect(char *name, int port);

extern void Yield  (int sec, int usec);

#endif

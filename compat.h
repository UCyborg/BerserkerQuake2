#ifndef COMPAT_H
#define COMPAT_H

#ifndef _WIN32
typedef unsigned short WORD;
typedef unsigned long DWORD;
typedef unsigned char BYTE;
typedef unsigned char byte;
typedef unsigned short SHORT;

#define _stricmp strcasecmp
#define stricmp strcasecmp

#define min(a, b) (((a) < (b)) ? (a) : (b))
#define max(a, b) (((a) > (b)) ? (a) : (b))
#define _mkdir( x )	mkdir( x, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH )

#define EXPORT __attribute__((visibility("default")))
#define NAKED
#else

#if 0
#define NAKED __declspec(naked)
#else
#define NAKED
#endif

#define EXPORT __declspec(dllexport)
#endif

#endif

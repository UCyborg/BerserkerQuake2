#ifndef COMPAT_H
#define COMPAT_H

#ifndef _WIN32
typedef unsigned short WORD;
typedef unsigned int DWORD;
typedef unsigned char BYTE;
typedef unsigned char byte;
typedef unsigned short SHORT;

#define _stricmp strcasecmp
#define stricmp strcasecmp

#define min(a, b) (((a) < (b)) ? (a) : (b))
#define max(a, b) (((a) > (b)) ? (a) : (b))
#define _mkdir( x )	mkdir( x, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH )

#define socketError errno

#define EXPORT __attribute__((visibility("default")))
#else
#define ioctl ioctlsocket
#define close closesocket

#define socketError WSAGetLastError()

#define EWOULDBLOCK WSAEWOULDBLOCK
#define ECONNRESET WSAECONNRESET
#define EMSGSIZE WSAEMSGSIZE
#define EADDRNOTAVAIL WSAEADDRNOTAVAIL
#define EHOSTUNREACH WSAEHOSTUNREACH
#define ENETUNREACH WSAENETUNREACH
#define EAFNOSUPPORT WSAEAFNOSUPPORT

#define EXPORT __declspec(dllexport)
#endif

#endif

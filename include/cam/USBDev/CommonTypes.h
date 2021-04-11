
#ifdef __linux__

#include <stdint.h>

typedef unsigned int UINT32;
typedef unsigned int UINT;
typedef unsigned char BYTE;
typedef unsigned char UCHAR;
typedef UCHAR* PUCHAR;
typedef int BOOL;
typedef unsigned short USHORT;
typedef unsigned short UINT16;
typedef int64_t INT64;
typedef uint64_t UINT64;
typedef unsigned short WORD;
typedef unsigned long DWORD;
typedef long LONG;
typedef wchar_t WCHAR;
typedef void* HANDLE;

#ifndef FALSE
#define FALSE               0
#endif

#ifndef TRUE
#define TRUE                1
#endif

#ifndef NULL
#define NULL 0
#endif

#include <stdlib.h>

#else
typedef unsigned int UINT32;
typedef unsigned int UINT;
typedef unsigned char BYTE;
typedef unsigned char UCHAR;
typedef UCHAR* PUCHAR;
typedef int BOOL;
typedef unsigned short USHORT;
typedef unsigned short UINT16;
typedef __int64 INT64;
typedef unsigned __int64 UINT64;
typedef unsigned short WORD;
typedef unsigned long DWORD;
typedef long LONG;
typedef wchar_t WCHAR;
typedef void* HANDLE;

#ifndef FALSE
#define FALSE               0
#endif

#ifndef TRUE
#define TRUE                1
#endif

#ifndef NULL
#define NULL 0
#endif

#include <stdlib.h>

#endif

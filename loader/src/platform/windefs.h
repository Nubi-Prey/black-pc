// Platform abstraction and Win32/NT type definitions for Linux x86 native build.
#pragma once

#if defined(_WIN32)
#include <windows.h>
#else

#include <cstdint>
#include <cstddef>
#include <cstring>

#ifndef __stdcall
#define __stdcall __attribute__((stdcall))
#endif

#ifndef __cdecl
#define __cdecl __attribute__((cdecl))
#endif

#ifndef __fastcall
#define __fastcall __attribute__((fastcall))
#endif

#ifndef __declspec
#define __declspec(x)
#endif

typedef uint8_t   BYTE;
typedef uint16_t  WORD;
typedef uint32_t  DWORD;
typedef uint64_t  QWORD;
typedef int32_t   LONG;
typedef uint32_t  ULONG;
typedef int16_t   SHORT;
typedef uint16_t  USHORT;
typedef char      CHAR;
typedef wchar_t   WCHAR;
typedef int32_t   BOOL;
typedef uint8_t   BOOLEAN;
typedef void*     HANDLE;
typedef void*     PVOID;
typedef void*     LPVOID;
typedef const void* LPCVOID;
typedef char*     LPSTR;
typedef const char* LPCSTR;
typedef wchar_t*  LPWSTR;
typedef const wchar_t* LPCWSTR;
typedef uintptr_t ULONG_PTR;
typedef intptr_t  LONG_PTR;
typedef size_t    SIZE_T;
typedef int64_t   LONGLONG;
typedef uint64_t  ULONGLONG;
typedef uint32_t  ACCESS_MASK;
typedef LONG      NTSTATUS;
typedef LONG      HRESULT;
typedef DWORD*    PDWORD;
typedef ULONG*    PULONG;
typedef HANDLE*   PHANDLE;
typedef void*     HMODULE;
typedef void*     HWND;

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef MAX_PATH
#define MAX_PATH 260
#endif

#ifndef S_OK
#define S_OK ((HRESULT)0L)
#endif

#ifndef S_FALSE
#define S_FALSE ((HRESULT)1L)
#endif

#ifndef FAILED
#define FAILED(hr) (((HRESULT)(hr)) < 0)
#endif

#ifndef SUCCEEDED
#define SUCCEEDED(hr) (((HRESULT)(hr)) >= 0)
#endif

typedef union _LARGE_INTEGER {
    struct {
        DWORD LowPart;
        LONG  HighPart;
    };
    struct {
        DWORD LowPart;
        LONG  HighPart;
    } u;
    LONGLONG QuadPart;
} LARGE_INTEGER, *PLARGE_INTEGER;

typedef union _ULARGE_INTEGER {
    struct {
        DWORD LowPart;
        DWORD HighPart;
    };
    struct {
        DWORD LowPart;
        DWORD HighPart;
    } u;
    ULONGLONG QuadPart;
} ULARGE_INTEGER, *PULARGE_INTEGER;

typedef struct _MEMORY_BASIC_INFORMATION {
    PVOID  BaseAddress;
    PVOID  AllocationBase;
    DWORD  AllocationProtect;
    SIZE_T RegionSize;
    DWORD  State;
    DWORD  Protect;
    DWORD  Type;
} MEMORY_BASIC_INFORMATION, *PMEMORY_BASIC_INFORMATION;

#define PAGE_NOACCESS          0x01
#define PAGE_READONLY          0x02
#define PAGE_READWRITE         0x04
#define PAGE_WRITECOPY         0x08
#define PAGE_EXECUTE           0x10
#define PAGE_EXECUTE_READ      0x20
#define PAGE_EXECUTE_READWRITE 0x40
#define PAGE_EXECUTE_WRITECOPY 0x80
#define PAGE_GUARD            0x100
#define PAGE_NOCACHE          0x200
#define PAGE_WRITECOMBINE     0x400

#define MEM_COMMIT             0x1000
#define MEM_RESERVE            0x2000
#define MEM_DECOMMIT           0x4000
#define MEM_RELEASE            0x8000
#define MEM_FREE              0x10000
#define MEM_PRIVATE           0x20000
#define MEM_MAPPED            0x40000
#define MEM_RESET             0x80000
#define MEM_TOP_DOWN         0x100000

#endif // _WIN32

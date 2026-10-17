// Platform abstraction and Win32/NT type definitions for Linux x86 native build.
#pragma once

#if defined(_WIN32)
#include <windows.h>
#else

#if __has_include(<windows_base.h>)
#include <windows_base.h>
#endif

#undef __stdcall
#define __stdcall __attribute__((stdcall))

#ifndef __cdecl
#define __cdecl __attribute__((cdecl))
#endif

#ifndef __fastcall
#define __fastcall __attribute__((fastcall))
#endif

#ifndef __declspec
#define __declspec(x)
#endif

#ifndef INFINITE
#define INFINITE 0xFFFFFFFF
#endif

typedef struct _FILETIME {
    uint32_t dwLowDateTime;
    uint32_t dwHighDateTime;
} FILETIME, *PFILETIME, *LPFILETIME;

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
typedef uintptr_t DWORD_PTR;
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

#ifndef GUID_DEFINED
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
#else
typedef LARGE_INTEGER *PLARGE_INTEGER;
#endif

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

#include <pthread.h>

struct CRITICAL_SECTION {
    pthread_mutex_t mutex;
};

inline void InitializeCriticalSection(CRITICAL_SECTION *cs)
{
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&cs->mutex, &attr);
    pthread_mutexattr_destroy(&attr);
}

inline void DeleteCriticalSection(CRITICAL_SECTION *cs)
{
    pthread_mutex_destroy(&cs->mutex);
}

inline void EnterCriticalSection(CRITICAL_SECTION *cs)
{
    pthread_mutex_lock(&cs->mutex);
}

inline void LeaveCriticalSection(CRITICAL_SECTION *cs)
{
    pthread_mutex_unlock(&cs->mutex);
}

inline BOOL TryEnterCriticalSection(CRITICAL_SECTION *cs)
{
    return pthread_mutex_trylock(&cs->mutex) == 0;
}

#include <sched.h>
#include <unistd.h>
#include <time.h>

#ifndef Sleep
inline void Sleep(DWORD dwMilliseconds) {
    if (dwMilliseconds == 0) {
        sched_yield();
    } else {
        struct timespec req;
        req.tv_sec = dwMilliseconds / 1000;
        req.tv_nsec = (dwMilliseconds % 1000) * 1000000L;
        nanosleep(&req, nullptr);
    }
}
#endif

#ifndef QueryPerformanceFrequency
inline BOOL QueryPerformanceFrequency(LARGE_INTEGER *lpFrequency) {
    if (!lpFrequency) return FALSE;
    lpFrequency->QuadPart = 1000000000LL;
    return TRUE;
}
#endif

#ifndef QueryPerformanceCounter
inline BOOL QueryPerformanceCounter(LARGE_INTEGER *lpPerformanceCount) {
    if (!lpPerformanceCount) return FALSE;
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    lpPerformanceCount->QuadPart = (int64_t)ts.tv_sec * 1000000000LL + ts.tv_nsec;
    return TRUE;
}
#endif

#ifndef GetCurrentThread
inline HANDLE GetCurrentThread() { return (HANDLE)(uintptr_t)-2; }
#endif

#ifndef SetThreadPriority
inline BOOL SetThreadPriority(HANDLE, int) { return TRUE; }
#endif

#ifndef THREAD_PRIORITY_TIME_CRITICAL
#define THREAD_PRIORITY_TIME_CRITICAL 15
#endif

inline void timeBeginPeriod(uint32_t) {}
inline void timeEndPeriod(uint32_t) {}

typedef struct _DEVMODEW {
    WCHAR dmDeviceName[32];
    WORD  dmSpecVersion;
    WORD  dmDriverVersion;
    WORD  dmSize;
    WORD  dmDriverExtra;
    DWORD dmFields;
    short dmOrientation;
    short dmPaperSize;
    short dmPaperLength;
    short dmPaperWidth;
    short dmScale;
    short dmCopies;
    short dmDefaultSource;
    short dmPrintQuality;
    short dmColor;
    short dmDuplex;
    short dmYResolution;
    short dmTTOption;
    short dmCollate;
    WCHAR dmFormName[32];
    WORD  dmLogPixels;
    DWORD dmBitsPerPel;
    DWORD dmPelsWidth;
    DWORD dmPelsHeight;
    DWORD dmDisplayFlags;
    DWORD dmDisplayFrequency;
} DEVMODEW, *PDEVMODEW, *LPDEVMODEW;

inline BOOL EnumDisplaySettingsW(LPCWSTR, DWORD, DEVMODEW*) { return FALSE; }
inline HMODULE LoadLibraryW(LPCWSTR) { return nullptr; }
inline void* GetProcAddress(HMODULE, LPCSTR) { return nullptr; }

inline ULONGLONG GetTickCount64() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (ULONGLONG)ts.tv_sec * 1000ULL + (ts.tv_nsec / 1000000ULL);
}

#include <sys/mman.h>

inline LPVOID VirtualAlloc(LPVOID lpAddress, SIZE_T dwSize, DWORD flAllocationType, DWORD flProtect) {
    (void)flAllocationType;
    int prot = PROT_READ | PROT_WRITE;
    if (flProtect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE)) prot |= PROT_EXEC;
    int flags = MAP_PRIVATE | MAP_ANONYMOUS;
    if (lpAddress) flags |= MAP_FIXED;
    void *p = mmap(lpAddress, dwSize, prot, flags, -1, 0);
    return (p == MAP_FAILED) ? nullptr : p;
}

inline BOOL VirtualFree(LPVOID lpAddress, SIZE_T dwSize, DWORD dwFreeType) {
    (void)dwFreeType;
    return munmap(lpAddress, dwSize) == 0;
}

struct Win32EventObj {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    bool manual_reset;
    bool state;
};

inline HANDLE CreateEventW(void*, BOOL bManualReset, BOOL bInitialState, LPCWSTR) {
    auto *ev = new Win32EventObj();
    pthread_mutex_init(&ev->mutex, nullptr);
    pthread_cond_init(&ev->cond, nullptr);
    ev->manual_reset = bManualReset != 0;
    ev->state = bInitialState != 0;
    return (HANDLE)ev;
}

inline BOOL SetEvent(HANDLE hEvent) {
    if (!hEvent) return FALSE;
    auto *ev = (Win32EventObj*)hEvent;
    pthread_mutex_lock(&ev->mutex);
    ev->state = true;
    if (ev->manual_reset) {
        pthread_cond_broadcast(&ev->cond);
    } else {
        pthread_cond_signal(&ev->cond);
    }
    pthread_mutex_unlock(&ev->mutex);
    return TRUE;
}

inline BOOL ResetEvent(HANDLE hEvent) {
    if (!hEvent) return FALSE;
    auto *ev = (Win32EventObj*)hEvent;
    pthread_mutex_lock(&ev->mutex);
    ev->state = false;
    pthread_mutex_unlock(&ev->mutex);
    return TRUE;
}

inline DWORD WaitForSingleObject(HANDLE hHandle, DWORD dwMilliseconds) {
    if (!hHandle) return 0xFFFFFFFF;
    auto *ev = (Win32EventObj*)hHandle;
    pthread_mutex_lock(&ev->mutex);
    DWORD ret = 0;
    if (!ev->state) {
        if (dwMilliseconds == 0) {
            ret = 0x00000102; // WAIT_TIMEOUT
        } else if (dwMilliseconds == 0xFFFFFFFF) {
            while (!ev->state) {
                pthread_cond_wait(&ev->cond, &ev->mutex);
            }
        } else {
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_sec += dwMilliseconds / 1000;
            ts.tv_nsec += (dwMilliseconds % 1000) * 1000000L;
            if (ts.tv_nsec >= 1000000000L) {
                ts.tv_sec += 1;
                ts.tv_nsec -= 1000000000L;
            }
            while (!ev->state) {
                int r = pthread_cond_timedwait(&ev->cond, &ev->mutex, &ts);
                if (r != 0) {
                    ret = 0x00000102; // WAIT_TIMEOUT
                    break;
                }
            }
        }
    }
    if (ret == 0 && !ev->manual_reset) {
        ev->state = false;
    }
    pthread_mutex_unlock(&ev->mutex);
    return ret;
}

#include <sys/stat.h>
#include <string>
#include <cstdio>

inline BOOL CreateDirectoryW(LPCWSTR lpPathName, void*) {
    if (!lpPathName) return FALSE;
    std::wstring ws(lpPathName);
    std::string s(ws.begin(), ws.end());
    return mkdir(s.c_str(), 0755) == 0;
}

inline int _wfopen_s(FILE **pfile, const wchar_t *filename, const wchar_t *mode) {
    if (!pfile || !filename || !mode) return 22;
    std::wstring wfn(filename), wm(mode);
    std::string sfn(wfn.begin(), wfn.end()), sm(wm.begin(), wm.end());
    *pfile = fopen(sfn.c_str(), sm.c_str());
    return *pfile ? 0 : 2;
}

inline FILE *_wfopen(const wchar_t *filename, const wchar_t *mode) {
    if (!filename || !mode) return nullptr;
    std::wstring wfn(filename), wm(mode);
    std::string sfn(wfn.begin(), wfn.end()), sm(wm.begin(), wm.end());
    return fopen(sfn.c_str(), sm.c_str());
}

struct NT_TIB {
    void *ExceptionList;
    void *StackBase;
    void *StackLimit;
    void *SubSystemTib;
    void *FiberData;
    void *ArbitraryUserPointer;
    struct NT_TIB *Self;
};

inline void* NtCurrentTeb() {
    void *teb = nullptr;
#if defined(__i386__)
    __asm__ volatile ("movl %%fs:0, %0" : "=r" (teb));
#endif
    return teb;
}

#ifndef _AddressOfReturnAddress
#define _AddressOfReturnAddress() __builtin_frame_address(0)
#endif

#endif // _WIN32

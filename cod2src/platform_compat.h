#pragma once

/*
 * platform_compat.h
 *
 * Small Win32/MSVC compatibility layer for non-Windows builds of the
 * reconstructed cod2map source.  Windows builds continue to use native Win32
 * APIs.  POSIX and Emscripten builds provide the subset used by this project.
 */

#ifdef _WIN32
  #include <windows.h>
  #include <io.h>
  #include <direct.h>
  #include <fcntl.h>
  #ifndef __GNUC__
    #include <corecrt_stdio_config.h>
  #endif
#else
  #include <ctype.h>
  #include <dirent.h>
  #include <errno.h>
  #include <fcntl.h>
  #include <fnmatch.h>
  #include <limits.h>
  #ifndef __EMSCRIPTEN__
    #include <pthread.h>
  #endif
  #include <stdint.h>
  #include <stdio.h>
  #include <stdlib.h>
  #include <string.h>
  #include <strings.h>
  #include <sys/stat.h>
  #include <sys/time.h>
  #include <sys/types.h>
  #include <time.h>
  #include <unistd.h>
  #ifndef FNM_CASEFOLD
    #define FNM_CASEFOLD 0
  #endif

  #ifndef MAX_PATH
    #define MAX_PATH 260
  #endif

  typedef void *HANDLE;
  typedef void *HMODULE;
  typedef void *HWND;
  typedef void *HGLOBAL;
  typedef char CHAR;
  typedef unsigned int DWORD;
  typedef int BOOL;
  typedef void *LPVOID;
  typedef DWORD *LPDWORD;
  typedef unsigned short ATOM;
  typedef unsigned long WPARAM;
  typedef long LPARAM;
  typedef long LRESULT;

  #ifndef TRUE
    #define TRUE 1
    #define FALSE 0
  #endif

  #define __stdcall
  #define WINAPI
  #define CALLBACK
  #define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)
  #define INFINITE 0xffffffffUL
  #define _A_SUBDIR 0x10
  #define _MAX_PATH MAX_PATH
  #define _O_BINARY 0
  #define _O_RDONLY O_RDONLY
  #define _O_WRONLY O_WRONLY
  #define _O_RDWR O_RDWR
  #define _O_CREAT O_CREAT
  #define _O_TRUNC O_TRUNC
  #define _O_APPEND O_APPEND
  #define _S_IREAD S_IRUSR
  #define _S_IWRITE S_IWUSR

  #define MB_ICONERROR 0x00000010
  #define MB_ICONEXCLAMATION 0x00000030
  #define MB_OKCANCEL 0x00000001
  #define MB_ICONHAND 0x00000010
  #define MB_TASKMODAL 0x00002000
  #define MB_SETFOREGROUND 0x00010000
  #define IDOK 1
  #define CF_TEXT 1
  #define GMEM_MOVEABLE 0x0002
  #define INVALID_FILE_ATTRIBUTES ((DWORD)-1)
  #define FILE_ATTRIBUTE_READONLY 0x00000001

  #define _mkdir(path) mkdir((path), 0777)
  #define _rmdir(path) rmdir((path))
  #define _stricmp strcasecmp
  #define _strnicmp strncasecmp
  #define _access access
  #define _open open
  #define _read read
  #define _write write
  #define _close close
  #define _lseek lseek
  #define _fileno fileno
  #define _setmode(fd, mode) (0)
  #define _errno() (&errno)
  #define __time32_t time_t
  #define _time32(ptr) time((ptr))
  #define _getcwd getcwd
  #define _lock_file flockfile
  #define _unlock_file funlockfile
  char *_cod2_strdup(const char *s);
  char *_cod2_strlwr(char *s);
  #define _strdup _cod2_strdup
  #define _strlwr _cod2_strlwr
  #define _vsnprintf vsnprintf
  #define _snprintf snprintf
  char *_itoa(int value, char *buffer, int radix);

  #if defined(__EMSCRIPTEN__)
  typedef struct _RTL_CRITICAL_SECTION {
    int unused;
  } CRITICAL_SECTION, *LPCRITICAL_SECTION;
  #else
  typedef struct _RTL_CRITICAL_SECTION {
    pthread_mutex_t mutex;
  } CRITICAL_SECTION, *LPCRITICAL_SECTION;
  #endif

  typedef DWORD (*LPTHREAD_START_ROUTINE)(LPVOID);

  typedef struct _SYSTEM_INFO {
    DWORD dwNumberOfProcessors;
  } SYSTEM_INFO;

  struct _finddata_t {
    unsigned attrib;
    char name[MAX_PATH];
    long size;
  };

  typedef struct _WIN32_FIND_DATAA {
    unsigned dwFileAttributes;
    char cFileName[MAX_PATH];
  } WIN32_FIND_DATAA;

  intptr_t _findfirst(const char *pattern, struct _finddata_t *data);
  int _findnext(intptr_t handle, struct _finddata_t *data);
  int _findclose(intptr_t handle);

  HANDLE FindFirstFileA(const char *pattern, WIN32_FIND_DATAA *data);
  BOOL FindNextFileA(HANDLE handle, WIN32_FIND_DATAA *data);
  BOOL FindClose(HANDLE handle);

  HMODULE GetModuleHandleA(const char *name);
  DWORD GetModuleFileNameA(HMODULE module, char *buffer, DWORD size);
  DWORD GetFullPathNameA(const char *path, DWORD size, char *buffer, char **filePart);
  DWORD GetFileAttributesA(const char *path);
  BOOL SetFileAttributesA(const char *path, DWORD attrs);
  HWND FindWindowA(const char *className, const char *windowName);
  unsigned RegisterWindowMessageA(const char *name);
  unsigned GlobalAddAtomA(const char *name);
  BOOL PostMessageA(HWND hwnd, unsigned msg, WPARAM wparam, LPARAM lparam);
  int MessageBoxA(HWND hwnd, const char *text, const char *caption, unsigned type);
  void ExitProcess(unsigned code);
  void DebugBreak(void);
  HWND GetDesktopWindow(void);
  BOOL OpenClipboard(HWND hwnd);
  BOOL EmptyClipboard(void);
  BOOL CloseClipboard(void);
  HGLOBAL GlobalAlloc(unsigned flags, size_t bytes);
  void *GlobalLock(HGLOBAL mem);
  BOOL GlobalUnlock(HGLOBAL mem);
  HANDLE SetClipboardData(unsigned format, HANDLE mem);

  void InitializeCriticalSection(CRITICAL_SECTION *section);
  void DeleteCriticalSection(CRITICAL_SECTION *section);
  void EnterCriticalSection(CRITICAL_SECTION *section);
  void LeaveCriticalSection(CRITICAL_SECTION *section);
  HANDLE CreateThread(void *attr, size_t stackSize, LPTHREAD_START_ROUTINE start, LPVOID param, DWORD flags, LPDWORD threadId);
  DWORD WaitForSingleObject(HANDLE handle, DWORD milliseconds);
  void GetSystemInfo(SYSTEM_INFO *info);

  void *LoadLibraryA(const char *name);
  void *GetProcAddress(void *module, const char *name);
#endif

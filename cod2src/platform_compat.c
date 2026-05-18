#include "cod2map.h"

#ifndef _WIN32

typedef struct find_handle_s {
  DIR *dir;
  char dirpath[MAX_OS_PATH_SHORT];
  char pattern[MAX_OS_PATH_SHORT];
} find_handle_t;

static void normalize_slashes(char *s)
{
  while (*s) {
    if (*s == '\\') *s = '/';
    ++s;
  }
}

static void split_pattern(const char *pattern, char *dir, size_t dirSize, char *wild, size_t wildSize)
{
  char tmp[MAX_OS_PATH_SHORT];
  char *slash;

  if (!pattern || !*pattern) pattern = "*";
  snprintf(tmp, sizeof(tmp), "%s", pattern);
  normalize_slashes(tmp);

  slash = strrchr(tmp, '/');
  if (slash) {
    *slash = '\0';
    snprintf(dir, dirSize, "%s", tmp[0] ? tmp : ".");
    snprintf(wild, wildSize, "%s", slash + 1);
  } else {
    snprintf(dir, dirSize, ".");
    snprintf(wild, wildSize, "%s", tmp);
  }

  if (!*wild) snprintf(wild, wildSize, "*");
}

static void lowercase_copy(char *dst, size_t dstSize, const char *src)
{
  size_t i;

  if (!dstSize)
    return;

  for (i = 0; i + 1 < dstSize && src && src[i]; i++)
    dst[i] = (char)tolower((unsigned char)src[i]);
  dst[i] = '\0';
}

static int fnmatch_case_insensitive(const char *pattern, const char *name)
{
#if defined(FNM_CASEFOLD) && FNM_CASEFOLD
  return fnmatch(pattern, name, FNM_CASEFOLD);
#else
  char lowerPattern[MAX_OS_PATH_SHORT];
  char lowerName[MAX_OS_PATH_SHORT];

  lowercase_copy(lowerPattern, sizeof(lowerPattern), pattern);
  lowercase_copy(lowerName, sizeof(lowerName), name);
  return fnmatch(lowerPattern, lowerName, 0);
#endif
}

static int fill_find_data(find_handle_t *fh, struct _finddata_t *data)
{
  struct dirent *de;
  char full[MAX_OS_PATH_SHORT];
  struct stat st;

  while ((de = readdir(fh->dir)) != NULL) {
    if (fnmatch_case_insensitive(fh->pattern, de->d_name) != 0)
      continue;

    snprintf(full, sizeof(full), "%s/%s", fh->dirpath, de->d_name);
    if (stat(full, &st) != 0)
      memset(&st, 0, sizeof(st));

    data->attrib = S_ISDIR(st.st_mode) ? _A_SUBDIR : 0;
    data->size = (long)st.st_size;
    snprintf(data->name, sizeof(data->name), "%s", de->d_name);
    return 0;
  }

  return -1;
}

intptr_t _findfirst(const char *pattern, struct _finddata_t *data)
{
  find_handle_t *fh;

  fh = (find_handle_t *)calloc(1, sizeof(*fh));
  if (!fh)
    return -1;

  split_pattern(pattern, fh->dirpath, sizeof(fh->dirpath), fh->pattern, sizeof(fh->pattern));
  fh->dir = opendir(fh->dirpath);
  if (!fh->dir) {
    free(fh);
    return -1;
  }

  if (fill_find_data(fh, data) != 0) {
    closedir(fh->dir);
    free(fh);
    return -1;
  }

  return (intptr_t)fh;
}

int _findnext(intptr_t handle, struct _finddata_t *data)
{
  find_handle_t *fh = (find_handle_t *)(intptr_t)handle;
  if (!fh || !fh->dir)
    return -1;
  return fill_find_data(fh, data);
}

int _findclose(intptr_t handle)
{
  find_handle_t *fh = (find_handle_t *)(intptr_t)handle;
  if (!fh)
    return -1;
  if (fh->dir)
    closedir(fh->dir);
  free(fh);
  return 0;
}

HANDLE FindFirstFileA(const char *pattern, WIN32_FIND_DATAA *data)
{
  struct _finddata_t fd;
  intptr_t h = _findfirst(pattern, &fd);
  if (h == -1)
    return INVALID_HANDLE_VALUE;
  data->dwFileAttributes = fd.attrib;
  snprintf(data->cFileName, sizeof(data->cFileName), "%s", fd.name);
  return (HANDLE)h;
}

BOOL FindNextFileA(HANDLE handle, WIN32_FIND_DATAA *data)
{
  struct _finddata_t fd;
  if (_findnext((intptr_t)handle, &fd) != 0)
    return FALSE;
  data->dwFileAttributes = fd.attrib;
  snprintf(data->cFileName, sizeof(data->cFileName), "%s", fd.name);
  return TRUE;
}

BOOL FindClose(HANDLE handle)
{
  return _findclose((intptr_t)handle) == 0;
}




char *_cod2_strlwr(char *s)
{
  char *p = s;
  if (!s) return NULL;
  while (*p) { *p = (char)tolower((unsigned char)*p); ++p; }
  return s;
}

char *_cod2_strdup(const char *s)
{
  size_t len;
  char *out;
  if (!s) return NULL;
  len = strlen(s) + 1;
  out = (char *)malloc(len);
  if (out) memcpy(out, s, len);
  return out;
}

char *_itoa(int value, char *buffer, int radix)
{
  if (!buffer) return NULL;
  if (radix == 16) snprintf(buffer, 34, "%x", value);
  else if (radix == 8) snprintf(buffer, 34, "%o", value);
  else snprintf(buffer, 34, "%d", value);
  return buffer;
}


DWORD GetFullPathNameA(const char *path, DWORD size, char *buffer, char **filePart)
{
  char tmp[MAX_OS_PATH_SHORT];
  char *slash;

  if (!path || !buffer || !size)
    return 0;

  if (path[0] == '/') {
    snprintf(tmp, sizeof(tmp), "%s", path);
  } else {
    char cwd[MAX_OS_PATH_SHORT];
    if (!getcwd(cwd, sizeof(cwd)))
      snprintf(cwd, sizeof(cwd), ".");
    snprintf(tmp, sizeof(tmp), "%s/%s", cwd, path);
  }
  normalize_slashes(tmp);
  snprintf(buffer, size, "%s", tmp);
  slash = strrchr(buffer, '/');
  if (filePart) *filePart = slash ? slash + 1 : buffer;
  return (DWORD)strlen(buffer);
}

DWORD GetFileAttributesA(const char *path)
{
  struct stat st;
  if (!path || stat(path, &st) != 0)
    return INVALID_FILE_ATTRIBUTES;
  return (access(path, W_OK) == 0) ? 0 : FILE_ATTRIBUTE_READONLY;
}

BOOL SetFileAttributesA(const char *path, DWORD attrs)
{
  struct stat st;
  mode_t mode;
  if (!path || stat(path, &st) != 0)
    return FALSE;
  mode = st.st_mode;
  if (attrs & FILE_ATTRIBUTE_READONLY)
    mode &= ~(S_IWUSR | S_IWGRP | S_IWOTH);
  else
    mode |= S_IWUSR;
  return chmod(path, mode) == 0;
}

HMODULE GetModuleHandleA(const char *name)
{
  (void)name;
  return NULL;
}

DWORD GetModuleFileNameA(HMODULE module, char *buffer, DWORD size)
{
  (void)module;
  if (!buffer || !size)
    return 0;
#if defined(__linux__)
  {
    ssize_t len = readlink("/proc/self/exe", buffer, size - 1);
    if (len > 0) {
      buffer[len] = '\0';
      return (DWORD)len;
    }
  }
#endif
  snprintf(buffer, size, "cod2map");
  return (DWORD)strlen(buffer);
}

HWND FindWindowA(const char *className, const char *windowName)
{
  (void)className; (void)windowName; return NULL;
}

unsigned RegisterWindowMessageA(const char *name)
{
  (void)name; return 0;
}

unsigned GlobalAddAtomA(const char *name)
{
  (void)name; return 0;
}

BOOL PostMessageA(HWND hwnd, unsigned msg, WPARAM wparam, LPARAM lparam)
{
  (void)hwnd; (void)msg; (void)wparam; (void)lparam; return FALSE;
}

int MessageBoxA(HWND hwnd, const char *text, const char *caption, unsigned type)
{
  (void)hwnd; (void)type;
  fprintf(stderr, "%s: %s\n", caption ? caption : "MessageBox", text ? text : "");
  return IDOK;
}

void ExitProcess(unsigned code)
{
  exit((int)code);
}

void DebugBreak(void)
{
  abort();
}

HWND GetDesktopWindow(void) { return NULL; }
BOOL OpenClipboard(HWND hwnd) { (void)hwnd; return FALSE; }
BOOL EmptyClipboard(void) { return FALSE; }
BOOL CloseClipboard(void) { return TRUE; }
HGLOBAL GlobalAlloc(unsigned flags, size_t bytes) { (void)flags; return malloc(bytes); }
void *GlobalLock(HGLOBAL mem) { return mem; }
BOOL GlobalUnlock(HGLOBAL mem) { (void)mem; return TRUE; }
HANDLE SetClipboardData(unsigned format, HANDLE mem) { (void)format; return mem; }

void InitializeCriticalSection(CRITICAL_SECTION *section)
{
#if defined(__EMSCRIPTEN__)
  (void)section;
#else
  pthread_mutex_init(&section->mutex, NULL);
#endif
}

void DeleteCriticalSection(CRITICAL_SECTION *section)
{
#if defined(__EMSCRIPTEN__)
  (void)section;
#else
  pthread_mutex_destroy(&section->mutex);
#endif
}

void EnterCriticalSection(CRITICAL_SECTION *section)
{
#if defined(__EMSCRIPTEN__)
  (void)section;
#else
  pthread_mutex_lock(&section->mutex);
#endif
}

void LeaveCriticalSection(CRITICAL_SECTION *section)
{
#if defined(__EMSCRIPTEN__)
  (void)section;
#else
  pthread_mutex_unlock(&section->mutex);
#endif
}

typedef struct thread_start_s {
  LPTHREAD_START_ROUTINE start;
  LPVOID param;
} thread_start_t;

static void *pthread_trampoline(void *arg)
{
  thread_start_t *ctx = (thread_start_t *)arg;
  LPTHREAD_START_ROUTINE start = ctx->start;
  LPVOID param = ctx->param;
  free(ctx);
  return (void *)(intptr_t)start(param);
}

HANDLE CreateThread(void *attr, size_t stackSize, LPTHREAD_START_ROUTINE start, LPVOID param, DWORD flags, LPDWORD threadId)
{
#if defined(__EMSCRIPTEN__)
  (void)attr; (void)stackSize; (void)flags;
  if (threadId) *threadId = 0;
  start(param);
  return NULL;
#else
  pthread_t *thread;
  thread_start_t *ctx;
  (void)attr; (void)stackSize; (void)flags;
  thread = (pthread_t *)malloc(sizeof(*thread));
  ctx = (thread_start_t *)malloc(sizeof(*ctx));
  if (!thread || !ctx) {
    free(thread); free(ctx); return NULL;
  }
  ctx->start = start;
  ctx->param = param;
  if (pthread_create(thread, NULL, pthread_trampoline, ctx) != 0) {
    free(thread); free(ctx); return NULL;
  }
  if (threadId) *threadId = (DWORD)(uintptr_t)*thread;
  return (HANDLE)thread;
#endif
}

DWORD WaitForSingleObject(HANDLE handle, DWORD milliseconds)
{
#if defined(__EMSCRIPTEN__)
  (void)handle; (void)milliseconds; return 0;
#else
  pthread_t *thread = (pthread_t *)handle;
  (void)milliseconds;
  if (thread) {
    pthread_join(*thread, NULL);
    free(thread);
  }
  return 0;
#endif
}

void GetSystemInfo(SYSTEM_INFO *info)
{
  long n = 1;
#if !defined(__EMSCRIPTEN__)
  n = sysconf(_SC_NPROCESSORS_ONLN);
#endif
  if (n < 1) n = 1;
  info->dwNumberOfProcessors = (DWORD)n;
}

void *LoadLibraryA(const char *name)
{
  (void)name; return NULL;
}

void *GetProcAddress(void *module, const char *name)
{
  (void)module; (void)name; return NULL;
}

#endif /* !_WIN32 */

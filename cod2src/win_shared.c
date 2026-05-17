/*
win_shared.c — Windows platform utilities

Reconstructed from cod2map.exe by Rose.
*/

#include "cod2map.h"

HANDLE hHeap;

int            g_crtComMode;
int            g_crtInvalidParamHandler;
int            g_crtNewMode;
intptr_t       g_crtPioinfo[16];
void         (*g_crtPostInitTable)(void) = NULL;
void         (*g_crtPreInitTable[2])(void) = { NULL, NULL };
void         (*g_crtUnexpectedHandler)() = NULL;
char           sys_basepath[MAX_OS_PATH_SHORT];
unsigned int   uNumber;


/*
================
Sys_Mkdir

Creates a directory at the specified path
================
*/
int Sys_Mkdir(const char *path)
{
  return _mkdir(path);
}

/*
================
Sys_DefaultCDPath

Returns the default CD path (unused, always empty string)
================
*/
const char *Sys_DefaultCDPath(void)
{
  return "";
}

/*
================
Sys_DefaultHomePath

Returns the default home path (unused in cod2map, always NULL)
================
*/
const char *Sys_DefaultHomePath(void)
{
  return NULL;
}

/*
================
Sys_DefaultBasePath

Returns the directory containing the executable, cached in a static buffer
================
*/
char *Sys_DefaultBasePath(void)
{
#if defined(__EMSCRIPTEN__)
  int len;

  if ( !sys_basepath[0] )
  {
    if ( !_getcwd(sys_basepath, sizeof(sys_basepath)) )
    {
      sys_basepath[0] = '/';
      sys_basepath[1] = '\0';
    }
    for ( len = 0; sys_basepath[len]; len++ )
    {
      if ( sys_basepath[len] == '\\' )
        sys_basepath[len] = '/';
    }
    while ( len > 1 && sys_basepath[len - 1] == '/' )
      sys_basepath[--len] = '\0';
    if ( len > 4 && !Q_stricmp(&sys_basepath[len - 4], "/bin") )
      sys_basepath[len - 4] = '\0';
  }
  return sys_basepath;
#else
  HMODULE hModule;
  DWORD len;
  CHAR ch;

  if ( !sys_basepath[0] )
  {
    hModule = GetModuleHandleA(0);
    len = GetModuleFileNameA(hModule, sys_basepath, sizeof(sys_basepath));
    if ( len == MAX_OS_PATH_SHORT )
    {
      len = MAX_OS_PATH_SHORT - 1;
    }
    else if ( !len )
    {
      sys_basepath[len] = 0;
      return sys_basepath;
    }
    do
    {
      ch = sys_basepath[len];
      if ( ch == '\\' )
        break;
      if ( ch == '/' )
        break;
      if ( ch == ':' )
        break;
      --len;
    }
    while ( len );
    sys_basepath[len] = 0;
  }
  return sys_basepath;
#endif
}

/*
================
Sys_ListFilteredFiles

Finds files matching a filter pattern in a directory, appending results to list
================
*/
void Sys_ListFilteredFiles(const char *basedir, const char *subdirs, const char *filter, char **list, int *numfiles)
{
  intptr_t findhandle;
  char search[MAX_OS_PATH_SHORT];
  char filename[MAX_OS_PATH_SHORT];
  struct _finddata_t finddata;

  if ( *numfiles < MAX_FOUND_FILES - 1 )
  {
    if ( strlen(subdirs) )
      Com_sprintf(search, sizeof(search), "%s\\%s\\*", basedir, subdirs);
    else
      Com_sprintf(search, sizeof(search), "%s\\*", basedir);
    findhandle = _findfirst(search, &finddata);
    if ( findhandle != -1 )
    {
      do
      {
        if ( (finddata.attrib & _A_SUBDIR) == 0 || Q_stricmp(finddata.name, ".") && Q_stricmp(finddata.name, "..") && Q_stricmp(finddata.name, "CVS") )
        {
          if ( *numfiles >= MAX_FOUND_FILES - 1 )
            break;
          if ( subdirs )
            Com_sprintf(filename, sizeof(filename), "%s\\%s", subdirs, finddata.name);
          else
            Com_sprintf(filename, sizeof(filename), "%s", finddata.name);
          if ( Com_FilterPath(filter, filename, 0) )
            list[(*numfiles)++] = CopyStringHunk(filename);
        }
      }
      while ( _findnext(findhandle, &finddata) != -1 );
      _findclose(findhandle);
    }
  }
}

/*
================
Sys_FilterExtension

Check if a filename matches a wildcard extension pattern
================
*/
qboolean Sys_FilterExtension(const char *filename, const char *extension)
{
  char pattern[MAX_OS_PATH_SHORT];

  Com_sprintf(pattern, sizeof(pattern), "*.%s", extension);
  return I_stricmpWild(pattern, filename) == 0;
}

/*
================
Sys_ListFiles

List files in a directory matching an extension or filter pattern
================
*/
char **Sys_ListFiles(const char *directory, const char *extension, const char *filter, int *numfiles, qboolean wantsubs)
{
  int nfiles;
  char **listCopy;
  int i;
  const char *ext;
  intptr_t findhandle;
  int subdirFlag;
  char *tmpList[MAX_FOUND_FILES];
  char search[MAX_OS_PATH_SHORT];
  struct _finddata_t finddata;

  if ( filter )
  {
    nfiles = 0;
    Sys_ListFilteredFiles(directory, g_fsBaseGame, filter, tmpList, &nfiles);
    tmpList[nfiles] = NULL;
    *numfiles = nfiles;
    if ( nfiles )
    {
      listCopy = (char **)Z_Malloc((nfiles + 1) * sizeof(char *));
      i = 0;
      if ( nfiles > 0 )
      {
        memcpy(listCopy, tmpList, nfiles * sizeof(char *));
        i = nfiles;
      }
      listCopy[i] = NULL;
      return listCopy;
    }
    return NULL;
  }
  ext = extension;
  if ( extension )
  {
    if ( *extension == '/' && !extension[1] )
    {
      ext = g_fsBaseGame;
      subdirFlag = 0;
    }
    else
    {
      subdirFlag = _A_SUBDIR;
    }
  }
  else
  {
    ext = g_fsBaseGame;
    subdirFlag = _A_SUBDIR;
  }
  if ( *ext )
    Com_sprintf(search, sizeof(search), "%s\\*.%s", directory, ext);
  else
    Com_sprintf(search, sizeof(search), "%s\\*", directory);
  nfiles = 0;
  findhandle = _findfirst(search, &finddata);
  if ( findhandle == -1 )
  {
    *numfiles = 0;
    return NULL;
  }
  do
  {
    if ( !wantsubs )
    {
      if ( subdirFlag == (finddata.attrib & _A_SUBDIR) )
        continue;
    }
    else
    {
      if ( (finddata.attrib & _A_SUBDIR) == 0 )
        continue;
    }
    if ( (finddata.attrib & _A_SUBDIR) && (!Q_stricmp(finddata.name, ".") || !Q_stricmp(finddata.name, "..") || !Q_stricmp(finddata.name, "CVS")) )
      continue;
    if ( *ext && !Sys_FilterExtension(finddata.name, ext) )
      continue;
    tmpList[nfiles++] = CopyStringHunk(finddata.name);
    if ( nfiles == MAX_FOUND_FILES - 1 )
      break;
  }
  while ( _findnext(findhandle, &finddata) != -1 );
  tmpList[nfiles] = NULL;
  _findclose(findhandle);
  *numfiles = nfiles;
  if ( !nfiles )
    return NULL;
  listCopy = (char **)Z_Malloc((nfiles + 1) * sizeof(char *));
  i = 0;
  if ( nfiles > 0 )
  {
    memcpy(listCopy, tmpList, nfiles * sizeof(char *));
    i = nfiles;
  }
  listCopy[i] = NULL;
  return listCopy;
}

/*
================
Sys_FreeFileList

Free a file list allocated by Sys_ListFiles
================
*/
void Sys_FreeFileList(char **list)
{
  int i;

  if ( list )
  {
    for ( i = 0; list[i]; i++ )
      Z_Free(list[i]);
    Z_Free(list);
  }
}

/*
================
Sys_DirectoryHasContents

Check if a directory has any files or subdirectories
================
*/
qboolean Sys_DirectoryHasContents(const char *dir)
{
  intptr_t findhandle;
  char search[MAX_OS_PATH_SHORT];
  struct _finddata_t finddata;

  Com_sprintf(search, sizeof(search), "%s\\*", dir);
  findhandle = _findfirst(search, &finddata);
  if ( findhandle == -1 )
    return 0;
  while ( (finddata.attrib & _A_SUBDIR) != 0 && (!Q_stricmp(finddata.name, ".") || !Q_stricmp(finddata.name, "..") || !Q_stricmp(finddata.name, "CVS")) )
  {
    if ( _findnext(findhandle, &finddata) == -1 )
      return 0;
  }
  return 1;
}

/*
================
Sys_RemoveDir

Recursively remove a directory and all its contents
================
*/
qboolean Sys_RemoveDir(const char *path)
{
  char lastChar;
  intptr_t findhandle;
  bool failed;
  char *fmt;
  char fullpath[MAX_OS_PATH_SHORT];
  struct _finddata_t finddata;
  char hasTrailingSlash;

  lastChar = path[strlen(path) - 1];
  if ( lastChar == '\\' || lastChar == '/' )
  {
    hasTrailingSlash = 1;
    Com_sprintf(fullpath, sizeof(fullpath), "%s*", path, finddata.name);
  }
  else
  {
    hasTrailingSlash = 0;
    Com_sprintf(fullpath, sizeof(fullpath), "%s\\*", path, finddata.name);
  }
  findhandle = _findfirst(fullpath, &finddata);
  if ( findhandle == -1 )
    return _rmdir(path) != -1;
  failed = 0;
  do
  {
    if ( finddata.name[0] != '.' || finddata.name[1] && (finddata.name[1] != '.' || finddata.name[2]) )
    {
      fmt = "%s%s";
      if ( !hasTrailingSlash )
        fmt = "%s\\%s";
      Com_sprintf(fullpath, sizeof(fullpath), fmt, path, finddata.name);
      failed = (finddata.attrib & _A_SUBDIR) != 0 ? Sys_RemoveDir(fullpath) == 0 : remove(fullpath) == -1;
      if ( failed )
        break;
    }
  }
  while ( _findnext(findhandle, &finddata) != -1 );
  _findclose(findhandle);
  return !failed && _rmdir(path) != -1;
}

/*
================
Sys_IsRenderThread

Check if the current thread is the render thread
================
*/
qboolean Sys_IsRenderThread(void)
{
  return 0;
}

/*
================
Sys_IsMainThread

Check if the current thread is the main thread
================
*/
qboolean Sys_IsMainThread(void)
{
  return 1;
}

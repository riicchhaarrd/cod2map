/*
com_shared.c — Shared utilities

Reconstructed from cod2map.exe by Rose.
*/

#include "cod2map.h"

char s_assertDisable_Com_Memcpy;


/*
================
Com_Error

Error function. Calls registered error handler if set,
otherwise prints to stderr and exits.
================
*/
int Com_Error(const char *fmt, ...)
{
  va_list args;

  va_start(args, fmt);
  if ( g_errorHandler )
    g_errorHandler(fmt, args);
  va_end(args);
  return 0;
}

/*
================
Com_SetErrorHandler

Sets a custom error callback for Com_Error.
cod2map-specific (not in q3/CoD2 server).
================
*/
void Com_SetErrorHandler(void (*handler)(const char *, va_list))
{
  g_errorHandler = handler;
}

/*
================
Com_DPrintf

Debug printf - only prints if verbose mode is enabled.
================
*/
int Com_DPrintf(const char *fmt, ...)
{
  va_list args;

  va_start(args, fmt);
  if ( verbose )
    return vprintf(fmt, args);
  return 0;
}

/*
================
Com_Printf

Main console output. Always prints to stdout, also sends
to CoD2Map Process Server window via Win32 IPC.
================
*/
void Com_Printf(const char *fmt, ...)
{
  ATOM atom;
  char buf[MAXPRINTMSG];
  va_list args;

  va_start(args, fmt);
  vsprintf(buf, fmt, args);
  printf(buf);

  /* IPC: send output to CoD2Map Process Server window */
  if ( !g_printfInit )
  {
    g_printfInit = 1;
    hWnd = FindWindowA(0, "CoD2Map Process Server");
    if ( !hWnd )
      return;
    Msg = RegisterWindowMessageA("Q3MPS_BroadcastCommand");
  }
  if ( hWnd )
  {
    atom = GlobalAddAtomA(buf);
    PostMessageA(hWnd, Msg, 0, atom);
  }
}

/*
================
CopyString

Allocates memory and copies a string (equivalent to strdup).
================
*/
char *CopyString(const char *str)
{
  char *copy;
  int len = (int)strlen(str) + 1;

  copy = malloc(len);
  if ( !copy )
    Z_MallocFailed(len);
  memset(copy, 0, len);
  strcpy(copy, str);
  return copy;
}

/*
================
Sys_Printf

System printf - always prints to stdout.
================
*/
int Sys_Printf(const char *fmt, ...)
{
  va_list args;

  va_start(args, fmt);
  return vprintf(fmt, args);
}

/*
================
Com_Memcpy

Byte-by-byte memory copy with assert validation.
================
*/
void Com_Memcpy( void *dest, const void *src, int count )
{
  int i;

  Assert( src || !count, s_assertDisable_Com_Memcpy );
  Assert( dest || !count, s_assertDisable_Com_Memcpy );

  for ( i = 0; i < count; i++ )
    ((char *)dest)[i] = ((const char *)src)[i];
}

/*
================
Com_Memset4

Fills memory with a 4-byte repeated value.
================
*/
int Com_Memset4( void *dest, int fillValue, int count )
{
  int *p = dest;
  int i;

  for ( i = 0; i < count; i++ )
    p[i] = fillValue;

  return fillValue;
}

/*
================
Com_Memset

Fills memory byte-by-byte using 4-byte packed writes.
================
*/
int Com_Memset( void *dest, int fillByte, int byteCount )
{
  unsigned char *p;
  unsigned short packedWord;
  int packedValue, i;

  packedWord = (unsigned short)(fillByte | (fillByte << 8));
  packedValue = packedWord | (packedWord << 16);

  Com_Memset4( dest, packedValue, byteCount / 4 );

  p = (unsigned char *)dest + (byteCount & ~3);
  for ( i = 0; i < (byteCount & 3); i++ )
    p[i] = fillByte;

  return packedValue;
}

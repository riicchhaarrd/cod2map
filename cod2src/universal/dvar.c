/*
dvar.c — Dynamic variable system

Reconstructed from cod2map.exe by Rose.
*/

#include "cod2map.h"

Dvar_t   *dvar_cheats;
Dvar_t    dvarPool[MAX_DVARS];
intptr_t  dvarHashTable[DVAR_HASH_TABLE_SIZE];

char      digitStrings[10][2] = {
         "0", "1", "2", "3", "4", "5", "6", "7", "8", "9"
};
int       dvarCount;
int       dvarParseBuffer[16];
int       dvar_modifiedFlags;
char      g_dvarString0[4] = "0";
char      g_dvarString1[4] = "1";
int       g_dvarStringBufIdx;
int       g_dvarStringBufs[10];
char     *g_dvarStringOff = NULL;
char     *g_dvarStringOn = "on";
char      g_emptyString[4] = {0};
char      isDvarSystemActive;
intptr_t  sortedDvars;

char s_assertDisable_Dvar_AddFlags;
char s_assertDisable_Dvar_AssignResetStringValue;
char s_assertDisable_Dvar_ClampValueToDomain;
char s_assertDisable_Dvar_ClearModified;
char s_assertDisable_Dvar_ClearModifiedFlags;
char s_assertDisable_Dvar_CopyString;
char s_assertDisable_Dvar_DisplayableLatchedValue;
char s_assertDisable_Dvar_DisplayableResetValue;
char s_assertDisable_Dvar_DomainToString_Internal;
char s_assertDisable_Dvar_FindMalleableVar;
char s_assertDisable_Dvar_FreeResetValue;
char s_assertDisable_Dvar_GetString;
char s_assertDisable_Dvar_GetUnpackedColor;
char s_assertDisable_Dvar_PerformUnregistration;
char s_assertDisable_Dvar_RegisterString;
char s_assertDisable_Dvar_RegisterVariant;
char s_assertDisable_Dvar_SetBool;
char s_assertDisable_Dvar_SetColor;
char s_assertDisable_Dvar_SetFloat;
char s_assertDisable_Dvar_SetInt;
char s_assertDisable_Dvar_SetResetValue;
char s_assertDisable_Dvar_SetString;
char s_assertDisable_Dvar_SetVariant;
char s_assertDisable_Dvar_ReRegisterVariant;
char s_assertDisable_Dvar_SetVec2;
char s_assertDisable_Dvar_SetVec3;
char s_assertDisable_Dvar_SetVec4;
char s_assertDisable_Dvar_StringToBool;
char s_assertDisable_Dvar_StringToEnum;
char s_assertDisable_Dvar_StringToFloat;
char s_assertDisable_Dvar_StringToInt;
char s_assertDisable_Dvar_StringToValue;
char s_assertDisable_Dvar_StringToVec2;
char s_assertDisable_Dvar_StringToVec3;
char s_assertDisable_Dvar_StringToVec4;
char s_assertDisable_Dvar_ValueInDomain;
char s_assertDisable_Dvar_ValueToString;
char s_assertDisable_Dvar_ValuesEqual;


/*
================
Dvar_IsSystemActive

Returns whether the dvar system is active.
================
*/
bool Dvar_IsSystemActive()
{
  return isDvarSystemActive;
}

/*
================
Dvar_GenerateHashValue

Generates a hash value for dvar name lookup in the hash table.
================
*/
int Dvar_GenerateHashValue(const char *name)
{
  int i;
  unsigned char hash;

  if ( !name )
    Com_ErrorLevel(1, "null name in generateHashValue");

  hash = 0;
  for ( i = 0; name[i]; i++ )
    hash += (i + HASH_POSITION_SEED) * tolower(name[i]);
  return hash;
}

/*
================
Dvar_MakeExplicitType

Allocates current/latched/reset value arrays for vector-typed dvars.
================
*/
int *Dvar_MakeExplicitType(Dvar_t *dvar)
{
  int *buffer;
  int components;
  int *latchedBuf;
  int *resetBuf;

  buffer = Z_Malloc( 3 * sizeof(int) * dvar->type );
  components = dvar->type;
  dvar->current.vector = (float *)buffer;
  latchedBuf = &buffer[components];
  dvar->latched.vector = (float *)latchedBuf;
  resetBuf = &latchedBuf[components];
  dvar->reset.vector = (float *)resetBuf;
  return resetBuf;
}

/*
================
Dvar_FreeCurrentValue

Frees the current value string of a dvar, if it's uniquely owned.
================
*/
void Dvar_FreeCurrentValue(Dvar_t *dvar)
{
  char *str;
  char firstCh;

  str = (char *)dvar->current.string;
  if ( str == (char *)dvar->latched.string || str == (char *)dvar->reset.string )
  {
    dvar->current.integer = 0;
  }
  else
  {
    firstCh = *str;
    if ( *str && (str[1] || firstCh < '0' || firstCh > '9') && str != g_dvarStringOff && str != g_dvarStringOn )
      Z_Free(str);
    dvar->current.integer = 0;
  }
}

/*
================
Dvar_FreeLatchedValue

Frees the latched value string of a dvar, if it's uniquely owned.
================
*/
void Dvar_FreeLatchedValue(Dvar_t *dvar)
{
  char *str;
  char firstCh;

  str = (char *)dvar->latched.string;
  if ( str == (char *)dvar->current.string || str == (char *)dvar->reset.string )
  {
    dvar->latched.integer = 0;
  }
  else
  {
    firstCh = *str;
    if ( *str && (str[1] || firstCh < '0' || firstCh > '9') && str != g_dvarStringOff && str != g_dvarStringOn )
      Z_Free(str);
    dvar->latched.integer = 0;
  }
}

/*
================
Dvar_FreeResetValue

Frees the reset value string of a dvar, if it's uniquely owned.
================
*/
void Dvar_FreeResetValue(Dvar_t *dvar)
{
  char *str;
  char firstCh;

  str = (char *)dvar->reset.string;
  if ( str == (char *)dvar->current.string || str == (char *)dvar->latched.string )
  {
    dvar->reset.integer = 0;
  }
  else
  {
    firstCh = *str;
    if ( *str && (str[1] || firstCh < '0' || firstCh > '9') && str != g_dvarStringOff && str != g_dvarStringOn )
      Z_Free(str);
    dvar->reset.integer = 0;
  }
}

/*
================
Dvar_EnumToString

Returns the string representation of the current enum value.
================
*/
const char *Dvar_EnumToString( Dvar_t *dvar )
{
  int idx;

  Assert( dvar, s_assertDisable_Dvar_FreeResetValue );
  Assert( dvar->name, s_assertDisable_Dvar_FreeResetValue );

  Assert(!(dvar->type != DVAR_TYPE_ENUM), s_assertDisable_Dvar_FreeResetValue);
  Assert(!(!dvar->domain.enumeration.strings), s_assertDisable_Dvar_FreeResetValue);

  idx = (int)dvar->current.integer;
  Assert(!((idx < 0 || (idx >= dvar->domain.enumeration.stringCount && idx))), s_assertDisable_Dvar_FreeResetValue);

  if ( dvar->domain.enumeration.stringCount )
    return dvar->domain.enumeration.strings[dvar->current.integer];
  return g_fsBaseGame;
}

/*
================
Dvar_ValueToString

Converts a dvar value to its string representation based on type.
================
*/
char *Dvar_ValueToString( Dvar_t *dvar, DvarValue_t value )
{
  switch ( dvar->type )
  {
    case DVAR_TYPE_BOOL:
      return value.enabled ? g_dvarString1 : g_dvarString0;
    case DVAR_TYPE_FLOAT:
      return va( "%g", value.value );
    case DVAR_TYPE_VEC2:
      return va( "%g %g", value.vector[0], value.vector[1] );
    case DVAR_TYPE_VEC3:
      return va( FMT_VECTOR3, value.vector[0], value.vector[1], value.vector[2] );
    case DVAR_TYPE_VEC4:
      return va( "%g %g %g %g", value.vector[0], value.vector[1], value.vector[2], value.vector[3] );
    case DVAR_TYPE_INT:
      return va( "%i", value.integer );

    case DVAR_TYPE_ENUM:
      Assert(!((value.integer < 0 || (value.integer >= dvar->domain.enumeration.stringCount && value.integer != 0))), s_assertDisable_Dvar_ValueToString);
      if ( !dvar->domain.enumeration.stringCount )
        return g_fsBaseGame;
      return (char *)dvar->domain.enumeration.strings[value.integer];

    case DVAR_TYPE_STRING:
      Assert(value.string, s_assertDisable_Dvar_ValueToString);
      return va( "%s", value.string );

    case DVAR_TYPE_COLOR:
      return va( "%g %g %g %g",
        value.color[0] * (1.0 / 255.0),
        value.color[1] * (1.0 / 255.0),
        value.color[2] * (1.0 / 255.0),
        value.color[3] * (1.0 / 255.0) );

    default:
      Assert(0, s_assertDisable_Dvar_ValueToString);
      return g_fsBaseGame;
  }
}

/*
================
Dvar_StringToBool

Converts a string to a boolean value.
================
*/
int Dvar_StringToBool( const char *string )
{
  Assert(string, s_assertDisable_Dvar_StringToBool);
  return atol( string ) != 0;
}

/*
================
Dvar_StringToInt

Converts a string to an integer value.
================
*/
int Dvar_StringToInt( const char *string )
{
  Assert(string, s_assertDisable_Dvar_StringToInt);
  return atol( string );
}

/*
================
Dvar_StringToFloat

Converts a string to a float value.
================
*/
double Dvar_StringToFloat( const char *string )
{
  Assert(string, s_assertDisable_Dvar_StringToFloat);
  return atof( string );
}

/*
================
Dvar_DisplayableResetValue

Returns the displayable string for a dvar's reset value.
================
*/
char *Dvar_DisplayableResetValue( Dvar_t *dvar )
{
  Assert(dvar, s_assertDisable_Dvar_DisplayableResetValue);
  return Dvar_ValueToString( dvar, dvar->reset );
}

/*
================
Dvar_DisplayableLatchedValue

Returns the displayable string for a dvar's latched value.
================
*/
char *Dvar_DisplayableLatchedValue( Dvar_t *dvar )
{
  Assert(dvar, s_assertDisable_Dvar_DisplayableLatchedValue);
  return Dvar_ValueToString( dvar, dvar->latched );
}

/*
================
Dvar_ClampValueToDomain

Clamps a dvar value to its valid domain range based on type.
================
*/
float *Dvar_ClampValueToDomain( unsigned char type, float *value, intptr_t resetValue, float min, float max )
{
  float *clampedValue;
  union { float f; int i; } umin, umax;
  umin.f = min; umax.f = max;

  switch ( type )
  {
    case DVAR_TYPE_BOOL:
      *(unsigned char *)&value = (unsigned char)(uintptr_t)value != 0;
      return value;
    case DVAR_TYPE_FLOAT:
      { union { float *p; float f; } uval = {value};
      if ( uval.f < (double)min )
        return (float *)(intptr_t)umin.i;
      if ( uval.f <= (double)max )
        return value;
      return (float *)(intptr_t)umax.i; }
    case DVAR_TYPE_VEC2:
    case DVAR_TYPE_VEC3:
    case DVAR_TYPE_VEC4:
      { int i, count = type; /* DVAR_TYPE_VEC2=2, VEC3=3, VEC4=4 */
      for ( i = 0; i < count; i++ )
      {
        if ( value[i] < min )
          value[i] = min;
        else if ( value[i] > max )
          value[i] = max;
      } }
      return value;
    case DVAR_TYPE_INT:
      Assert(!(umin.i > umax.i), s_assertDisable_Dvar_ClampValueToDomain);
      clampedValue = value;
      if ( (intptr_t)value < umin.i )
        return (float *)(uintptr_t)umin.i;
      if ( (intptr_t)value <= umax.i )
        return clampedValue;
      return (float *)(uintptr_t)umax.i;
    case DVAR_TYPE_ENUM:
      clampedValue = value;
      if ( (intptr_t)value >= 0 && (intptr_t)value < umin.i )
        return clampedValue;
      clampedValue = (float *)(intptr_t)resetValue;
      if ( resetValue >= 0 && (resetValue < umin.i || !resetValue) )
        return clampedValue;
      Assert(resetValue >= 0 && (resetValue < umin.i || !resetValue), s_assertDisable_Dvar_ClampValueToDomain);
      return clampedValue;
    case DVAR_TYPE_STRING:
    case DVAR_TYPE_COLOR:
      return value;
    default:
      Assert(0, s_assertDisable_Dvar_ClampValueToDomain);
      return value;
  }
}

/*
================
Dvar_ValueInDomain

Returns true if the value is within the dvar's valid domain.
================
*/
bool Dvar_ValueInDomain(unsigned char type, float value, float min, float max)
{
  union { float f; int i; } umin, umax, uval;
  int i;
  umin.f = min; umax.f = max; uval.f = value;

  switch ( type )
  {
    case DVAR_TYPE_BOOL:
      Assert(!(*(unsigned char *)&value >= 2), s_assertDisable_Dvar_ValueInDomain);
      return 1;
    case DVAR_TYPE_FLOAT:
      return value >= (double)min && value <= (double)max;
    case DVAR_TYPE_VEC2:
    case DVAR_TYPE_VEC3:
    case DVAR_TYPE_VEC4:
      { float *vec = *(float **)&value;
      for ( i = 0; i < type; i++ )
        if ( vec[i] < min || vec[i] > max )
          return 0;
      } return 1;
    case DVAR_TYPE_INT:
      Assert(!(umin.i > umax.i), s_assertDisable_Dvar_ClampValueToDomain);
      return uval.i >= umin.i && uval.i <= umax.i;
    case DVAR_TYPE_ENUM:
      return value >= 0.0 && (uval.i < umin.i || value == 0.0);
    case DVAR_TYPE_STRING:
    case DVAR_TYPE_COLOR:
      return 1;
    default:
      Assert(0, s_assertDisable_Dvar_ClampValueToDomain);
      return 0;
  }
}

/*
================
Dvar_ValuesEqual_Vec2

Returns true if two vec2 values are equal.
================
*/
int Dvar_ValuesEqual_Vec2(float *value0, float *value1)
{
  return *value0 == *value1 && value0[1] == value1[1];
}

/*
================
Dvar_VectorDomainToString

Formats a vector domain description string.
================
*/
int Dvar_VectorDomainToString(size_t bufLen, char *buf, int components, float min, float max)
{
  if ( min == -FLT_MAX )
  {
    if ( max == FLT_MAX )
      return _snprintf(buf, bufLen, "Domain is any %iD vector", components);
    else
      return _snprintf(buf, bufLen, "Domain is any %iD vector with components %g or smaller", components, max);
  }
  else if ( max == FLT_MAX )
  {
    return _snprintf(buf, bufLen, "Domain is any %iD vector with components %g or bigger", components, min);
  }
  else
  {
    return _snprintf(buf, bufLen, "Domain is any %iD vector with components from %g to %g", components, min, max);
  }
}

/*
================
Dvar_DomainToString_Internal

Converts a dvar domain to a human-readable description string.
================
*/
char *Dvar_DomainToString_Internal(char *buf, int bufLen, unsigned char type, long long domain, int *enumLinesCount)
{
  char *result;
  int written, enumIdx, lineLen;
  char *bufEnd;

  union { long long ll; float f[2]; } domainU;
  domainU.ll = domain;

  Assert(bufLen > 0, s_assertDisable_Dvar_DomainToString_Internal);
  bufEnd = &buf[bufLen];
  if ( enumLinesCount )
    *enumLinesCount = 0;
  switch ( type )
  {
    case DVAR_TYPE_BOOL:
      _snprintf(buf, bufLen, "Domain is 0 or 1");
      result = buf;
      *(bufEnd - 1) = 0;
      return result;
    case DVAR_TYPE_FLOAT:
      if ( domainU.f[0] == -FLT_MAX )
      {
        if ( domainU.f[1] == FLT_MAX )
          _snprintf(buf, bufLen, "Domain is any number");
        else
          _snprintf(buf, bufLen, "Domain is any number %g or smaller", domainU.f[1]);
        result = buf;
        *(bufEnd - 1) = 0;
      }
      else
      {
        if ( domainU.f[1] == FLT_MAX )
          _snprintf(buf, bufLen, "Domain is any number %g or bigger", domainU.f[0]);
        else
          _snprintf(buf, bufLen, "Domain is any number from %g to %g", domainU.f[0], domainU.f[1]);
        result = buf;
        *(bufEnd - 1) = 0;
      }
      return result;
    case DVAR_TYPE_VEC2:
    case DVAR_TYPE_VEC3:
    case DVAR_TYPE_VEC4:
      Dvar_VectorDomainToString(bufLen, buf, type, domainU.f[0], domainU.f[1]);
      result = buf;
      *(bufEnd - 1) = 0;
      return result;
    case DVAR_TYPE_INT:
      { int iMin = (int)domain, iMax = (int)(domain >> 32);
      if ( iMin == INT_MIN )
      {
        if ( iMax == INT_MAX )
          _snprintf(buf, bufLen, "Domain is any integer");
        else
          _snprintf(buf, bufLen, "Domain is any integer %i or smaller", iMax);
      }
      else
      {
        if ( iMax == INT_MAX )
          _snprintf(buf, bufLen, "Domain is any integer %i or bigger", iMin);
        else
          _snprintf(buf, bufLen, "Domain is any integer from %i to %i", iMin, iMax);
      } }
      result = buf;
      *(bufEnd - 1) = 0;
      return result;
    case DVAR_TYPE_ENUM:
      { int enumCount = (int)domain;
      const char **enumStrings = (const char **)(uintptr_t)(domain >> 32);
      written = _snprintf(buf, bufLen, "Domain is one of the following:");
      if ( written >= 0 )
      {
        buf += written;
        for ( enumIdx = 0; enumIdx < enumCount; enumIdx++ )
        {
          lineLen = _snprintf(buf, bufEnd - buf, "\n  %2i: %s", enumIdx, enumStrings[enumIdx]);
          if ( lineLen < 0 )
            break;
          if ( enumLinesCount )
            ++*enumLinesCount;
          buf += lineLen;
        }
      } }
      result = buf;
      *(bufEnd - 1) = 0;
      return result;
    case DVAR_TYPE_STRING:
      _snprintf(buf, bufLen, "Domain is any text");
      result = buf;
      *(bufEnd - 1) = 0;
      return result;
    case DVAR_TYPE_COLOR:
      _snprintf(buf, bufLen, "Domain is any 4-component color, in RGBA format");
      result = buf;
      *(bufEnd - 1) = 0;
      return result;
    default:
      Assert(0, s_assertDisable_Dvar_DomainToString_Internal);
      *buf = 0;
      *(bufEnd - 1) = 0;
      return buf;
  }
}

/*
================
Dvar_PrintDomain

Prints the domain description for a dvar type.
================
*/
int Dvar_PrintDomain(char type, long long domain)
{
  char *str;
  char buf[MAX_OS_PATH];

  str = Dvar_DomainToString_Internal(buf, sizeof(buf), type, domain, 0);
  return Sys_Printf("  %s\n", str);
}

/*
================
Dvar_FindVar

Finds a dvar by name using the hash table.
================
*/
Dvar_t *Dvar_FindVar(char *name)
{
  Dvar_t *dvar;

  dvar = (Dvar_t *)dvarHashTable[Dvar_GenerateHashValue(name)];
  if ( !dvar )
    return NULL;
  while ( Q_stricmp(name, dvar->name) )
  {
    dvar = dvar->hashNext;
    if ( !dvar )
      return NULL;
  }
  return dvar;
}

/*
================
Dvar_CompareVec4

Returns true if two vec4 values are exactly equal.
================
*/
bool Dvar_CompareVec4(float *v0, float *v1)
{
  return *v0 == *v1 && v0[1] == v1[1] && v0[2] == v1[2] && v0[3] == v1[3];
}

/*
================
Dvar_ClearModified

Clears the modified flag on a dvar.
================
*/
void Dvar_ClearModified(Dvar_t *dvar)
{
  Assert(dvar, s_assertDisable_Dvar_ClearModified);
  dvar->modified = 0;
}

/*
================
Dvar_GetString

Returns the string value of a dvar by name.
================
*/
const char *Dvar_GetString( const char *dvarName )
{
  Dvar_t *dvar;

  dvar = (Dvar_t *)Dvar_FindVar( (char *)dvarName );
  if ( !dvar )
    return (const char *)g_fsBaseGame;

  Assert(!(dvar->type != DVAR_TYPE_STRING && dvar->type != DVAR_TYPE_ENUM), s_assertDisable_Dvar_GetString);

  if ( dvar->type == DVAR_TYPE_ENUM )
    return Dvar_EnumToString( dvar );
  return dvar->current.string;
}

/*
================
Dvar_Shutdown

Frees all dvar resources and resets the dvar system.
================
*/
void Dvar_Shutdown(void)
{
  Dvar_t *dvar;

  for ( dvar = (Dvar_t *)sortedDvars; dvar; dvar = dvar->next )
  {
    if ( dvar->type == DVAR_TYPE_STRING )
    {
      Dvar_FreeCurrentValue(dvar);
      Dvar_FreeResetValue(dvar);
      Dvar_FreeLatchedValue(dvar);
    }
    else if ( dvar->type == DVAR_TYPE_VEC2 || dvar->type == DVAR_TYPE_VEC3 || dvar->type == DVAR_TYPE_VEC4 )
    {
      Z_Free(dvar->current.vector);
    }
    if ( dvar->flags & DVAR_EXTERNAL )
      Z_Free((void *)dvar->name);
  }
  memset(dvarHashTable, 0, sizeof(dvarHashTable));
  dvarCount = 0;
  sortedDvars = 0;
  dvar_cheats = NULL;
  dvar_modifiedFlags = 0;
  isDvarSystemActive = 0;
}

/*
================
Dvar_AddFlags

Adds flags to a dvar, asserting that protected flags are not set.
================
*/
void Dvar_AddFlags( Dvar_t *dvar, int flags )
{
  Assert( dvar, s_assertDisable_Dvar_AddFlags );

  #define DVAR_PROTECTED_FLAGS 0x70F0  /* bits 4-7, 12-14: protected flag mask */
  Assert((flags & DVAR_PROTECTED_FLAGS) == 0, s_assertDisable_Dvar_AddFlags);

  dvar->flags |= flags;
}

/*
================
Dvar_CopyString

Copies a string for dvar storage, returning cached pointers for common
values like empty, single digits, "on", and "off".
================
*/
char *Dvar_CopyString(const char *string)
{
  char firstCh;
  unsigned int len;
  char secondCh;

  Assert(string, s_assertDisable_Dvar_CopyString);
  firstCh = *string;
  if ( !*string )
    return (char *)g_fsBaseGame;
  len = (unsigned int)strlen(string);
  secondCh = string[1];
  if ( secondCh )
  {
    if ( firstCh == 'o' )
    {
      if ( len == 3 )
      {
        if ( secondCh == 'f' && string[2] == 'f' && !string[3] )
          return (char *)g_dvarStringOff;
      }
      else if ( len == 2 && secondCh == 'n' && !string[2] )
      {
        return g_dvarStringOn;
      }
    }
  }
  else if ( firstCh >= '0' && firstCh <= '9' )
  {
    return digitStrings[firstCh - '0'];
  }
  return CopyStringHunk((char *)string);
}

/*
================
Dvar_AssignCurrentStringValue

Assigns a string to the dvar's current value, reusing latched/reset pointers when possible.
================
*/
void Dvar_AssignCurrentStringValue(const char *string, Dvar_t *dvar)
{
  const char *match;

  match = dvar->latched.string;
  if ( match && (string == match || strcmp(string, dvar->latched.string) == 0)
    || (match = dvar->reset.string) != 0
    && (string == match || strcmp(string, dvar->reset.string) == 0) )
  {
    dvar->current.string = match;
  }
  else
  {
    dvar->current.string = Dvar_CopyString(string);
  }
}

/*
================
Dvar_AssignLatchedStringValue

Assigns a string to the dvar's latched value, reusing current/reset pointers when possible.
================
*/
void Dvar_AssignLatchedStringValue(const char *string, Dvar_t *dvar)
{
  const char *match;

  match = dvar->current.string;
  if ( match && (string == match || strcmp(string, dvar->current.string) == 0)
    || (match = dvar->reset.string) != 0
    && (string == match || strcmp(string, dvar->reset.string) == 0) )
  {
    dvar->latched.string = match;
  }
  else
  {
    dvar->latched.string = Dvar_CopyString(string);
  }
}

/*
================
Dvar_AssignResetStringValue

Assigns a string to the dvar's reset value, reusing current/latched pointers when possible.
================
*/
void Dvar_AssignResetStringValue(const char *string, Dvar_t *dvar)
{
  const char *match;

  Assert(string, s_assertDisable_Dvar_AssignResetStringValue);
  match = dvar->current.string;
  if ( match && (string == match || !strcmp(string, dvar->current.string))
    || (match = dvar->latched.string) != 0 && (string == match || !strcmp(string, dvar->latched.string)) )
  {
    dvar->reset.string = match;
  }
  else
  {
    dvar->reset.string = Dvar_CopyString(string);
  }
}

/*
================
Dvar_StringToVec2

Parses a string into a vec2, using a rotating parse buffer.
================
*/
float *Dvar_StringToVec2(const char *string)
{
  int bufIdx;
  float *result;

  Assert(string, s_assertDisable_Dvar_StringToVec2);
  bufIdx = g_dvarStringBufIdx;
  if ( (unsigned int)(g_dvarStringBufIdx + 2) > DVAR_PARSE_BUF_SLOTS )
    bufIdx = 0;
  result = (float *)&dvarParseBuffer[bufIdx];
  g_dvarStringBufIdx = bufIdx + 2;
  result[0] = 0.0f;
  result[1] = 0.0f;
  sscanf( string, g_fmtVec2, &result[0], &result[1] );
  return result;
}

/*
================
Dvar_StringToVec3

Parses a string into a vec3, using a rotating parse buffer.
================
*/
float *Dvar_StringToVec3(const char *string)
{
  int bufIdx;
  float *result;

  Assert(string, s_assertDisable_Dvar_StringToVec3);
  bufIdx = g_dvarStringBufIdx;
  if ( (unsigned int)(g_dvarStringBufIdx + 3) > DVAR_PARSE_BUF_SLOTS )
    bufIdx = 0;
  result = (float *)&dvarParseBuffer[bufIdx];
  g_dvarStringBufIdx = bufIdx + 3;
  result[0] = 0.0f;
  result[1] = 0.0f;
  result[2] = 0.0f;
  sscanf( string, FMT_VECTOR3, &result[0], &result[1], &result[2] );
  return result;
}

/*
================
Dvar_StringToVec4

Parses a string into a vec4, using a rotating parse buffer.
================
*/
float *Dvar_StringToVec4( const char *string )
{
  int bufIdx;
  float *result;

  Assert( string, s_assertDisable_Dvar_StringToVec4 );
  bufIdx = g_dvarStringBufIdx;
  if ( (unsigned int)(g_dvarStringBufIdx + 4) > DVAR_PARSE_BUF_SLOTS )
    bufIdx = 0;
  result = (float *)&dvarParseBuffer[bufIdx];
  g_dvarStringBufIdx = bufIdx + 4;
  result[0] = 0.0f;
  result[1] = 0.0f;
  result[2] = 0.0f;
  result[3] = 0.0f;
  sscanf( string, g_fmtVec4, &result[0], &result[1], &result[2], &result[3] );
  return result;
}

/*
================
Dvar_StringToEnum

Converts a string to an enum index by name match, numeric parse, or prefix match.
================
*/
int Dvar_StringToEnum(DvarLimits_t *domain, const char *string)
{
  int i, numericValue;
  size_t len;

  Assert(domain, s_assertDisable_Dvar_StringToEnum);
  Assert(string, s_assertDisable_Dvar_StringToEnum);

  /* exact case-insensitive match */
  for ( i = 0; i < domain->enumeration.stringCount; i++ )
  {
    if ( !_stricmp(string, domain->enumeration.strings[i]) )
      return i;
  }

  /* try parsing as numeric index */
  numericValue = atoi(string);
  if ( *string >= '0' && *string <= '9' && numericValue >= 0 && numericValue < domain->enumeration.stringCount )
    return numericValue;

  /* prefix match */
  len = strlen(string);
  for ( i = 0; i < domain->enumeration.stringCount; i++ )
  {
    if ( !_strnicmp(string, domain->enumeration.strings[i], len) )
      return i;
  }

  return DVAR_INVALID_ENUM_INDEX;
}

/*
================
Dvar_StringToColor

Parses a string into a packed RGBA color (4 bytes), clamping each component to [0,1].
================
*/
void Dvar_StringToColor(unsigned char *color, const char *string)
{
  float colorVec[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
  double clamped;
  int i;

  sscanf(string, g_fmtVec4, &colorVec[0], &colorVec[1], &colorVec[2], &colorVec[3]);
  for ( i = 0; i < 4; i++ )
  {
    clamped = colorVec[i];
    if ( clamped > 1.0 )
      clamped = 1.0;
    if ( clamped < 0.0 )
      clamped = 0.0;
    color[i] = (unsigned char)xs_RoundToInt((float)(clamped * 255.0) + FISTP_BIAS);
  }
}

/*
================
Dvar_StringToValue

Converts a string to a DvarValue based on the specified type.
================
*/
char *Dvar_StringToValue( const char *string, int type, int domain )
{
  Assert( string, s_assertDisable_Dvar_StringToValue );

  switch ( (unsigned char)type )
  {
    case DVAR_TYPE_BOOL:
      return (char *)(intptr_t)Dvar_StringToBool( string );
    case DVAR_TYPE_FLOAT:
      { union { float f; int i; } u; u.f = (float)Dvar_StringToFloat( string ); return (char *)(intptr_t)u.i; }
    case DVAR_TYPE_VEC2:
      return (char *)Dvar_StringToVec2( string );
    case DVAR_TYPE_VEC3:
      return (char *)Dvar_StringToVec3( string );
    case DVAR_TYPE_VEC4:
      return (char *)Dvar_StringToVec4( string );
    case DVAR_TYPE_INT:
      return (char *)(intptr_t)Dvar_StringToInt( string );
    case DVAR_TYPE_ENUM:
      return (char *)(intptr_t)Dvar_StringToEnum( (DvarLimits_t *)&domain, string );
    case DVAR_TYPE_STRING:
      return (char *)string;
    case DVAR_TYPE_COLOR:
      Dvar_StringToColor( (unsigned char *)&type, string );
      return (char *)(intptr_t)type;
    default:
      Assert(0, s_assertDisable_Dvar_StringToValue);
      return 0;
  }
}

/*
================
Dvar_ValuesEqual

Returns true if two dvar values of the given type are equal.
================
*/
bool Dvar_ValuesEqual( unsigned char type, float *val0, float *val1 )
{
  switch ( type )
  {
    case DVAR_TYPE_BOOL:
      return (char)val0 == (unsigned char)val1;
    case DVAR_TYPE_FLOAT:
      { union { float *p; float f; } u0 = {val0}, u1 = {val1}; return u0.f == u1.f; }
    case DVAR_TYPE_VEC2:
      return Dvar_ValuesEqual_Vec2( val0, val1 );
    case DVAR_TYPE_VEC3:
      return VectorCompareExact( val0, val1 );
    case DVAR_TYPE_VEC4:
      return Dvar_CompareVec4( val0, val1 );
    case DVAR_TYPE_INT:
    case DVAR_TYPE_ENUM:
      return val0 == val1;
    case DVAR_TYPE_STRING:
      Assert(!(val0 == NULL), s_assertDisable_Dvar_StringToValue);
      Assert(!(val1 == NULL), s_assertDisable_Dvar_StringToValue);
      return strcmp( (const char *)val0, (const char *)val1 ) == 0;
    case DVAR_TYPE_COLOR:
      return val0 == val1;
    default:
      Assert(0, s_assertDisable_Dvar_ValuesEqual);
      return 0;
  }
}

/*
================
Dvar_SetLatchedValue

Sets the latched value of a dvar, handling vector and string types specially.
================
*/
char *Dvar_SetLatchedValue(Dvar_t *dvar, intptr_t value)
{
  float *src = (float *)value;

  switch ( dvar->type )
  {
    case DVAR_TYPE_VEC2:
      dvar->latched.vector[0] = src[0];
      dvar->latched.vector[1] = src[1];
      break;
    case DVAR_TYPE_VEC3:
      VectorCopy(src, dvar->latched.vector);
      break;
    case DVAR_TYPE_VEC4:
      Vector4Copy(src, dvar->latched.vector);
      break;
    case DVAR_TYPE_STRING:
      Dvar_FreeLatchedValue(dvar);
      Dvar_AssignLatchedStringValue((const char *)value, dvar);
      break;
    default:
      dvar->latched.integer = value;
      break;
  }
  return (char *)value;
}

/*
================
Dvar_SetVariant

Sets a dvar's value from any source, checking domain, permissions, and latching.
================
*/
void Dvar_SetVariant( Dvar_t *dvar, intptr_t value, DvarSetSource_t source )
{
  unsigned short dvarFlags;
  float *cur, *lat;

  union { intptr_t i; float f; } uval;
  uval.i = value;

  while ( 1 )
  {
    Assert(dvar, s_assertDisable_Dvar_SetVariant);
    Assert(dvar->name, s_assertDisable_Dvar_SetVariant);
    if ( Dvar_ValueInDomain(dvar->type, uval.f, dvar->domain.value.min, dvar->domain.value.max) )
      break;
    { char *msg = Dvar_ValueToString(dvar, *(DvarValue_t *)&value);
    Sys_Printf("'%s' is not a valid value for dvar '%s'\n", msg, dvar->name); }
    Dvar_PrintDomain(dvar->type, *(long long *)&dvar->domain);
    if ( dvar->type != DVAR_TYPE_ENUM )
      return;
    if ( Dvar_ValueInDomain(DVAR_TYPE_ENUM, dvar->reset.value, dvar->domain.value.min, dvar->domain.value.max) || s_assertDisable_Dvar_SetVariant )
    {
      value = dvar->reset.integer;
    }
    else
    {
      Assert(0, s_assertDisable_Dvar_SetVariant);
      value = dvar->reset.integer;
    }
  }
  if ( (source == DVAR_SOURCE_EXTERNAL || source == DVAR_SOURCE_SCRIPT) )
  {
    dvarFlags = dvar->flags;
    if ( (dvarFlags & DVAR_ROM) != 0 )
    {
      Sys_Printf("%s is read only.\n", dvar->name);
      return;
    }
    if ( (dvarFlags & DVAR_INIT) != 0 )
    {
      Sys_Printf("%s is write protected.\n", dvar->name);
      return;
    }
    if ( source == DVAR_SOURCE_EXTERNAL && (dvarFlags & DVAR_CHEAT) != 0 && !dvar_cheats->current.enabled )
    {
      Sys_Printf("%s is cheat protected.\n", dvar->name);
      return;
    }
    if ( (dvarFlags & DVAR_LATCH) != 0 )
    {
      Dvar_SetLatchedValue(dvar, value);
      if ( !Dvar_ValuesEqual(dvar->type, (float *)dvar->latched.integer, (float *)dvar->current.integer) )
        Sys_Printf("%s will be changed upon restarting.\n", dvar->name);
      return;
    }
  }
  
  /* Internal source, or external/script without LATCH flag */
  {
    if ( Dvar_ValuesEqual(dvar->type, (float *)dvar->current.integer, (float *)value) )
    {
      Dvar_SetLatchedValue(dvar, dvar->current.integer);
    }
    else
    {
      dvar_modifiedFlags |= dvar->flags;
      { float *valueVec = (float *)value;
      switch ( dvar->type )
      {
        case DVAR_TYPE_VEC2:
          cur = dvar->current.vector;
          lat = dvar->latched.vector;
          cur[0] = valueVec[0]; cur[1] = valueVec[1];
          lat[0] = valueVec[0]; lat[1] = valueVec[1];
          dvar->modified = 1;
          break;
        case DVAR_TYPE_VEC3:
          cur = dvar->current.vector;
          lat = dvar->latched.vector;
          VectorCopy( valueVec, cur );
          VectorCopy( valueVec, lat );
          dvar->modified = 1;
          break;
        case DVAR_TYPE_VEC4:
          Vector4Copy((float *)value, dvar->current.vector);
          Vector4Copy((float *)value, dvar->latched.vector);
          dvar->modified = 1;
          break;
        case DVAR_TYPE_STRING:
          Assert( dvar->name, s_assertDisable_Dvar_SetVariant );
          Assert(!((const char *)value == dvar->current.string && (const char *)value != dvar->latched.string && (const char *)value != dvar->reset.string), s_assertDisable_Dvar_SetVariant);
          Dvar_FreeCurrentValue(dvar);
          Dvar_AssignCurrentStringValue((const char *)value, dvar);
          Dvar_FreeLatchedValue(dvar);
          dvar->latched.string = dvar->current.string;
          dvar->modified = 1;
          break;
        default:
          dvar->current.integer = value;
          dvar->latched.integer = value;
          dvar->modified = 1;
          break;
      }
      }
    }
  }
}

/*
================
Dvar_GetUnpackedColor

Unpacks a dvar's color value (4 packed bytes) into 4 floats in [0,1] range.
================
*/
void Dvar_GetUnpackedColor( Dvar_t *dvar, float *expandedColor )
{
  unsigned char rgba[4];

  Assert( dvar, s_assertDisable_Dvar_GetUnpackedColor );
  Assert(!(dvar->type != DVAR_TYPE_COLOR && (dvar->type != DVAR_TYPE_STRING || (dvar->flags & DVAR_EXTERNAL) == 0)), s_assertDisable_Dvar_GetUnpackedColor);

  if ( dvar->type == DVAR_TYPE_COLOR )
    memcpy(rgba, &dvar->current.integer, 4);
  else
    Dvar_StringToColor(rgba, (const char *)dvar->current.string);

  expandedColor[0] = (float)(rgba[0] * (1.0 / 255.0));
  expandedColor[1] = (float)(rgba[1] * (1.0 / 255.0));
  expandedColor[2] = (float)(rgba[2] * (1.0 / 255.0));
  expandedColor[3] = (float)(rgba[3] * (1.0 / 255.0));
}

/*
================
Dvar_PerformUnregistration

Converts a dvar to STRING type and marks it as DVAR_EXTERNAL.
================
*/
void Dvar_PerformUnregistration(Dvar_t *dvar)
{
  unsigned short dvarFlags;
  char type;
  char *latchedStr;
  char *copiedStr;
  char *resetStr;
  void *oldVecMem;

  Assert(dvar, s_assertDisable_Dvar_PerformUnregistration);
  dvarFlags = dvar->flags;
  if ( (dvarFlags & DVAR_EXTERNAL) == 0 )
  {
    dvar->flags = dvarFlags | DVAR_EXTERNAL;
    dvar->name = CopyStringHunk(dvar->name);
  }
  type = dvar->type;
  if ( type != DVAR_TYPE_STRING )
  {
    if ( type == DVAR_TYPE_VEC2 || type == DVAR_TYPE_VEC3 || type == DVAR_TYPE_VEC4 )
      oldVecMem = (void *)dvar->current.vector;
    else
      oldVecMem = 0;
    latchedStr = Dvar_DisplayableLatchedValue(dvar);
    copiedStr = Dvar_CopyString(latchedStr);
    dvar->current.string = copiedStr;
    dvar->latched.string = copiedStr;
    resetStr = Dvar_DisplayableResetValue(dvar);
    Dvar_AssignResetStringValue(resetStr, dvar);
    dvar->type = DVAR_TYPE_STRING;
    if ( oldVecMem )
      Z_Free(oldVecMem);
  }
}

/*
================
Dvar_ClearModifiedFlags

Clears a system flag from a dvar, unregistering it if no system flags remain.
================
*/
void Dvar_ClearModifiedFlags( Dvar_t *dvar, int sysFlag )
{
  unsigned short dvarFlags;

  Assert(dvar, s_assertDisable_Dvar_ClearModifiedFlags);
  if ( (dvar->flags & DVAR_EXTERNAL) == 0 )
  {
    #define DVAR_SYS_FLAG  0x2000  /* (1 << 13) */
    #define DVAR_SYS_MASK  0x7000  /* (1 << 12) | (1 << 13) | (1 << 14) */
    Assert(!(sysFlag != DVAR_SYS_FLAG), s_assertDisable_Dvar_ClearModifiedFlags);
    dvarFlags = dvar->flags;
    Assert(!((dvarFlags & sysFlag) == 0), s_assertDisable_Dvar_ClearModifiedFlags);
    dvarFlags = dvar->flags;
    Assert(!((dvarFlags & DVAR_EXTERNAL) != 0), s_assertDisable_Dvar_ClearModifiedFlags);
    dvar->flags &= ~(unsigned short)sysFlag;
    if ( (dvar->flags & DVAR_SYS_MASK) == 0 )
      Dvar_PerformUnregistration(dvar);
  }
}

/*
================
Dvar_SetResetValue

Sets the reset value of a dvar, handling vector and string types specially.
================
*/
char *Dvar_SetResetValue(Dvar_t *dvar, intptr_t value)
{
  float *src = (float *)value;

  Assert(dvar, s_assertDisable_Dvar_SetResetValue);
  switch ( dvar->type )
  {
    case DVAR_TYPE_VEC2:
      dvar->reset.vector[0] = src[0];
      dvar->reset.vector[1] = src[1];
      break;
    case DVAR_TYPE_VEC3:
      VectorCopy(src, dvar->reset.vector);
      break;
    case DVAR_TYPE_VEC4:
      Vector4Copy(src, dvar->reset.vector);
      break;
    case DVAR_TYPE_STRING:
      Dvar_FreeResetValue(dvar);
      Dvar_AssignResetStringValue((const char *)value, dvar);
      break;
    default:
      dvar->reset.integer = value;
      break;
  }
  return (char *)value;
}

/*
================
Dvar_AssignCurrentAndResetValues

Sets both current and latched values of a dvar simultaneously.
================
*/
char *Dvar_AssignCurrentAndResetValues(Dvar_t *dvar, intptr_t value)
{

  { float *src = (float *)value;
  switch ( dvar->type )
  {
    case DVAR_TYPE_VEC2:
      dvar->current.vector[0] = src[0]; dvar->current.vector[1] = src[1];
      dvar->latched.vector[0] = src[0]; dvar->latched.vector[1] = src[1];
      break;
    case DVAR_TYPE_VEC3:
      VectorCopy(src, dvar->current.vector);
      VectorCopy(src, dvar->latched.vector);
      break;
    case DVAR_TYPE_VEC4:
      Vector4Copy(src, dvar->current.vector);
      Vector4Copy(src, dvar->latched.vector);
      break;
    case DVAR_TYPE_STRING:
      if ( (const char *)value != dvar->current.string )
      {
        Dvar_FreeCurrentValue(dvar);
        Dvar_AssignCurrentStringValue((const char *)value, dvar);
      }
      dvar->latched.integer = value;
      break;
    default:
      dvar->current.integer = value;
      dvar->latched.integer = value;
      break;
  }
  }
  return (char *)value;
}

/*
================
Dvar_ReRegisterVariant

Re-registers a dvar with a new type, converting from string and setting up new values.
================
*/
int Dvar_ReRegisterVariant(Dvar_t *dvar, int type, unsigned short flags, intptr_t value, int domainMin, float domainMax)
{
  union { int i; float f; } udomMin;
  udomMin.i = domainMin;
  unsigned char oldType;
  float *clampedValue;
  intptr_t resetValue;
  float *resultValue;
  unsigned char newType;
  int *vecMem;
  int vecSize;
  int *vecLatched;
  int result;

  oldType = dvar->type;
  Assert(!(oldType != DVAR_TYPE_STRING), s_assertDisable_Dvar_ReRegisterVariant);
  dvar->type = (unsigned char)type;
  dvar->domain.integer.min = domainMin;
  dvar->domain.value.max = domainMax;
  if ( (flags & DVAR_ROM) != 0 || (flags & DVAR_CHEAT) != 0 && dvar_cheats && !dvar_cheats->current.enabled )
  {
    resetValue = value;
    resultValue = (float *)value;
  }
  else
  {
    clampedValue = (float *)Dvar_StringToValue((char *)dvar->current.string, type, domainMin);
    resetValue = value;
    resultValue = Dvar_ClampValueToDomain((unsigned char)type, clampedValue, value, udomMin.f, domainMax);
  }
  if ( dvar->type != DVAR_TYPE_STRING )
    Dvar_FreeCurrentValue(dvar);
  Dvar_FreeLatchedValue(dvar);
  Dvar_FreeResetValue(dvar);
  newType = dvar->type;
  if ( newType == DVAR_TYPE_VEC2 || newType == DVAR_TYPE_VEC3 || newType == DVAR_TYPE_VEC4 )
  {
    vecMem = Z_Malloc( 3 * sizeof(int) * newType );
    vecSize = dvar->type;
    dvar->current.integer = (intptr_t)vecMem;
    vecLatched = &vecMem[vecSize];
    dvar->latched.integer = (intptr_t)vecLatched;
    dvar->reset.integer = (intptr_t)&vecLatched[vecSize];
  }
  Dvar_SetResetValue(dvar, resetValue);
  Dvar_AssignCurrentAndResetValues(dvar, (intptr_t)resultValue);
  result = flags | dvar_modifiedFlags;
  dvar_modifiedFlags = result;
  return result;
}

/*
================
Dvar_ReRegister

Re-registers an external dvar with new type and flags, converting from string storage.
================
*/
unsigned short Dvar_ReRegister(Dvar_t *dvar, const char *dvarName, int type, unsigned short flags, char *value, int domainMin, float domainMax)
{
  unsigned short result;
  short dvarFlags;
  char *resetValue;

  result = dvar->flags;
  if ( (result & DVAR_EXTERNAL) != 0 && (flags & DVAR_EXTERNAL) == 0 )
  {
    Assert(!((result & DVAR_SYS_MASK) != DVAR_EXTERNAL), s_assertDisable_Dvar_ReRegisterVariant);
    dvarFlags = dvar->flags;
    if ( dvarFlags >= 0 || (flags & DVAR_SERVERINFO_NOUPDATE) == 0 || (dvarFlags & (DVAR_INIT | DVAR_ROM | DVAR_CHEAT)) != 0 )
      resetValue = value;
    else
      resetValue = Dvar_StringToValue((char *)dvar->reset.string, type, domainMin);
    Dvar_PerformUnregistration(dvar);
    Z_Free((void *)dvar->name);
    dvar->flags &= ~DVAR_EXTERNAL;
    dvar->name = dvarName;
    return Dvar_ReRegisterVariant(dvar, type, flags, (intptr_t)resetValue, domainMin, domainMax);
  }
  return result;
}

/*
================
Dvar_RegisterVariant

Registers or updates an existing dvar with the given type, flags, and reset value.
================
*/
void Dvar_RegisterVariant(Dvar_t *dvar, const char *dvarName, int type, unsigned short flags, char *resetValue, int domainMin, float domainMax)
{
  int ar;
  unsigned char dvarType;
  char *msg;
  bool isEnum;
  unsigned char curType;
  float *resetPtr;
  char *resetDisplay;
  char *valueStr;

  Assert(dvar, s_assertDisable_Dvar_RegisterVariant);
  Assert(dvarName, s_assertDisable_Dvar_RegisterVariant);
  dvarType = dvar->type;
  Assert(dvarType == (unsigned char)type || (dvar->flags & DVAR_EXTERNAL), s_assertDisable_Dvar_RegisterVariant);
  if ( (((flags >> 8) ^ (dvar->flags >> 8)) & (DVAR_INIT | DVAR_LATCH | DVAR_ROM)) != 0 )
  {
    Dvar_ReRegister(dvar, dvarName, type, flags, resetValue, domainMin, domainMax);
    if ( (flags & DVAR_EXTERNAL) == 0 && (flags & DVAR_CHANGEABLE_RESET) != 0 && (dvar->flags & DVAR_CHANGEABLE_RESET) == 0 )
    {
      isEnum = dvar->type == DVAR_TYPE_ENUM;
      dvar->name = dvarName;
      if ( isEnum )
      {
        dvar->domain.integer.min = domainMin;
        dvar->domain.value.max = domainMax;
      }
    }
  }
  if ( (dvar->flags & DVAR_EXTERNAL) == 0 || (curType = dvar->type, curType == (unsigned char)type) )
  {
    resetPtr = (float *)resetValue;
  }
  else
  {
    Assert(curType == DVAR_TYPE_STRING, s_assertDisable_Dvar_RegisterVariant);
    resetPtr = (float *)resetValue;
    Dvar_ReRegisterVariant(dvar, type, flags, (intptr_t)resetValue, domainMin, domainMax);
  }
  Assert(dvar->type == (unsigned char)type, s_assertDisable_Dvar_RegisterVariant);
  #define DVAR_SKIP_RESET_CHECK  0x8600  /* CHANGEABLE_RESET | SAVED | AUTOEXEC */
  Assert((dvar->flags & DVAR_SKIP_RESET_CHECK) || Dvar_ValuesEqual((unsigned char)type, (float *)dvar->reset.vector, resetPtr), s_assertDisable_Dvar_RegisterVariant);
  dvar->flags |= flags;
  if ( (dvar->flags & DVAR_CHEAT) && dvar_cheats && !dvar_cheats->current.enabled )
  {
    Dvar_SetVariant(dvar, dvar->reset.integer, DVAR_SOURCE_INTERNAL);
    Dvar_SetLatchedValue(dvar, dvar->reset.integer);
  }
  if ( (dvar->flags & DVAR_LATCH) != 0 )
    Dvar_SetVariant(dvar, dvar->latched.integer, DVAR_SOURCE_INTERNAL);
}

/*
================
Dvar_RegisterNew

Allocates a new dvar from the pool and inserts it into the sorted list and hash table.
================
*/
Dvar_t *Dvar_RegisterNew(char type, char *dvarName, short flags, intptr_t value, int domainMin, int domainMax)
{
  int idx, hashIdx;
  Dvar_t *dvar;
  char *copyStr;
  intptr_t prevHash;

  idx = dvarCount;
  if ( dvarCount >= MAX_DVARS )
  {
    Com_ErrorLevel(0, "Can't create dvar '%s': %i dvars already exist", dvarName, MAX_DVARS);
    idx = dvarCount;
  }
  dvarCount = idx + 1;
  dvar = &dvarPool[idx];
  dvar->type = type;
  if ( (flags & DVAR_EXTERNAL) != 0 )
    dvar->name = CopyStringHunk(dvarName);
  else
    dvar->name = dvarName;
  { float *src = (float *)value;
  int k;
  switch ( type )
  {
    case DVAR_TYPE_VEC2:
    case DVAR_TYPE_VEC3:
    case DVAR_TYPE_VEC4:
      Dvar_MakeExplicitType(dvar);
      for ( k = 0; k < type; k++ )
      {
        dvar->current.vector[k] = src[k];
        dvar->latched.vector[k] = src[k];
        dvar->reset.vector[k] = src[k];
      }
      break;
    case DVAR_TYPE_STRING:
      copyStr = Dvar_CopyString((const char *)value);
      dvar->current.string = copyStr;
      dvar->latched.string = copyStr;
      dvar->reset.string = copyStr;
      break;
    default:
      dvar->current.integer = value;
      dvar->latched.integer = value;
      dvar->reset.integer = value;
      break;
  }
  }
  dvar->domain.integer.min = domainMin;
  dvar->domain.integer.max = domainMax;
  dvar->modified = 0;

  /* insert into sorted linked list */
  { Dvar_t **insertPos = (Dvar_t **)&sortedDvars;
  while ( *insertPos && _stricmp(dvar->name, (*insertPos)->name) >= 0 )
    insertPos = &(*insertPos)->next;
  dvar->next = *insertPos;
  *insertPos = dvar; }
  dvar->flags = flags;
  hashIdx = Dvar_GenerateHashValue(dvarName);
  prevHash = dvarHashTable[hashIdx];
  dvarHashTable[hashIdx] = (intptr_t)dvar;
  dvar->hashNext = (Dvar_t *)prevHash;
  return dvar;
}

/*
================
Dvar_FindMalleableVar

Finds an existing dvar by name or registers a new one with the given parameters.
================
*/
Dvar_t *Dvar_FindMalleableVar(short flags, char *dvarName, int type, char *value, int domainMin, int domainMax)
{
  union { int i; float f; } udomMin, udomMax;
  udomMin.i = domainMin; udomMax.i = domainMax;
  char *msg;
  int ar;
  Dvar_t *dvar;

  Assert((flags & DVAR_SYS_MASK) != 0, s_assertDisable_Dvar_FindMalleableVar);
  Assert((flags & DVAR_EXTERNAL) || CanKeepStringPointer((uintptr_t)dvarName), s_assertDisable_Dvar_FindMalleableVar);
  dvar = (Dvar_t *)dvarHashTable[Dvar_GenerateHashValue(dvarName)];
  if ( !dvar ) {
    return Dvar_RegisterNew((signed char)type, dvarName, flags, (intptr_t)value, domainMin, domainMax);
  }
  while ( Q_stricmp(dvarName, dvar->name) )
  {
    dvar = dvar->hashNext;
    if ( !dvar )
      return Dvar_RegisterNew((signed char)type, dvarName, flags, (intptr_t)value, domainMin, domainMax);
  }
  Dvar_RegisterVariant(dvar, dvarName, type, flags, value, domainMin, udomMax.f);
  return dvar;
}

/*
================
Dvar_RegisterBool

Registers a boolean dvar with the given name, default value, and flags.
================
*/
Dvar_t *Dvar_RegisterBool(const char *dvarName, char *value, short flags)
{
  return Dvar_FindMalleableVar(flags, (char *)dvarName, DVAR_TYPE_BOOL, value, 0, 0);
}

/*
================
Dvar_RegisterInt

Registers an integer dvar with the given name, default value, min/max domain, and flags.
================
*/
Dvar_t *Dvar_RegisterInt(const char *dvarName, char *value, int min, int max, short flags)
{
  return Dvar_FindMalleableVar(flags, (char *)dvarName, DVAR_TYPE_INT, value, min, max);
}

/*
================
Dvar_RegisterString

Registers a string dvar with the given name, default value, and flags.
================
*/
Dvar_t *Dvar_RegisterString(const char *dvarName, const char *value, short flags)
{
  Assert(dvarName, s_assertDisable_Dvar_RegisterString);
  Assert(value, s_assertDisable_Dvar_RegisterString);
  Assert((flags & DVAR_EXTERNAL) || CanKeepStringPointer((uintptr_t)value), s_assertDisable_Dvar_RegisterString);
  return Dvar_FindMalleableVar(flags, (char *)dvarName, DVAR_TYPE_STRING, (char *)value, 0, 0);
}

/*
================
Dvar_SetBool

Sets a boolean dvar's value from the specified source.
================
*/
void Dvar_SetBool(Dvar_t *dvar, char value, DvarSetSource_t source)
{
  Assert(dvar, s_assertDisable_Dvar_SetBool);
  Assert(dvar->name, s_assertDisable_Dvar_SetBool);
  Assert(!(dvar->type && (dvar->type != DVAR_TYPE_STRING || (dvar->flags & DVAR_EXTERNAL) == 0)), s_assertDisable_Dvar_SetBool);
  if ( dvar->type )
  {
    void *boolStr = value ? g_dvarString1 : g_dvarString0;
    Dvar_SetVariant(dvar, (intptr_t)boolStr, source);
  }
  else
  {
    Dvar_SetVariant(dvar, (intptr_t)(unsigned char)value, source);
  }
}

/*
================
Dvar_SetInt

Sets an integer dvar's value from the specified source.
================
*/
void Dvar_SetInt( Dvar_t *dvar, char *value, DvarSetSource_t source )
{
  char *valuePtr;
  char stringBuf[32];

  Assert( dvar, s_assertDisable_Dvar_SetInt );
  Assert( dvar->name, s_assertDisable_Dvar_SetInt );
  Assert(!(dvar->type != DVAR_TYPE_INT && dvar->type != DVAR_TYPE_ENUM && (dvar->type != DVAR_TYPE_STRING || (dvar->flags & DVAR_EXTERNAL) == 0)), s_assertDisable_Dvar_SetInt);

  if ( dvar->type == DVAR_TYPE_INT || dvar->type == DVAR_TYPE_ENUM )
    valuePtr = value;
  else
  {
    Com_sprintf( stringBuf, sizeof(stringBuf), "%i", value );
    valuePtr = stringBuf;
  }
  Dvar_SetVariant( dvar, (intptr_t)valuePtr, source );
}

/*
================
Dvar_SetFloat

Sets a float dvar's value from the specified source.
================
*/
void Dvar_SetFloat( Dvar_t *dvar, float value, DvarSetSource_t source )
{
  char *valuePtr;
  char stringBuf[32];

  Assert( dvar, s_assertDisable_Dvar_SetFloat );
  Assert( dvar->name, s_assertDisable_Dvar_SetFloat );
  Assert(!(dvar->type != DVAR_TYPE_FLOAT && (dvar->type != DVAR_TYPE_STRING || (dvar->flags & DVAR_EXTERNAL) == 0)), s_assertDisable_Dvar_SetFloat);

  if ( dvar->type == DVAR_TYPE_FLOAT )
  {
    union { float f; int i; } u; u.f = value;
    valuePtr = (char *)(intptr_t)u.i;  /* raw float bits as DvarValue */
  }
  else
  {
    Com_sprintf( stringBuf, sizeof(stringBuf), "%g", value );
    valuePtr = stringBuf;
  }
  Dvar_SetVariant( dvar, (intptr_t)valuePtr, source );
}

/*
================
Dvar_SetVec2

Sets a vec2 dvar's value from the specified source.
================
*/
void Dvar_SetVec2( Dvar_t *dvar, float x, float y, DvarSetSource_t source )
{
  char *valuePtr;
  float vecBuf[2];
  char stringBuf[MAX_QPATH];

  Assert( dvar, s_assertDisable_Dvar_SetVec2 );
  Assert( dvar->name, s_assertDisable_Dvar_SetVec2 );
  Assert(!(dvar->type != DVAR_TYPE_VEC4 && (dvar->type != DVAR_TYPE_STRING || (dvar->flags & DVAR_EXTERNAL) == 0)), s_assertDisable_Dvar_SetVec2);
  if ( dvar->type == DVAR_TYPE_VEC4 )
  {
    vecBuf[0] = x;
    vecBuf[1] = y;
    valuePtr = (char *)vecBuf;
  }
  else
  {
    Com_sprintf( stringBuf, sizeof(stringBuf), "%g %g", x, y );
    valuePtr = stringBuf;
  }
  Dvar_SetVariant(dvar, (intptr_t)valuePtr, source);
}

/*
================
Dvar_SetVec3

Sets a vec3 dvar's value from the specified source.
================
*/
void Dvar_SetVec3( Dvar_t *dvar, float x, float y, float z, DvarSetSource_t source )
{
  char *valuePtr;
  float vecBuf[3];
  char stringBuf[96];

  Assert( dvar, s_assertDisable_Dvar_SetVec3 );
  Assert( dvar->name, s_assertDisable_Dvar_SetVec3 );
  Assert(!(dvar->type != DVAR_TYPE_VEC3 && (dvar->type != DVAR_TYPE_STRING || (dvar->flags & DVAR_EXTERNAL) == 0)), s_assertDisable_Dvar_SetVec3);

  if ( dvar->type == DVAR_TYPE_VEC3 )
  {
    vecBuf[0] = x;
    vecBuf[1] = y;
    vecBuf[2] = z;
    valuePtr = (char *)vecBuf;
  }
  else
  {
    Com_sprintf( stringBuf, sizeof(stringBuf), FMT_VECTOR3, x, y, z );
    valuePtr = stringBuf;
  }
  Dvar_SetVariant( dvar, (intptr_t)valuePtr, source );
}

/*
================
Dvar_SetVec4

Sets a vec4 dvar's value from the specified source.
================
*/
void Dvar_SetVec4( Dvar_t *dvar, float x, float y, float z, float w, DvarSetSource_t source )
{
  char *valuePtr;
  float vecBuf[4];
  char stringBuf[128];

  Assert( dvar, s_assertDisable_Dvar_SetVec4 );
  Assert( dvar->name, s_assertDisable_Dvar_SetVec4 );
  Assert(!(dvar->type != DVAR_TYPE_VEC4 && (dvar->type != DVAR_TYPE_STRING || (dvar->flags & DVAR_EXTERNAL) == 0)), s_assertDisable_Dvar_SetVec4);

  if ( dvar->type == DVAR_TYPE_VEC4 )
  {
    vecBuf[0] = x;
    vecBuf[1] = y;
    vecBuf[2] = z;
    vecBuf[3] = w;
    valuePtr = (char *)vecBuf;
  }
  else
  {
    Com_sprintf( stringBuf, sizeof(stringBuf), "%g %g %g %g", x, y, z, w );
    valuePtr = stringBuf;
  }
  Dvar_SetVariant( dvar, (intptr_t)valuePtr, source );
}

/*
================
Dvar_SetString

Sets a string or enum dvar's value from the specified source.
================
*/
void Dvar_SetString(char *string, Dvar_t *dvar, DvarSetSource_t source)
{
  int ar;
  char dvarType;
  char *msg;
  char *newValue;
  char stringBuf[MAX_OS_PATH];

  Assert(dvar, s_assertDisable_Dvar_SetString);
  Assert(dvar->name, s_assertDisable_Dvar_SetString);
  dvarType = dvar->type;
  Assert(dvarType == DVAR_TYPE_STRING || dvarType == DVAR_TYPE_ENUM, s_assertDisable_Dvar_SetString);
  Assert(string, s_assertDisable_Dvar_SetString);
  if ( dvar->type == DVAR_TYPE_STRING )
  {
    I_strncpyz( stringBuf, string, sizeof(stringBuf) );
    newValue = stringBuf;
  }
  else
  {
    newValue = (char *)(intptr_t)Dvar_StringToEnum(&dvar->domain, string);
    Assert(newValue != (char *)DVAR_INVALID_ENUM_INDEX, s_assertDisable_Dvar_SetString);
  }
  Dvar_SetVariant(dvar, (intptr_t)newValue, source);
}

/*
================
Dvar_SetColor

Sets a color dvar's value from RGBA floats, packing into 4 bytes.
================
*/
void Dvar_SetColor(Dvar_t *dvar, float r, float g, float b, float a, DvarSetSource_t source)
{
  char *valuePtr;
  char stringBuf[128];

  Assert(dvar, s_assertDisable_Dvar_SetColor);
  Assert(dvar->name, s_assertDisable_Dvar_SetColor);
  Assert(dvar->type == DVAR_TYPE_COLOR || (dvar->type == DVAR_TYPE_STRING && (dvar->flags & DVAR_EXTERNAL)), s_assertDisable_Dvar_SetColor);
  if ( dvar->type == DVAR_TYPE_COLOR )
  {
    float rgba[4] = { r, g, b, a };
    unsigned char bytes[4];
    int i, packed;
    double clamped;

    for ( i = 0; i < 4; i++ )
    {
      clamped = rgba[i];
      if ( clamped > 1.0 ) clamped = 1.0;
      if ( clamped < 0.0 ) clamped = 0.0;
      bytes[i] = (unsigned char)xs_RoundToInt((float)(clamped * 255.0) + FISTP_BIAS);
    }
    packed = bytes[0] | (bytes[1] << 8) | (bytes[2] << 16) | (bytes[3] << 24);
    valuePtr = (char *)(intptr_t)packed;
  }
  else
  {
    Com_sprintf( stringBuf, sizeof(stringBuf), "%g %g %g %g", r, g, b, a );
    valuePtr = stringBuf;
  }
  Dvar_SetVariant(dvar, (intptr_t)valuePtr, source);
}

/*
================
Dvar_SetStringInternal

Sets a string dvar's value internally (convenience wrapper).
================
*/
void Dvar_SetStringInternal(Dvar_t *dvar, char *string)
{
  Dvar_SetString(string, dvar, DVAR_SOURCE_INTERNAL);
}

/*
================
Dvar_SetFromString

Parses a string into the dvar's type and sets the value from the specified source.
================
*/
void Dvar_SetFromString(Dvar_t *dvar, char *string, DvarSetSource_t source)
{
  char *parsedValue;
  char stringBuf[MAX_OS_PATH];

  I_strncpyz( stringBuf, string, sizeof(stringBuf) );
  parsedValue = Dvar_StringToValue(stringBuf, dvar->type, dvar->domain.integer.min);
  if ( dvar->type == DVAR_TYPE_ENUM && parsedValue == (char *)DVAR_INVALID_ENUM_INDEX )
  {
    Sys_Printf("'%s' is not a valid value for dvar '%s'\n", stringBuf, dvar->name);
    Dvar_PrintDomain(dvar->type, *(long long *)&dvar->domain);
    parsedValue = (char *)dvar->reset.integer;
  }
  Dvar_SetVariant(dvar, (intptr_t)parsedValue, source);
}

/*
================
Dvar_SetStringByName

Sets a string dvar by name, registering it as external if not found.
================
*/
void Dvar_SetStringByName(const char *dvarName, char *string)
{
  Dvar_t *dvar;

  dvar = (Dvar_t *)Dvar_FindVar((char *)dvarName);
  if ( dvar )
    Dvar_SetString(string, dvar, DVAR_SOURCE_INTERNAL);
  else
    Dvar_RegisterString(dvarName, string, DVAR_EXTERNAL);
}

/*
================
Dvar_Init

Initializes the dvar system and registers the sv_cheats dvar.
================
*/
Dvar_t *Dvar_Init(void)
{
  isDvarSystemActive = 1;
  #define DVAR_FLAGS_SV_CHEATS 4120  /* DVAR_ROM | DVAR_SYS */
  dvar_cheats = Dvar_FindMalleableVar( DVAR_FLAGS_SV_CHEATS, "sv_cheats", DVAR_TYPE_BOOL, NULL, 0, 0 );
  return dvar_cheats;
}

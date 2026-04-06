/*
targetplatform.c — Platform selection

Reconstructed from cod2map.exe by Rose.
*/

#include "cod2map.h"

static const char* platform_name_pc = "pc";
static const char* platform_name_xenon = "xenon";
static const char* material_dir_pc = "materials";
static const char* material_dir_xenon = "materials";

PlatformEntry_t platformTable[2] = {
    { 0, "xenon", 0, "xenon", "materials" },   /* Xbox 360 - bigEndian=0 (needs swap) */
    { 1, "pc",    1, "pc",    "materials" }     /* PC - bigEndian=1 (no swap needed)   */
};

PlatformEntry_t *g_sourcePlatform;

char s_assertDisable_SetTargetPlatformByName;
char s_assertDisable_tpBoundsCheck;
char s_assertDisable_tpTableMatch;


/*
================
GetPlatformById

Looks up platform entry by numeric ID.
================
*/
PlatformEntry_t *GetPlatformById(unsigned int platformId)
{

  Assert(platformId < PLATFORM_COUNT, s_assertDisable_tpBoundsCheck);
  Assert(platformTable[platformId].platformId == (int)platformId, s_assertDisable_tpTableMatch);

  return &platformTable[platformId];
}

/*
================
SetTargetPlatformByName

Sets the active target platform by name string.
================
*/
int SetTargetPlatformByName(char *platformName)
{
  int i;

  Assert(platformName, s_assertDisable_SetTargetPlatformByName);

  for (i = 0; i < PLATFORM_COUNT; i++)
  {
    if (_stricmp(platformName, platformTable[i].name) == 0)
    {
      if (!g_targetPlatform || g_targetPlatform == &platformTable[i])
      {
        g_targetPlatform = &platformTable[i];
        return 1;
      }
      else
      {
        Com_Error(
          "Error: Target platform already set as %s, trying to set again as %s\n",
          g_targetPlatform->name,
          platformTable[i].name);
        return 0;
      }
    }
  }
  return 0;
}

/*
================
ValidatePlatformSet

Validates that a target platform has been set.
Prints available platforms if none is set.
================
*/
int ValidatePlatformSet(void)
{
  int i;

  if ( g_targetPlatform )
    return 1;
  printf("No platform specified.  '-platform' must be set using one of the following:\n");
  for (i = 0; i < PLATFORM_COUNT; i++)
  {
    printf("  %s\n", platformTable[i].name);
  }
  return 0;
}

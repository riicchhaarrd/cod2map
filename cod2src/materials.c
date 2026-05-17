/*
materials.c — Material loading and parsing

Reconstructed from cod2map.exe by Rose.
*/

#include "cod2map.h"

MatExpandRegion_t g_matExpandRegion;


/*
================
FindMaterialInCache

Looks up a material name in the shader cache
================
*/
ShaderInfo_t *FindMaterialInCache(const char *materialName)
{
  int i;

  for ( i = 0; i < g_matExpandRegion.materialCount; i++ )
  {
    if ( !strcmp(materialName, g_matExpandRegion.materialCache[i].name) )
      return &g_matExpandRegion.materialCache[i];
  }
  return NULL;
}

/*
================
Material_ByteSwap

Byte-swaps material file data for platform endianness.
Converts all multi-byte fields from big-endian to little-endian.
================
*/
int Material_ByteSwap(char *data)
{
  MatFileHeader_t *mat;

  Assert(data, g_matExpandRegion.assertFlag_info);

  Swap_Init();

  mat = (MatFileHeader_t *)data;

  /* swap 16-bit fields */
  mat->hashIndex    = BigShort(mat->hashIndex);
  mat->sortedIndex  = BigShort(mat->sortedIndex);
  mat->toolFlags    = BigShort(mat->toolFlags);
  mat->autoTexScaleW = BigShort(mat->autoTexScaleW);
  mat->autoTexScaleH = BigShort(mat->autoTexScaleH);

  /* swap 32-bit fields */
  mat->locale       = BigLong(mat->locale);
  { union { float f; int i; } u; u.f = mat->maxDeformMove; u.i = BigFloat(u.i); mat->maxDeformMove = u.f; }
  { union { float f; int i; } u; u.f = mat->tessSize; u.i = BigFloat(u.i); mat->tessSize = u.f; }
  mat->surfaceFlags = BigLong(mat->surfaceFlags);
  mat->contentFlags = BigLong(mat->contentFlags);

  return Swap_InitByteSwap();
}

/* material loading statistics */
static int materialsLoaded = 0;
static int materialsCached = 0;
static int materialsFailed = 0;
static char failedMaterials[MAX_TRACKED_FAILURES][MAX_QPATH];

/*
================
LoadMaterial

Loads a material by name, caching and byte-swapping as needed
================
*/
ShaderInfo_t *LoadMaterial(const char *materialName)
{
  ShaderInfo_t *result, *matEntry;
  int assertResult;
  void *fileData;
  char filePath[MAX_OS_PATH];

  /* check if already loaded in cache */
  result = FindMaterialInCache(materialName);

  if ( !result )
  {
    /* validate target platform state */
    if ( !g_targetPlatform && !g_matExpandRegion.assertFlag_platform )
    {
      assertResult = AssertHandler("targetPlatform", ".\\materials.cpp", 165, 0, 1);
      if ( assertResult )
      {
        if ( assertResult == 2 )
          g_matExpandRegion.assertFlag_platform = 1;
      }
      else
      {
        DebugBreak();
      }
    }

    if ( !g_targetPlatform->materialDirectory && !g_matExpandRegion.assertFlag_matDir0 )
    {
      assertResult = AssertHandler("targetPlatform->materialDirectory", ".\\materials.cpp", 166, 0, 1);
      if ( assertResult )
      {
        if ( assertResult == 2 )
          g_matExpandRegion.assertFlag_matDir0 = 1;
      }
      else
      {
        DebugBreak();
      }
    }

    if ( !*g_targetPlatform->materialDirectory && !g_matExpandRegion.assertFlag_matDir )
    {
      assertResult = AssertHandler("targetPlatform->materialDirectory[0]", ".\\materials.cpp", 167, 0, 1);
      if ( assertResult )
      {
        if ( assertResult == 2 )
          g_matExpandRegion.assertFlag_matDir = 1;
      }
      else
      {
        DebugBreak();
      }
    }

    /* build file path and load from IWD */
    Com_sprintf(filePath, sizeof(filePath), "%s/%s", g_targetPlatform->materialDirectory, materialName);

    int fileSize = FS_LoadFile(filePath, &fileData);
    if ( fileSize >= 0 )
    {
      /* allocate cache slot after successful load */
      matEntry = &g_matExpandRegion.materialCache[g_matExpandRegion.materialCount++];

      /* byte-swap on little-endian platforms */
      int bigEndian = g_targetPlatform->bigEndian;
      int needsSwap = !bigEndian;

      if (needsSwap) {
        Material_ByteSwap(fileData);
      }

      /* parse material header and copy properties to cache entry */
      {
        MatFileHeader_t *matData = (MatFileHeader_t *)fileData;
        unsigned short toolFlagsWord;
        unsigned char gameFlags;
        float tessSize;
        unsigned char contentByte;

        /* convert relative offsets to absolute pointers (stored in-place, truncated on 64-bit) */
        matData->nameOfs += (intptr_t)fileData;
        matData->refImageOfs += (intptr_t)fileData;

        I_strncpyz(matEntry->name, materialName, sizeof(matEntry->name));

        matEntry->surfaceFlags = matData->surfaceFlags;
        matEntry->contentFlags = matData->contentFlags;
        toolFlagsWord = matData->toolFlags;
        gameFlags = matData->gameFlags;
        tessSize = matData->tessSize;
        matEntry->toolFlagsWord = toolFlagsWord;
        matEntry->unused76 = (gameFlags >> 1) & 1;
        matEntry->unused80 = toolFlagsWord & 1;

        /* use default tessellation size if material specifies zero */
        if (tessSize == 0.0f) {
          matEntry->subdivisions = g_matExpandRegion.defaultTessSize;
        } else {
          matEntry->subdivisions = tessSize;
        }

        matEntry->reserved92 = 0;
        matEntry->surfFlags_bit7 = (matEntry->surfaceFlags >> 7) & 1;
        matEntry->globalTexture = (toolFlagsWord & TOOL_GLOBAL_TEXTURE) ? 1 : 0;

        /* check for non-colliding content or tool type */
        contentByte = (unsigned char)matData->contentFlags;
        if ((contentByte & CONTENTS_NONCOLLIDING) || ((toolFlagsWord & TOOL_TYPE_MASK) == TOOL_TYPE_NONCOLLIDE)) {
          matEntry->unused100 = 1;
        } else {
          matEntry->unused100 = 0;
        }

        matEntry->autoTexScaleW = matData->autoTexScaleW;
        matEntry->autoTexScaleH = matData->autoTexScaleH;
        matEntry->reserved120 = 0;
      }

      FS_FreeFile(fileData);
      materialsLoaded++;
      return matEntry;
    }
    else
    {
      /* track failed material loads */
      if (materialsFailed < MAX_TRACKED_FAILURES) {
        strncpy(failedMaterials[materialsFailed], materialName ? materialName : "(null)", sizeof(failedMaterials[0]) - 1);
        failedMaterials[materialsFailed][sizeof(failedMaterials[0]) - 1] = 0;
      }
      materialsFailed++;

      if ( !strcmp(materialName, "$default") )
        Com_Error("Cannot find material '$default'");

      /* fall back to $default material */
      return LoadMaterial("$default");
    }
  }

  materialsCached++;
  return result;
}
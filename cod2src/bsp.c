/*
bsp.c — BSP compilation pipeline

Reconstructed from cod2map.exe by Rose.

Main entry point and compilation pipeline: parses arguments, loads map,
builds BSP tree, emits brushes/surfaces/portals/collision, writes d3dbsp.
*/

#include "cod2map.h"

unsigned char _memory_placeholder[256];
unsigned char _stack_placeholder[0x10000];

OptionEntry_t optionsTable[] = {
    { "-platform",           "Required if not byteswapping; specifies target platform",   (OptionHandler)HandlePlatformOption   },
    { "-pcToXenon",          "Converts a PC bsp into a Xenon bsp",                        (OptionHandler)Opt_PcToXenon          },
    { "-xenonToPc",          "Converts a Xenon bsp into a PC bsp",                        (OptionHandler)Opt_XenonToPc          },
    { "-v",                  "Verbose; enables extra compilation messages",               (OptionHandler)Opt_Verbose            }, 
    { "-verboseEntities",    "Includes verbose messages for submodels if '-v' is given",  (OptionHandler)Opt_VerboseEntities    }, 
    { "-onlyEnts",           "Compile doesn't touch triggers, geometry, or lighting",     (OptionHandler)Opt_OnlyEnts           }, 
    { "-sampleScale",        "Scales all lightmaps; 2 doubles pixel size, 0.5 halves it", (OptionHandler)Opt_sampleScale        },
    { "-blockSize",          "Grid size for regular BSP splits; 0 uses largest possible", (OptionHandler)Opt_blockSize          }, 
    { "-subdivisions",       "Divides all geometry on a grid; only works for small maps", (OptionHandler)Opt_subdivisions       }, 
    { "-noSubdivide",        "Ignores the 'tessSize' setting in all materials",           (OptionHandler)Opt_NoSubdivide        }, 
    { "-brushSmoothAngle",   "Smooth lighting at brush edges at angles less than this",   (OptionHandler)Opt_brushSmoothAngle   }, 
    { "-curveSmoothAngle",   "Smooth lighting at curve edges at angles less than this",   (OptionHandler)Opt_curveSmoothAngle   }, 
    { "-terrainSmoothAngle", "Smooth lighting at terrain edges at angles less than this", (OptionHandler)Opt_terrainSmoothAngle }, 
    { "-loadFrom",           "Reads this map instead, but still writes to <mapfile>",     (OptionHandler)Opt_loadFrom           }, 
    { "-noWater",            "Ignores all water brushes",                                 (OptionHandler)Opt_NoWater            }, 
    { "-noCurves",           "Ignores all patch and terrain geometry",                    (OptionHandler)Opt_NoCurves           }, 
    { "-noDetail",           "Ignores all detail brushes",                                (OptionHandler)Opt_NoDetail           }, 
    { "-fullDetail",         "Turns all detail brushes into structural brushes",          (OptionHandler)Opt_FullDetail         }, 
    { "-leakTest",           "Quits immediately if the map leaked",                       (OptionHandler)Opt_LeakTest           }, 
    { "-brushMethod",        "Brush optimization method (players/bullets/all/none)",      (OptionHandler)Opt_brushMethod        },  
    { "-expandPlayer",       "Writes a map for Radiant to see player-to-brush collision", (OptionHandler)Opt_ExpandPlayer       }, 
    { "-expandBullet",       "Writes a map for Radiant to see bullet-to-brush collision", (OptionHandler)Opt_ExpandBullet       }, 
    { "-debugPortals",       "Writes a _portals.map showing portal/structural geometry",  (OptionHandler)Opt_DebugPortals       }, 
    { "-noReorderTris",      "Disables reordering of optimized triangles for T&L cache",  (OptionHandler)Opt_NoReorderTris      }, 
    { NULL, NULL, NULL }
};

char g_fmtIntColonStr[8] = { '%', 'i', ':', '%', 's', '\0', '\0', '\0' };
ContentFlag_t contentsTable[] = {
    { "weaponClip",    CONTENTS_DETAIL | CONTENTS_CLIPSHOT | CONTENTS_MISSILECLIP },
    { "nonColliding",  CONTENTS_DETAIL | CONTENTS_NONCOLLIDING },
    { "detail",        CONTENTS_DETAIL },
    { NULL, 0 }
};

ContentFlag_t toolFlagsTable[] = {
    { "splitGeo",  0x00000100 },
    { NULL, 0 }
};

char *g_scriptClassnames[7] =
{
  "info_vehicle_node",
  "info_vehicle_node_rotate",
  "script_model",
  "script_origin",
  "script_vehicle",
  "misc_prefab",
  "script_struct"
};

Brush_t  *g_buildBrush;
Entity_t *g_currentEntity = 0;
HWND      hWnd;
Plane_t  *g_planeHashTable[PLANE_HASH_SIZE];
Plane_t  *mapplanes = NULL;

int  nummapplanes_storage[4];
int *nummapplanes = &nummapplanes_storage[0];

float   g_mapMins[3];
#define mapMins g_mapMins[0]
#define mapMins_y g_mapMins[1]
#define mapMins_z g_mapMins[2]

float   g_mapMaxs[3];
#define mapMaxs g_mapMaxs[0]
#define mapMaxs_y g_mapMaxs[1]
#define mapMaxs_z g_mapMaxs[2]

int          ArgList;
float        blockSize = DEFAULT_BLOCK_SIZE;
int          brushMethod = 2;
float        brushSmoothAngle = 1.0f;
char         bspLightmapData[MAX_MAP_LIGHTBYTES];
int          byteswapMode;
float        curveSmoothAngle = 0.01f;
int          fulldetail;
int          g_currentEntityIndex;
char         g_loadFromPath[MAX_OS_PATH];
char         g_mapSourceFile[MAX_OS_PATH];
int          g_numBrushes;
int          g_numMapBrushes;
int          g_numMapBrushSides;
int          g_numMapDrawSurfs;
int          g_numMapEntities;
int          g_numMapPlanes;
int          g_numPatches;
int          g_onlyEnts;
char         g_outputBasePath[MAX_OS_PATH];
int          g_parseEntityCount;
int          g_removedBrushSides;
int          g_treeNodeCount;
int          leaktest;
unsigned int Msg = 4294967295u;
int          noCurveBrushes;
int          nodetail;
int          nosubdivide;
int          nowater;
int          numActiveBrushes;
int          numBSPPaths;
int          numLightmaps = 1;
int          numMapDrawSurfs;
int          numthreads = -1;
int          reorderTris = 1;
float        sampleScale = 1.0;
float        terrainSmoothAngle = 0.01f;
int          testExpand;
int          testlevel = 2;
int          verbose;
int          verboseEntities;

char g_assertFlags0;
char g_assertFlags1;
char g_assertFlags2;
char g_assertFlags3;
char g_assertFlags4;
char g_assertFlags5;
char g_assertFlags6;
char g_assertFlags7;
char g_assertFlags8;
char g_assertFlags9;
char g_assertFlagsA;
char g_assertFlagsB;
char g_assertFlagsC;
char g_assertFlagsD;
char g_assertFlagsE;
char g_assertFlagsF;
char s_assertDisable_BSP_SetTargetPlatform;
char s_assertDisable_BSP_SetTargetPlatform_0;
char s_assertDisable_OnlyEnts;
char s_assertDisable_ProcessBSPArguments_0;


/*
================
EmitWorldBSP

Build BSP tree for worldspawn entity (entity 0).
================
*/
int EmitWorldBSP(void)
{
  Face_t *faces;
  Tree_t *tree;
  Face_t *detailFaces;
  int leaked;

  BeginModel();
  g_entities[0].firstDrawSurf = 0;
  OpenDebugFile();
  ProcessEntityPatches(g_entities);
  PatchMapDrawSurfs(g_entities);

  faces = CollectBrushWindings(g_entities[0].brushes);
  tree = BuildBspTree(faces);
  ProcessBspTree(tree);
  FilterStructuralBrushesIntoBspTree(g_entities, tree);

  if ( FloodEntities(tree) )
  {
    /* no leak — rebuild tree with detail brushes */
    MarkVisibleSides(tree->headnode);
    ClipSidesIntoTree(g_entities, tree);
    detailFaces = CollectDetailWindings(g_entities[0].brushes);
    FreeTreeBlock(tree);
    tree = BuildBspTree(detailFaces);
    ProcessBspTree(tree);
    FilterStructuralBrushesIntoBspTree(g_entities, tree);
    leaked = 0;
  }
  else
  {
    /* leak detected */
    Com_Printf("**********************\n");
    Com_Printf("******* leaked *******\n");
    Com_Printf("**********************\n");
    LeakFile(tree);
    if ( leaktest )
    {
      Com_Printf("--- MAP LEAKED, ABORTING LEAKTEST ---\n");
      exit(0);
    }
    leaked = 1;
    ClipSidesIntoTree(g_entities, tree);
  }

  /* portalize */
  CloseDebugFile();
  PortalizeWorld(g_entities[0].brushes, tree, leaked);
  NumberClusters(tree);
  if ( !leaked )
    WritePortalFile(tree);
  FloodAreas(tree);

  /* detail brushes and draw surfaces */
  FilterDetailBrushesIntoBspTree(g_entities, tree);
  if ( !nosubdivide )
    SubdivideDrawSurfs(g_entities);
  AllocateLightmaps(g_entities);
  EmitDrawSurfaces(g_entities, tree);
  EmitLeafBrushes(g_entities, tree);
  AddBrushNeighborBevels(g_entities);

  EndModel(tree->headnode);
  FreeTreeBlock(tree);

  if ( testExpand )
    return ExpandBrushesAndWriteMap();
  return testExpand;
}

/*
================
EmitEntityBSP

Build BSP for a non-worldspawn entity (submodel).
================
*/
void EmitEntityBSP(void)
{
  Entity_t *e;
  Node_t *node;
  Brush_t *brush, *bc;
  Tree_t *tree;

  BeginModel();
  e = &g_entities[g_currentEntityIndex];
  e->firstDrawSurf = numMapDrawSurfs;
  ProcessEntityPatches(e);
  PatchMapDrawSurfs(e);

  /* copy all brushes into a single leaf node */
  node = AllocNode();
  node->planenum = PLANENUM_LEAF;
  for ( brush = e->brushes; brush; brush = brush->next )
  {
    bc = CopyBrush(brush);
    bc->next = node->leafBrushes;
    node->leafBrushes = bc;
  }

  /* emit draw surfaces */
  tree = AllocTree();
  tree->headnode = node;
  ClipSidesIntoTree(e, tree);
  if ( !nosubdivide )
    SubdivideDrawSurfs(e);
  AllocateLightmaps(e);
  EmitDrawSurfaces(e, tree);
  EmitLeafBrushes(e, tree);
  AddBrushNeighborBevels(e);

  EndModel(node);
  FreeTreeBlock(tree);
}

/*
================
EmitBSP

Emit BSP data for all entities.
================
*/
int EmitBSP(void)
{
  int entityIndex;
  int savedVerbose;
  Entity_t *ent;

  entityIndex = 0;
  savedVerbose = verbose;

  for ( g_currentEntityIndex = 0; entityIndex < num_entities; g_currentEntityIndex = entityIndex )
  {
    ent = &g_entities[entityIndex];

    /* only process entities that have geometry */
    if ( ent->brushes || ent->patches )
    {
      Com_DPrintf("############### model %i ###############\n", numBSPModels);

      if ( g_currentEntityIndex )
        EmitEntityBSP();
      else
        EmitWorldBSP();

      entityIndex = g_currentEntityIndex;
      if ( !verboseEntities )
        verbose = 0;
    }
    ++entityIndex;
  }

  verbose = savedVerbose;
  return entityIndex;
}

/*
================
Com_ErrorLevel

Error reporting with severity level.
================
*/
int Com_ErrorLevel(int errorLevel, char *fmt, ...)
{
  char buffer[MAX_STRING_CHARS];
  va_list args;

  va_start(args, fmt);
  _vsnprintf(buffer, sizeof(buffer), fmt, args);
  va_end(args);
  buffer[sizeof(buffer) - 1] = '\0';
  Com_Printf("%s", buffer);
  return 0;
}

/*
================
FS_Startup_Simple

Initializes the filesystem with default base paths.
================
*/
char *FS_Startup_Simple()
{
  return FS_InitFilesystemFromArgs(g_basePath, g_fsBaseGame, g_fsBaseGame);
}

/*
================
BSPInfo

Print BSP file statistics for one or more map files.
================
*/
int BSPInfo(int argc, const char **argv)
{
  int i;
  char *ext;
  FILE *fp;
  int fileSize;

  if ( argc < 1 )
  {
    Com_Printf("No files to dump info for.\n");
    return 0;
  }

  Swap_InitByteSwap();

  /* dump info for each bsp file */
  for ( i = 0; i < argc; i++ )
  {
    Com_Printf("---------------------\n");
    StripExtension((char *)argv[i]);
    SetBSPFileExtensions("d3d");
    I_strncpyz(g_outputBasePath, argv[i], sizeof(g_outputBasePath));
    ext = GetBSPFileExtension();
    DefaultExtension(g_outputBasePath, ext);

    /* load and print sizes */
    fp = fopen(g_outputBasePath, "rb");
    if ( fp )
    {
      fileSize = FS_FileLength(fp);
      fclose(fp);
      Com_Printf("%s: %i\n", g_outputBasePath, fileSize);
      LoadBSPFile(g_outputBasePath);
      PrintBSPFileSizes(fileSize);
      Com_Printf("---------------------\n");
    }
    else
    {
      Com_Error("no bsp files with name '%s'\n", argv[i]);
    }
  }
  return argc;
}

/*
================
OnlyEnts

Re-emit entity data without recompiling BSP geometry.
================
*/
int OnlyEnts(void)
{
  char *ext;
  char bspPath[MAX_OS_PATH];

  ext = GetBSPFileExtension();
  Com_sprintf(bspPath, sizeof(bspPath), "%s%s", g_outputBasePath, ext);

  /* load existing bsp, re-parse entities, write back */
  LoadBSPFile(bspPath);
  num_entities = 0;
  LoadMapFile(g_mapSourceFile);
  ProcessWorldBrushes();
  ProcessCoronaLights();
  UnparseEntitiesWithOrigins();
  Assert(g_targetPlatform, s_assertDisable_OnlyEnts);
  return WriteBSPFile(bspPath, g_targetPlatform->bigEndian);
}

/*
================
Com_FatalError

Error handler, prints error and exits.
================
*/
void Com_FatalError(const char *fmt, va_list args)
{
  vprintf(fmt, args);
  Com_Printf("\r\n");
  exit(1);
}

/*
================
ParseSmoothAngle

Parse angle string and convert to cosine threshold for smooth shading.
================
*/
long double ParseSmoothAngle(char *str, const char *optionName)
{
  double angle;
  long double result;
  float angleF;

  angle = atof(str);
  angleF = angle;

  if ( angle > 0.0 )
  {
    if ( angleF < 180.0 )
    {
      /* convert angle to cosine threshold with small bias */
      printf("%s = %g\n", optionName, angleF);
      result = cos(angleF * DEG2RAD) - PLANESIDE_EPSILON;
      if ( result < 0.0 )
        return 0.0;
    }
    else
    {
      /* 180+ degrees = smooth everything */
      printf("%s = 180\n", optionName);
      return 1.0;
    }
  }
  else
  {
    /* zero or negative = no smoothing */
    printf("%s = 0\n", optionName);
    return 1.0;
  }
  return result;
}

/*
================
BSP_SetTargetPlatform

Set BSP file extensions based on target platform.
MUST NOT be inlined — original is standalone function.
================
*/
void BSP_SetTargetPlatform()
{
  Assert(g_targetPlatform, s_assertDisable_BSP_SetTargetPlatform);

  /* PC and Xenon both use d3d extensions */
  if ( g_targetPlatform->platformId < PLATFORM_COUNT )
  {
    SetBSPFileExtensions("d3d");
  }
  else if ( !g_assertsDisabled )
  {
    /* unhandled platform */
    AssertFatal(0, s_assertDisable_BSP_SetTargetPlatform_0);
  }
}

/*
================
BSP_ByteSwap

Byte-swap a BSP file between PC and Xenon formats
================
*/
int BSP_ByteSwap(const CHAR *basePath)
{
  SetBSPFileExtensions("d3d");
  FS_Startup(basePath);
  FS_InitFilesystemFromArgs(g_basePath, g_fsBaseGame, g_fsBaseGame);

  /* build source path */
  I_strncpyz(g_outputBasePath, ExpandArg(basePath), sizeof(g_outputBasePath));
  StripExtension(g_outputBasePath);
  I_strncat(g_outputBasePath, sizeof(g_outputBasePath), GetBSPFileExtension());

  /* build target path with swapped platform directory */
  I_strncpyz(g_bspSwapPath, g_outputBasePath, sizeof(g_bspSwapPath));
  FS_ReplacePlatformPath(g_bspSwapPath, (char *)g_sourcePlatform->basePath, g_targetPlatform->basePath);

  printf("\nByte swapping: \n");
  printf("\t %s -> \n", g_outputBasePath);
  printf("\t %s \n", g_bspSwapPath);
  LoadAndWriteBSPFile(g_outputBasePath, g_bspSwapPath, g_targetPlatform->bigEndian);
  return 0;
}

/*
================
Opt_OnlyEnts

Enable -onlyEnts mode.
================
*/
int Opt_OnlyEnts()
{
  Com_Printf("onlyents = true\n");
  g_onlyEnts = 1;
  return 1;
}

/*
================
Opt_Verbose
================
*/
int Opt_Verbose()
{
  verbose = 1;
  return 1;
}

/*
================
Opt_VerboseEntities
================
*/
int Opt_VerboseEntities()
{
  Com_Printf("verboseentities = true\n");
  verboseEntities = 1;
  return 1;
}

/*
================
Opt_NoWater
================
*/
int Opt_NoWater()
{
  Com_Printf("nowater = true\n");
  nowater = 1;
  return 1;
}

/*
================
Opt_NoCurves
================
*/
int Opt_NoCurves()
{
  Com_Printf("nocurves = true\n");
  noCurveBrushes = 1;
  return 1;
}

/*
================
Opt_NoDetail
================
*/
int Opt_NoDetail()
{
  Com_Printf("nodetail = true\n");
  nodetail = 1;
  return 1;
}

/*
================
Opt_FullDetail
================
*/
int Opt_FullDetail()
{
  Com_Printf("fulldetail = true\n");
  fulldetail = 1;
  return 1;
}

/*
================
Opt_NoSubdivide
================
*/
int Opt_NoSubdivide()
{
  Com_Printf("nosubdivide = true\n");
  nosubdivide = 1;
  return 1;
}

/*
================
Opt_LeakTest
================
*/
int Opt_LeakTest()
{
  Com_Printf("leaktest = true\n");
  leaktest = 1;
  return 1;
}

/*
================
Opt_ExpandPlayer
================
*/
int Opt_ExpandPlayer()
{
  Com_Printf("Writing expanded.map for player collision.\n");
  testExpand = 1;
  return 1;
}

/*
================
Opt_ExpandBullet
================
*/
int Opt_ExpandBullet()
{
  Com_Printf("Writing expanded.map for bullet collision.\n");
  testExpand = 2;
  return 1;
}

/*
================
Opt_DebugPortals
================
*/
int Opt_DebugPortals()
{
  debugPortals = 1;
  return 1;
}

/*
================
Opt_NoReorderTris
================
*/
int Opt_NoReorderTris()
{
  reorderTris = 0;
  return 1;
}

/*
================
Opt_PcToXenon
================
*/
int Opt_PcToXenon()
{
  byteswapMode = 1;
  g_sourcePlatform = GetPlatformById(PLATFORM_PC);
  g_targetPlatform = GetPlatformById(PLATFORM_XENON);
  return 1;
}

/*
================
Opt_XenonToPc
================
*/
int Opt_XenonToPc()
{
  byteswapMode = 1;
  g_sourcePlatform = GetPlatformById(PLATFORM_XENON);
  g_targetPlatform = GetPlatformById(PLATFORM_PC);
  return 1;
}

/*
================
ParseBSPOption

Look up a command-line option in the options table and call its handler
================
*/
int ParseBSPOption(const char **argv, int argc)
{
  int i;
  
  /* Search for the option in the table */
  for (i = 0; optionsTable[i].name != NULL; i++)
  {
    if (_stricmp(*argv, optionsTable[i].name) == 0)
    {
      /* Found - call handler */
      if (optionsTable[i].handler)
        return optionsTable[i].handler(argc, argv);
      return 1;
    }
  }
  
  /* Not found - print error and usage */
  Com_Printf("\n\nUnknown argument '%s'\n\n", *argv);
  Com_Printf("USAGE: cod2map [options] mapname, where options are 0 or more of the following.\n");
  Com_Printf("Options ignore capitalization; it is only present in the list for clarity.\n");
  for (i = 0; optionsTable[i].name != NULL; i++)
    Com_Printf("%-20s %s\n", optionsTable[i].name, optionsTable[i].description);
  exit(-1);
  return 0;
}

/*
================
ProcessBSPArguments

Iterate through command-line arguments and process each BSP option
================
*/
void ProcessBSPArguments(const char **argv, int argc)
{
  int i;
  int argsUsed;

  /* consume arguments left to right */
  for ( i = argc; i > 0; argv += argsUsed )
  {
    argsUsed = ParseBSPOption(argv, i);

    /* handler must consume at least one argument */
    AssertFatal(argsUsed > 0, s_assertDisable_ProcessBSPArguments_0);
    i -= argsUsed;
  }
}

/*
================
main

Entry point — parse arguments, initialize subsystems, run BSP compilation.
================
*/
int main(int argc, const char **argv, const char **envp)
{
  unsigned int i;
  double startTime, endTime;
  char *prtExt;
  char buf[MAX_OS_PATH];

  /* unbuffered stdout — survives crashes when piped/redirected */
  setvbuf(stdout, NULL, _IONBF, 0);
  
#if defined(_WIN32) && !defined(_WIN64)
  /* set x87 FPU to 53-bit precision (double) — matches original exe behavior */
  _controlfp(_PC_53, _MCW_PC);
#endif

#if defined(_MSC_VER)
  /* restore MSVC6 CRT printf rounding (round-half-up, 3-digit exponents) */
  _CRT_INTERNAL_LOCAL_PRINTF_OPTIONS &= ~_CRT_INTERNAL_PRINTF_STANDARD_ROUNDING;
  _CRT_INTERNAL_LOCAL_PRINTF_OPTIONS |= _CRT_INTERNAL_PRINTF_LEGACY_THREE_DIGIT_EXPONENTS;
#endif

  Com_Printf("CoD2Map v1.1 (c) 2002 Id Software Inc. / Infinity Ward\n");

  /* allocate brush points buffer */
  g_brushPoints = malloc(MAX_BRUSH_POINTS * BRUSH_POINT_STRIDE * sizeof(int));
  if (!g_brushPoints)
    exit(1);
  memset(g_brushPoints, 0, MAX_BRUSH_POINTS * BRUSH_POINT_STRIDE * sizeof(int));

  /* allocate map planes array */
  mapplanes = malloc(MAX_MAP_PLANES * sizeof(Plane_t));
  if (!mapplanes)
    exit(1);
  memset(mapplanes, 0, MAX_MAP_PLANES * sizeof(Plane_t));

  Com_SetErrorHandler(Com_FatalError);
  TrisTestWindingBin();

  if ( argc < 2 )
  {
    Com_Printf("USAGE: cod2map [options] mapname, where options are 0 or more of the following.\n");
    Com_Printf("Options ignore capitalization; it is only present in the list for clarity.\n");
    for ( i = 0; optionsTable[i].name != NULL; i++ )
      Com_Printf("%-20s %s\n", optionsTable[i].name, optionsTable[i].description);
    exit(-1);
  }

  /* handle -info and -vis modes */
  if ( !strcmp(argv[1], "-info") )
  {
    BSPInfo(argc - 2, argv + 2);
    return 0;
  }
  if ( !strcmp(argv[1], "-vis") )
  {
    VisMain(argc - 1, (char **)(argv + 1));
    return 0;
  }

  /* parse options and start BSP compilation */
  Com_Printf("---- cod2map ----\n");
  g_loadFromPath[0] = '\0';
  ProcessBSPArguments(argv + 1, argc - 2);
  startTime = I_FloatTime();
  ThreadSetDefault();

  if ( byteswapMode )
    return BSP_ByteSwap(argv[argc - 1]);

  if ( !ValidatePlatformSet() )
    return 0;

  BSP_SetTargetPlatform();
  FS_Startup(argv[argc - 1]);
  FS_Startup_Simple();

  /* set up output paths */
  I_strncpyz(g_outputBasePath, ExpandArg(argv[argc - 1]), sizeof(g_outputBasePath));
  StripExtension(g_outputBasePath);

  /* remove stale output files */
  prtExt = GetPRTFileExtension();
  Com_sprintf(buf, sizeof(buf), "%s%s", g_outputBasePath, prtExt);
  remove(buf);
  Com_sprintf(buf, sizeof(buf), "%s.lin", g_outputBasePath);
  remove(buf);

  /* determine source map file */
  I_strncpyz(g_mapSourceFile, ExpandArg(argv[argc - 1]), sizeof(g_mapSourceFile));
  if ( Q_stricmp(COM_GetExtension(g_mapSourceFile), ".reg") )
  {
    Com_sprintf(buf, sizeof(buf), "%s.reg", g_outputBasePath);
    remove(buf);
    DefaultExtension(g_mapSourceFile, ".map");
  }

  if ( g_onlyEnts )
  {
    OnlyEnts();
    return 0;
  }

  /* BSP compilation pipeline */
  if ( strlen(g_loadFromPath) )
    LoadMapFile(g_loadFromPath);
  else
    LoadMapFile(g_mapSourceFile);

  ProcessWorldBrushes();
  ProcessCoronaLights();
  ProcessVisibility();
  EmitBSP();
  EndBSPFile();

  endTime = I_FloatTime();
  Com_Printf("%5.0f seconds elapsed\n", endTime - startTime);

  return 0;
}

/*
================
Opt_loadFrom

Load map from alternate path.
================
*/
int Opt_loadFrom(int argc, char **argv)
{
  unsigned int i;

  if ( argc < 2 )
  {
    Com_Printf("USAGE: cod2map [options] mapname, where options are 0 or more of the following.\n");
    Com_Printf("Options ignore capitalization; it is only present in the list for clarity.\n");
    for ( i = 0; optionsTable[i].name != NULL; i++ )
      Com_Printf("%-20s %s\n", optionsTable[i].name, optionsTable[i].description);
    exit(-1);
  }
  I_strncpyz(g_loadFromPath, argv[1], sizeof(g_loadFromPath));
  return 2;
}

/*
================
Opt_blockSize

Set BSP block subdivision size.
================
*/
int Opt_blockSize(int argc, char **argv)
{
  unsigned int i;
  double sizeVal;

  if ( argc < 2 )
  {
    Com_Printf("USAGE: cod2map [options] mapname, where options are 0 or more of the following.\n");
    Com_Printf("Options ignore capitalization; it is only present in the list for clarity.\n");
    for ( i = 0; optionsTable[i].name != NULL; i++ )
      Com_Printf("%-20s %s\n", optionsTable[i].name, optionsTable[i].description);
    exit(-1);
  }

  sizeVal = atof(argv[1]);
  blockSize = sizeVal;

  /* clamp minimum */
  if ( sizeVal > 0.0 && blockSize <= MIN_BLOCK_SIZE )
    blockSize = MIN_BLOCK_SIZE;

  if ( blockSize > 0.0 )
  {
    Com_Printf("blocksize is %g units\n", blockSize);
    return 2;
  }

  blockSize = 0.0;
  Com_Printf("blocksize is disabled\n", 0.0);
  return 2;
}

/*
================
Opt_sampleScale

Set lightmap sample scale factor.
================
*/
int Opt_sampleScale(int argc, char **argv)
{
  unsigned int i;
  double scaleVal;
  float scaleF;

  if ( argc < 2 )
  {
    Com_Printf("USAGE: cod2map [options] mapname, where options are 0 or more of the following.\n");
    Com_Printf("Options ignore capitalization; it is only present in the list for clarity.\n");
    for ( i = 0; optionsTable[i].name != NULL; i++ )
      Com_Printf("%-20s %s\n", optionsTable[i].name, optionsTable[i].description);
    exit(-1);
  }

  scaleVal = atof(argv[1]);
  scaleF = scaleVal;
  if ( scaleVal <= 0.0 )
    Com_Error("sampleScale must be > 0");
  sampleScale = 1.0 / scaleF;
  Com_Printf("each lightmap sample will be scaled by %g\n", scaleF);
  return 2;
}

/*
================
Opt_brushSmoothAngle

Set smooth shading angle threshold for brush surfaces.
================
*/
int Opt_brushSmoothAngle(int argc, char **argv)
{
  unsigned int i;

  if ( argc < 2 )
  {
    Com_Printf("USAGE: cod2map [options] mapname, where options are 0 or more of the following.\n");
    Com_Printf("Options ignore capitalization; it is only present in the list for clarity.\n");
    for ( i = 0; optionsTable[i].name != NULL; i++ )
      Com_Printf("%-20s %s\n", optionsTable[i].name, optionsTable[i].description);
    exit(-1);
  }
  brushSmoothAngle = ParseSmoothAngle(argv[1], "brushSmoothAngle");
  return 2;
}

/*
================
Opt_curveSmoothAngle

Set smooth shading angle threshold for curve surfaces.
================
*/
int Opt_curveSmoothAngle(int argc, char **argv)
{
  unsigned int i;

  if ( argc < 2 )
  {
    Com_Printf("USAGE: cod2map [options] mapname, where options are 0 or more of the following.\n");
    Com_Printf("Options ignore capitalization; it is only present in the list for clarity.\n");
    for ( i = 0; optionsTable[i].name != NULL; i++ )
      Com_Printf("%-20s %s\n", optionsTable[i].name, optionsTable[i].description);
    exit(-1);
  }
  curveSmoothAngle = ParseSmoothAngle(argv[1], "curveSmoothAngle");
  return 2;
}

/*
================
Opt_terrainSmoothAngle

Set smooth shading angle threshold for terrain surfaces.
================
*/
int Opt_terrainSmoothAngle(int argc, char **argv)
{
  unsigned int i;

  if ( argc < 2 )
  {
    Com_Printf("USAGE: cod2map [options] mapname, where options are 0 or more of the following.\n");
    Com_Printf("Options ignore capitalization; it is only present in the list for clarity.\n");
    for ( i = 0; optionsTable[i].name != NULL; i++ )
      Com_Printf("%-20s %s\n", optionsTable[i].name, optionsTable[i].description);
    exit(-1);
  }
  terrainSmoothAngle = ParseSmoothAngle(argv[1], "terrainSmoothAngle");
  return 2;
}

/*
================
Opt_subdivisions

Set automatic subdivision tessellation size.
================
*/
int Opt_subdivisions(int argc, char **argv)
{
  unsigned int i;
  double subdivVal;

  if ( argc < 2 )
  {
    Com_Printf("USAGE: cod2map [options] mapname, where options are 0 or more of the following.\n");
    Com_Printf("Options ignore capitalization; it is only present in the list for clarity.\n");
    for ( i = 0; optionsTable[i].name != NULL; i++ )
      Com_Printf("%-20s %s\n", optionsTable[i].name, optionsTable[i].description);
    exit(-1);
  }

  subdivVal = atof(argv[1]);
  g_matExpandRegion.defaultTessSize = subdivVal;
  if ( subdivVal < 0.0 )
    g_matExpandRegion.defaultTessSize = 0.0;

  if ( g_matExpandRegion.defaultTessSize == 0.0 )
  {
    Com_Printf("automatic subdivision disabled\n");
    return 2;
  }
  Com_Printf("automatically subdividing everything to %g x %g units\n", g_matExpandRegion.defaultTessSize, g_matExpandRegion.defaultTessSize);
  return 2;
}

/*
================
Opt_brushMethod

Set brush collision method (players, bullets, all, none).
================
*/
int Opt_brushMethod(int argc, char **argv)
{
  unsigned int i, j;
  int methodIdx;

  /* brush method lookup table */
  struct { const char *name; int value; } methods[NUM_BRUSH_METHODS];
  methods[0].name = "players";  methods[0].value = 2;
  methods[1].name = "bullets";  methods[1].value = 1;
  methods[2].name = "all";      methods[2].value = -1;
  methods[3].name = "none";     methods[3].value = 0;

  if ( argc < 2 )
  {
    Com_Printf("USAGE: cod2map [options] mapname, where options are 0 or more of the following.\n");
    Com_Printf("Options ignore capitalization; it is only present in the list for clarity.\n");
    for ( i = 0; optionsTable[i].name != NULL; i++ )
      Com_Printf("%-20s %s\n", optionsTable[i].name, optionsTable[i].description);
    exit(-1);
  }

  /* find matching method */
  for ( methodIdx = 0; methodIdx < NUM_BRUSH_METHODS; methodIdx++ )
  {
    if ( !Q_stricmp(argv[1], methods[methodIdx].name) )
    {
      brushMethod = methods[methodIdx].value;
      return 2;
    }
  }

  Com_Printf("Invalid brush method '%s'.  Valid methods are:\n", argv[1]);
  for ( j = 0; j < 4; j++ )
    Com_Printf("  %s\n", methods[j].name);
  exit(-1);
  return 2;
}

/*
================
ProcessWorldBrushes

Assign model numbers to brush entities.
Iterates through all non-worldspawn entities and assigns "model" keys
to entities that have brushes or patches. Model numbers start at "*1".
These model references are used in the BSP to link entities to geometry.
================
*/
int ProcessWorldBrushes(void)
{
  int i;
  int modelNum;
  Entity_t *ent;
  char modelBuffer[12];

  modelNum = 1;
  for ( i = 1; i < num_entities; i++ )
  {
    ent = &g_entities[i];
    if ( ent->brushes || ent->patches )
    {
      Com_sprintf(modelBuffer, sizeof(modelBuffer), "*%i", modelNum++);
      SetKeyValue(ent, "model", modelBuffer);
    }
  }
  return num_entities;
}

/*
================
ProcessCoronaLights

Auto-configure corona light z-cutoff values.

Corona lights are glowing effects around light sources. This function finds
corona entities that don't have zcutoff/zfadeout set, then looks for nearby
misc_model entities (within 8 units) that are hanging lights. If found,
it automatically sets the z-cutoff values for proper rendering.

This prevents coronas from being visible through floors/ceilings by
fading them out when viewed from above/below.
================
*/
int ProcessCoronaLights(void)
{
  int i, j;
  Entity_t *ent, *other;
  const char *classname, *zcutoff, *model;

  for ( i = 1; i < num_entities; i++ )
  {
    ent = &g_entities[i];
    classname = ValueForKey(ent, "classname");
    if ( Q_stricmp(classname, "corona") )
      continue;

    zcutoff = ValueForKey(ent, "zcutoff");
    if ( zcutoff && *zcutoff )
      continue;

    /* find nearby misc_model that is a hanging light */
    for ( j = 1; j < num_entities; j++ )
    {
      other = &g_entities[j];
      if ( Q_stricmp(ValueForKey(other, "classname"), "misc_model") )
        continue;
      if ( VectorDistance(ent->origin, other->origin) > DUPLICATE_MODEL_DIST )
        continue;

      model = ValueForKey(other, "model");
      if ( !Q_stricmp(model, "xmodel/hangingutillight")
        || !Q_stricmp(model, "xmodel/light_ind_ceiling2on") )
      {
        SetKeyValue(ent, "zcutoff", "-0.15");
        SetKeyValue(ent, "zfadeout", "-0.25");
      }
      break;
    }
  }
  return num_entities;
}

/*
================
ProcessVisibility

Initialize visibility/BSP counters.
Resets global counters used during BSP generation before processing begins.
Called after map entities are loaded, before BSP tree building.
================
*/
int ProcessVisibility(void)
{
  numBSPModels = 0;
  numBSPNodes = 0;
  numBSPBrushSides = 0;
  numBSPLeafSurfaces = 0;
  numBSPLeafBrushes = 0;
  numBSPLeafs = 1;
  return 0;
}

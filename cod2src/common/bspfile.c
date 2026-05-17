/*
bspfile.c — BSP file format I/O

Reconstructed from cod2map.exe by Rose.
*/

#include "cod2map.h"

/* core geometry */
BspLeaf_disk_t  bspLeafs[MAX_MAP_LEAFS];
BspModel_t      bspModels[MAX_MAP_MODELS];
BspNode_disk_t  bspNodes[MAX_MAP_NODES];
BspPlane_disk_t bspPlanes[MAX_MAP_PLANES];

/* surfaces */
BspDrawVert_t  bspDrawVerts[MAX_MAP_DRAW_VERTS];
BspDrawVert_t  g_drawVertexBuf[(MAX_MAP_DRAW_VERTS + 1)];
BspTriSoup_t   bspTriangles[MAX_MAP_TRISOUPS];
#define bspTriSoupData (&g_drawVertexBuf[1])
unsigned short bspDrawIndexes[MAX_MAP_DRAW_INDEXES];

/* brushes */
BspBrushSide_t bspBrushSidesData[MAX_MAP_BRUSHSIDES];
BspBrush_t     bspBrushes[MAX_MAP_BRUSHES];
int            bspLeafBrushes[MAX_MAP_LEAFBRUSHES];
int            bspLeafSurfaces[MAX_MAP_LEAFSURFACES];

/* visibility / portals */
BspPortal_t bspPortals[MAX_MAP_PORTALS];
vec3_t      bspPortalVerts[MAX_MAP_PORTAL_VERTS];
char        bspVisBytes[MAX_MAP_VISIBILITY];

/* collision */
BspAabbTreeEntry_t   bspAabbTrees[MAX_MAP_AABBTREES];
BspCollisionBorder_t bspCollisionBorders[MAX_MAP_COLLISION_BORDERS];
BspCollisionEdge_t   bspCollisionEdgeData[MAX_MAP_COLLISION_EDGES];
BspCollisionPart_t   bspCollisionParts[MAX_MAP_COLLISION_PARTS];
BspCollisionTri_t    bspCollisionTriData[MAX_MAP_COLLISION_TRIS];
BspCollisionVert_t   bspCollisionVerts[MAX_MAP_COLLISION_VERTS];
CollisionAabbTree_t  bspCollisionAABBs[MAX_MAP_COLLISION_AABBS];

/* lighting */
BspLightGridColor_t bspLightGridColors[MAX_MAP_LIGHTGRIDCOLORS];
BspLightGridEntry_t bspLightGridHash[MAX_MAP_LIGHTGRID];
BspLight_t          bspLights[MAX_MAP_LIGHTS];

/* shadows */
BspShadowCluster_disk_t  bspShadowClusters[MAX_MAP_SHADOW_CLUSTERS];
BspShadowSource_t        bspShadowSources[MAX_MAP_SHADOW_SOURCES];
BspShadowVert_t          bspShadowVerts[MAX_MAP_SHADOW_VERTS];
DiskShadowAabb_t         bspShadowData[MAX_MAP_SHADOW_AABBTREES];
#define SHADOW_INDEX_SENTINEL 1
static short             _bspShadowIndexBuf[MAX_MAP_SHADOW_INDEXES + SHADOW_INDEX_SENTINEL];
short                   *bspShadowIndexes = &_bspShadowIndexBuf[SHADOW_INDEX_SENTINEL];

/* occluders */
BspOccluder_t     bspOccluders[MAX_MAP_OCCLUDERS];
BspOccluderEdge_t bspOccluderEdges[MAX_MAP_OCCLUDER_EDGES];
short             bspOccluderIndexes[MAX_MAP_OCCLUDER_INDEXES];
int               bspOccluderPlanes[MAX_MAP_OCCLUDER_PLANES];

/* cells / cullgroups */
BspCell_t      bspCells[MAX_MAP_CELLS];
BspCullGroup_t bspCullGroups[MAX_MAP_CULLGROUPS];
int            bspCullGroupIndexes[MAX_MAP_CULLGROUPINDEXES];

/* entities / materials / misc */
Entity_t    g_entities[MAX_MAP_ENTITIES];
Dmaterial_t bspMaterials[MAX_MAP_MATERIALS];
char        bspEntData[MAX_MAP_ENTSTRING];
char        bspPaths[MAX_MAP_PATHS];
char        g_bspSwapPath[MAX_OS_PATH];
char        g_largeBuf[LARGE_BUF_SIZE];

int  bspEntDataSize;
char Buffer[16];
char g_polyFileExt[16];
char g_prtFileExt[16];
int  numBSPAabbTrees;
int  numBSPBrushes;
int  numBSPBrushSides;
int  numBSPCells;
int  numBSPCollisionAABBs;
int  numBSPCollisionBorders;
int  numBSPCollisionEdges;
int  numBSPCollisionParts;
int  numBSPCollisionTris;
int  numBSPCollisionVerts;
int  numBSPCullGroupIndexes;
int  numBSPCullGroups;
int  numBSPDrawIndexes;
int  numBSPDrawVerts;
int  numBSPDrawVertsEmitted;
int  numBSPLeafBrushes;
int  numBSPLeafs;
int  numBSPLeafSurfaces;
int  numBSPLightBytes;
int  numBSPLightGridColors;
int  numBSPLightGridHash;
int  numBSPLights;
int  numBSPMaterials;
int  numBSPModels;
int  numBSPNodes;
int  numBSPOccluderEdges;
int  numBSPOccluderIndexes;
int  numBSPOccluderPlanes;
int  numBSPOccluders;
int  numBSPPlanes;
int  numBSPPortals;
int  numBSPPortalVerts;
int  numBSPShadowAabbTrees;
int  numBSPShadowClusters;
int  numBSPShadowIndices;
int  numBSPShadowSources;
int  numBSPShadowVerts;
int  numBSPTriSoups;
int  numBSPVisBytes;
int  num_entities;

char s_assertDisable_GetBSPFileExtension;
char s_assertDisable_GetPRTFileExtension;
char s_assertDisable_GetPolyFileExtension;
char s_assertDisable_SetBSPFileExtensions;
char s_assertDisable_SwapCollisionAabbLumpData;
char s_assertDisable_SwapCollisionPartitionLumpData;
char s_assertDisable_SwapDrawSurfLumpData;
char s_assertDisable_SwapDrawSurfLumpData_0;
char s_assertDisable_SwapLightGridEntryLumpData;
char s_assertDisable_SwapLumpData;
char s_assertDisable_SwapMaterialLumpData;
char s_assertDisable_SwapOccluderLumpData;
char s_assertDisable_SwapShortBlock;
char s_assertDisable_SwapShortBlock_0;
char s_assertDisable_SwapTriangleLumpData;
char s_assertDisable_SwapVisData;
char s_assertDisable_UnparseEntities;


/*
================
HandlePlatformOption

Parse -platform argument and set target platform.
================
*/
int HandlePlatformOption(int argc, char **argv)
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
  SetTargetPlatformByName(argv[1]);
  return 2;
}

/*
================
SwapBlock

Swap all 32-bit values in a block
================
*/
int SwapBlock(int size, void *block)
{
  unsigned int *p = block;
  int count;
  int i;
  int last;

  last = size;
  count = size >> 2;
  for ( i = 0; i < count; i++ )
  {
    last = BigLong(p[i]);
    p[i] = last;
  }
  return last;
}

/*
================
SwapShortBlock

Swap all 16-bit values in a block.
================
*/
void SwapShortBlock(int size, unsigned short *data)
{
  int count;
  int i;

  Assert(data != NULL, s_assertDisable_SwapShortBlock);
  Assert(size >= 0, s_assertDisable_SwapShortBlock_0);

  count = size >> 1;
  for ( i = 0; i < count; i++ )
    data[i] = BigShort(data[i]);
}

/*
================
SwapDrawSurfLumpData

Byte-swap draw surface lump entries.
================
*/
void SwapDrawSurfLumpData(BspTriSoup_disk_t *data, unsigned int lumpSize)
{
  int count;
  int i;

  Assert(data != NULL, s_assertDisable_SwapDrawSurfLumpData);
  count = lumpSize / sizeof(*data);
  Assert(lumpSize % sizeof(*data) == 0, s_assertDisable_SwapDrawSurfLumpData_0);

  /* swap each entry's fields */
  for ( i = 0; i < count; i++ )
  {
    /* material and lightmap */
    data[i].materialSortKey = BigLong(data[i].materialSortKey);
    data[i].lightmapIndex = BigShort(data[i].lightmapIndex);

    /* vertex/index ranges — patches also need numVerts swapped */
    data[i].firstVert = BigLong(data[i].firstVert);
    if ( !data[i].surfType )
      data[i].numVerts = BigLong(data[i].numVerts);
    data[i].firstIndex = BigLong(data[i].firstIndex);
    data[i].numIndices = BigLong(data[i].numIndices);

    /* buffer references */
    data[i].vertBufIndex = BigLong(data[i].vertBufIndex);
    data[i].vertBufFirstVert = BigLong(data[i].vertBufFirstVert);
    data[i].idxBufIndex = BigLong(data[i].idxBufIndex);
    data[i].idxBufFirstIdx = BigLong(data[i].idxBufFirstIdx);
  }
}

/*
================
SwapMaterialLumpData

Byte-swap material lump entries.
================
*/
void SwapMaterialLumpData(Dmaterial_t *data, unsigned int lumpSize)
{
  int count;
  int i;

  Assert(data != NULL, s_assertDisable_SwapMaterialLumpData);
  count = lumpSize / sizeof(Dmaterial_t);
  Assert(lumpSize % sizeof(Dmaterial_t) == 0, s_assertDisable_SwapMaterialLumpData);

  /* only surfaceFlags and contentFlags need swapping — name is a char array */
  for ( i = 0; i < count; i++ )
  {
    data[i].surfaceFlags = BigLong(data[i].surfaceFlags);
    data[i].contentFlags = BigLong(data[i].contentFlags);
  }
}

/*
================
SwapTriangleLumpData

Byte-swap triangle soup lump entries.
================
*/
int SwapTriangleLumpData(BspTriSoup_t *data, int lumpSize)
{
  int count;
  int i;

  Assert(data != NULL, s_assertDisable_SwapTriangleLumpData);
  Assert(lumpSize >= 0, s_assertDisable_SwapTriangleLumpData);

  count = lumpSize / sizeof(*data);

  /* swap each entry's fields */
  for ( i = 0; i < count; i++ )
  {
    data[i].materialIndex = BigShort(data[i].materialIndex);
    data[i].lightmapIndex = BigShort(data[i].lightmapIndex);
    data[i].firstVertex = BigLong(data[i].firstVertex);
    data[i].vertexCount = BigShort(data[i].vertexCount);
    data[i].indexCount = BigShort(data[i].indexCount);
    data[i].firstIndex = BigLong(data[i].firstIndex);
  }
  return count;
}

/*
================
SwapOccluderLumpData

Byte-swap occluder lump entries.
================
*/
int SwapOccluderLumpData(BspOccluder_t *data, int lumpSize)
{
  int count;
  int i;

  Assert(data != NULL, s_assertDisable_SwapOccluderLumpData);
  Assert(lumpSize >= 0, s_assertDisable_SwapOccluderLumpData);

  count = lumpSize / sizeof(*data);

  /* swap each entry's fields */
  for ( i = 0; i < count; i++ )
  {
    data[i].startPlanes  = BigLong(data[i].startPlanes);
    data[i].numNewPlanes = BigShort(data[i].numNewPlanes);
    data[i].numNewEdges  = BigShort(data[i].numNewEdges);
    data[i].startEdges   = BigLong(data[i].startEdges);
    data[i].startVerts   = BigLong(data[i].startVerts);
    data[i].numNewVerts  = BigShort(data[i].numNewVerts);
  }
  return count;
}

/*
================
SwapCollisionPartitionLumpData

Byte-swap collision partition lump entries.
================
*/
int SwapCollisionPartitionLumpData(BspCollisionPart_t *data, int lumpSize)
{
  int count;
  int i;

  Assert(data != NULL, s_assertDisable_SwapCollisionPartitionLumpData);
  Assert(lumpSize >= 0, s_assertDisable_SwapCollisionPartitionLumpData);

  count = lumpSize / sizeof(*data);

  /* swap int fields — byte fields don't need swapping */
  for ( i = 0; i < count; i++ )
  {
    data[i].reserved0 = BigShort(data[i].reserved0);
    data[i].firstTri = BigLong(data[i].firstTri);
    data[i].firstBorder = BigLong(data[i].firstBorder);
  }
  return count;
}

/*
================
SwapCollisionAabbLumpData

Byte-swap collision AABB lump entries.
================
*/
int SwapCollisionAabbLumpData(BspCollisionAabb_disk_t *data, int lumpSize)
{
  int count;
  int i;

  Assert(data != NULL, s_assertDisable_SwapCollisionAabbLumpData);
  Assert(lumpSize >= 0, s_assertDisable_SwapCollisionAabbLumpData);

  count = lumpSize / sizeof(*data);

  /* swap all fields */
  for ( i = 0; i < count; i++ )
  {
    /* bounds */
    data[i].mins[0] = (float)BigLong((int)data[i].mins[0]);
    data[i].mins[1] = (float)BigLong((int)data[i].mins[1]);
    data[i].mins[2] = (float)BigLong((int)data[i].mins[2]);
    data[i].maxs[0] = (float)BigLong((int)data[i].maxs[0]);
    data[i].maxs[1] = (float)BigLong((int)data[i].maxs[1]);
    data[i].maxs[2] = (float)BigLong((int)data[i].maxs[2]);

    /* child and partition references */
    data[i].firstChild = BigShort(data[i].firstChild);
    data[i].childCount = BigShort(data[i].childCount);
    data[i].firstPartition = BigLong(data[i].firstPartition);
  }
  return count;
}

/*
================
SwapLightGridEntryLumpData

Byte-swap light grid entries.
NOTE: parameter order is swapped vs other swap functions (size first, data second).
================
*/
void SwapLightGridEntryLumpData(int size, BspLightGridEntry_t *data)
{
  int count;
  int i;

  Assert(data != NULL, s_assertDisable_SwapLightGridEntryLumpData);
  Assert(size >= 0, s_assertDisable_SwapLightGridEntryLumpData);

  count = size / sizeof(*data);

  /* swap int and short fields — byte data stays as-is */
  for ( i = 0; i < count; i++ )
  {
    data[i].colorData = BigLong(data[i].colorData);
    data[i].dirIndex = BigShort(data[i].dirIndex);
  }
}

/*
================
SwapVisData

Byte-swap visibility data — variable-length structure with per-cluster portal lists.
================
*/
int SwapVisData(BspVisHeader_t *vis, int size, int swapFlag)
{
  short numClusters, portalCount = 0;
  union { unsigned short *s; BspVisPortal_t *p; } cur;
  int i, j;

  Assert(vis != NULL, s_assertDisable_SwapVisData);
  Assert(size >= 0, s_assertDisable_SwapVisData);

  /* swap header */
  vis->numClusters = BigLong(vis->numClusters);
  if ( swapFlag )
    numClusters = BigShort(vis->numPortals);
  else
    numClusters = vis->numPortals;
  vis->numPortals = BigShort(vis->numPortals);

  /* walk per-cluster portal lists */
  cur.s = vis->portalData;
  for ( i = 0; i < numClusters; i++ )
  {
    /* read portal count before swapping */
    if ( swapFlag )
      portalCount = BigShort(*cur.s);
    else
      portalCount = *cur.s;
    *cur.s = BigShort(*cur.s);
    cur.s++;

    /* swap each packed portal entry */
    for ( j = 0; j < portalCount; j++ )
    {
      cur.p[j].index = BigShort(cur.p[j].index);
      cur.p[j].offset = BigLong(cur.p[j].offset);
    }
    cur.p += portalCount;
  }
  return portalCount;
}

/*
================
SwapLumpData

Dispatch byte-swapping for a BSP lump based on lump index.
================
*/
void SwapLumpData(int lumpIdx, void *data, int maxCount, int count, int elemSize, int swapFlag)
{
  int dataSize;

  Assert(count >= 0, s_assertDisable_SwapLumpData);
  Assert(elemSize > 0, s_assertDisable_SwapLumpData);

  if ( !count )
    return;
  if ( count > maxCount )
    Com_Error("%i > %i\n", count, maxCount);

  dataSize = elemSize * count;
  Assert(data != NULL, s_assertDisable_SwapLumpData);
  if ( dataSize <= 0 )
  {
    Assert(dataSize == 0, s_assertDisable_SwapLumpData);
    return;
  }

  /* dispatch to lump-specific swap handler */
  switch ( lumpIdx )
  {
    case LUMP_MATERIALS:           SwapMaterialLumpData(data, dataSize); break;
    case LUMP_LIGHTBYTES:          return;  /* byte data — no swap needed */
    case LUMP_LIGHTGRIDENTRIES:    SwapLightGridEntryLumpData(dataSize, data); break;
    case LUMP_LIGHTGRIDCOLORS:     return;  /* byte data — no swap needed */
    case LUMP_PLANES:              SwapBlock(dataSize, data); break;
    case LUMP_BRUSHSIDES:          SwapBlock(dataSize, data); break;
    case LUMP_BRUSHES:             SwapShortBlock(dataSize, data); break;
    case LUMP_TRIANGLES:           SwapTriangleLumpData(data, dataSize); break;
    case LUMP_DRAWVERTS:           SwapBlock(dataSize, data); break;
    case LUMP_DRAWINDICES:         SwapShortBlock(dataSize, data); break;
    case LUMP_CULLGROUPS:          SwapBlock(dataSize, data); break;
    case LUMP_CULLGROUPINDICES:    SwapBlock(dataSize, data); break;
    case LUMP_OBSOLETE_1:          SwapBlock(dataSize, data); break;
    case LUMP_OBSOLETE_2:          SwapShortBlock(dataSize, data); break;
    case LUMP_OBSOLETE_3:          SwapBlock(dataSize, data); break;
    case LUMP_OBSOLETE_4:          SwapDrawSurfLumpData(data, dataSize); break;
    case LUMP_OBSOLETE_5:          SwapBlock(dataSize, data); break;
    case LUMP_PORTALVERTS:         SwapBlock(dataSize, data); break;
    case LUMP_OCCLUDER:            SwapOccluderLumpData(data, dataSize); break;
    case LUMP_OCCLUDERPLANES:      SwapBlock(dataSize, data); break;
    case LUMP_OCCLUDEREDGES:       return;  /* byte data — no swap needed */
    case LUMP_OCCLUDERINDICES:     SwapShortBlock(dataSize, data); break;
    case LUMP_AABBTREES:           SwapBlock(dataSize, data); break;
    case LUMP_CELLS:               SwapBlock(dataSize, data); break;
    case LUMP_PORTALS:             SwapBlock(dataSize, data); break;
    case LUMP_NODES:               SwapBlock(dataSize, data); break;
    case LUMP_LEAFS:               SwapBlock(dataSize, data); break;
    case LUMP_LEAFBRUSHES:         SwapBlock(dataSize, data); break;
    case LUMP_LEAFSURFACES:        SwapBlock(dataSize, data); break;
    case LUMP_COLLISIONVERTS:      SwapBlock(dataSize, data); break;
    case LUMP_COLLISIONEDGES:      SwapBlock(dataSize, data); break;
    case LUMP_COLLISIONTRIS:       SwapBlock(dataSize, data); break;
    case LUMP_COLLISIONBORDERS:    SwapBlock(dataSize, data); break;
    case LUMP_COLLISIONPARTITIONS: SwapCollisionPartitionLumpData(data, dataSize); break;
    case LUMP_COLLISIONAABBS:      SwapCollisionAabbLumpData(data, dataSize); break;
    case LUMP_MODELS:              SwapBlock(dataSize, data); break;
    case LUMP_VISIBILITY:
    {
      /* visibility header is just 2 ints */
      int *visHeader = data;
      visHeader[0] = BigLong(visHeader[0]);
      visHeader[1] = BigLong(visHeader[1]);
      break;
    }
    case LUMP_ENTITIES:            return;  /* string data — no swap needed */
    case LUMP_PATHCONNECTIONS:     SwapVisData(data, dataSize, swapFlag); break;
    default:
      Com_Error("SwapLumpData: Unknown lump type");
      break;
  }
}

/*
================
SwapBSPFile

Byte-swap all lumps in a loaded BSP file.
================
*/
int SwapBSPFile(int swapFlag)
{
  Swap_Init();

  /*           lump                      data                   maxCount                   count                   elemSize                                 */
  SwapLumpData(LUMP_MATERIALS,           bspMaterials,          MAX_MAP_MATERIALS,         numBSPMaterials,        sizeof(bspMaterials[0]),         swapFlag);
  SwapLumpData(LUMP_LIGHTBYTES,          bspLightmapData,       MAX_MAP_LIGHTBYTES,        numBSPLightBytes,       sizeof(bspLightmapData[0]),      swapFlag);
  SwapLumpData(LUMP_LIGHTGRIDENTRIES,    bspLightGridHash,      MAX_MAP_LIGHTGRID,         numBSPLightGridHash,    sizeof(bspLightGridHash[0]),     swapFlag);
  SwapLumpData(LUMP_LIGHTGRIDCOLORS,     bspLightGridColors,    MAX_MAP_LIGHTGRIDCOLORS,   numBSPLightGridColors,  sizeof(bspLightGridColors[0]),   swapFlag);
  SwapLumpData(LUMP_PLANES,              bspPlanes,             MAX_MAP_PLANES,            numBSPPlanes,           sizeof(bspPlanes[0]),            swapFlag);
  SwapLumpData(LUMP_BRUSHSIDES,          bspBrushSidesData,     MAX_MAP_BRUSHSIDES,        numBSPBrushSides,       sizeof(bspBrushSidesData[0]),    swapFlag);
  SwapLumpData(LUMP_BRUSHES,             bspBrushes,            MAX_MAP_BRUSHES,           numBSPBrushes,          sizeof(bspBrushes[0]),           swapFlag);
  SwapLumpData(LUMP_TRIANGLES,           bspTriangles,          MAX_MAP_TRISOUPS,          numBSPTriSoups,         sizeof(bspTriangles[0]),         swapFlag);
  SwapLumpData(LUMP_DRAWVERTS,           bspDrawVerts,          MAX_MAP_DRAW_VERTS,        numBSPDrawVerts,        sizeof(bspDrawVerts[0]),         swapFlag);
  SwapLumpData(LUMP_DRAWINDICES,         bspDrawIndexes,        MAX_MAP_DRAW_INDEXES,      numBSPDrawIndexes,      sizeof(bspDrawIndexes[0]),       swapFlag);
  SwapLumpData(LUMP_CULLGROUPS,          bspCullGroups,         MAX_MAP_CULLGROUPS,        numBSPCullGroups,       sizeof(bspCullGroups[0]),        swapFlag);
  SwapLumpData(LUMP_CULLGROUPINDICES,    bspCullGroupIndexes,   MAX_MAP_CULLGROUPINDEXES,  numBSPCullGroupIndexes, sizeof(bspCullGroupIndexes[0]),  swapFlag);
  SwapLumpData(LUMP_OBSOLETE_1,          bspShadowVerts,        MAX_MAP_SHADOW_VERTS,      numBSPShadowVerts,      sizeof(bspShadowVerts[0]),       swapFlag);
  SwapLumpData(LUMP_OBSOLETE_2,          bspShadowIndexes,      MAX_MAP_SHADOW_INDEXES,    numBSPShadowIndices,    sizeof(bspShadowIndexes[0]),     swapFlag);
  SwapLumpData(LUMP_OBSOLETE_3,          bspShadowClusters,     MAX_MAP_SHADOW_CLUSTERS,   numBSPShadowClusters,   sizeof(bspShadowClusters[0]),    swapFlag);
  SwapLumpData(LUMP_OBSOLETE_4,          bspShadowData,         MAX_MAP_SHADOW_AABBTREES,  numBSPShadowAabbTrees,  sizeof(bspShadowData[0]),        swapFlag);
  SwapLumpData(LUMP_OBSOLETE_5,          bspShadowSources,      MAX_MAP_SHADOW_SOURCES,    numBSPShadowSources,    sizeof(bspShadowSources[0]),     swapFlag);
  SwapLumpData(LUMP_PORTALVERTS,         bspPortalVerts,        MAX_MAP_PORTAL_VERTS,      numBSPPortalVerts,      sizeof(bspPortalVerts[0]),       swapFlag);
  SwapLumpData(LUMP_OCCLUDER,            bspOccluders,          MAX_MAP_OCCLUDERS,         numBSPOccluders,        sizeof(bspOccluders[0]),         swapFlag);
  SwapLumpData(LUMP_OCCLUDERPLANES,      bspOccluderPlanes,     MAX_MAP_OCCLUDER_PLANES,   numBSPOccluderPlanes,   sizeof(bspOccluderPlanes[0]),    swapFlag);
  SwapLumpData(LUMP_OCCLUDEREDGES,       bspOccluderEdges,      MAX_MAP_OCCLUDER_EDGES,    numBSPOccluderEdges,    sizeof(bspOccluderEdges[0]),     swapFlag);
  SwapLumpData(LUMP_OCCLUDERINDICES,     bspOccluderIndexes,    MAX_MAP_OCCLUDER_INDEXES,  numBSPOccluderIndexes,  sizeof(bspOccluderIndexes[0]),   swapFlag);
  SwapLumpData(LUMP_AABBTREES,           bspAabbTrees,          MAX_MAP_AABBTREES,         numBSPAabbTrees,        sizeof(bspAabbTrees[0]),         swapFlag);
  SwapLumpData(LUMP_CELLS,               bspCells,              MAX_MAP_CELLS,             numBSPCells,            sizeof(bspCells[0]),             swapFlag);
  SwapLumpData(LUMP_PORTALS,             bspPortals,            MAX_MAP_PORTALS,           numBSPPortals,          sizeof(bspPortals[0]),           swapFlag);
  SwapLumpData(LUMP_NODES,               bspNodes,              MAX_MAP_NODES,             numBSPNodes,            sizeof(bspNodes[0]),             swapFlag);
  SwapLumpData(LUMP_LEAFS,               bspLeafs,              MAX_MAP_LEAFS,             numBSPLeafs,            sizeof(bspLeafs[0]),             swapFlag);
  SwapLumpData(LUMP_LEAFBRUSHES,         bspLeafBrushes,        MAX_MAP_LEAFBRUSHES,       numBSPLeafBrushes,      sizeof(bspLeafBrushes[0]),       swapFlag);
  SwapLumpData(LUMP_LEAFSURFACES,        bspLeafSurfaces,       MAX_MAP_LEAFSURFACES,      numBSPLeafSurfaces,     sizeof(bspLeafSurfaces[0]),      swapFlag);
  SwapLumpData(LUMP_COLLISIONVERTS,      bspCollisionVerts,     MAX_MAP_COLLISION_VERTS,   numBSPCollisionVerts,   sizeof(bspCollisionVerts[0]),    swapFlag);
  SwapLumpData(LUMP_COLLISIONEDGES,      bspCollisionEdgeData,  MAX_MAP_COLLISION_EDGES,   numBSPCollisionEdges,   sizeof(bspCollisionEdgeData[0]), swapFlag);
  SwapLumpData(LUMP_COLLISIONTRIS,       bspCollisionTriData,   MAX_MAP_COLLISION_TRIS,    numBSPCollisionTris,    sizeof(bspCollisionTriData[0]),  swapFlag);
  SwapLumpData(LUMP_COLLISIONBORDERS,    bspCollisionBorders,   MAX_MAP_COLLISION_BORDERS, numBSPCollisionBorders, sizeof(bspCollisionBorders[0]),  swapFlag);
  SwapLumpData(LUMP_COLLISIONPARTITIONS, bspCollisionParts,     MAX_MAP_COLLISION_PARTS,   numBSPCollisionParts,   sizeof(bspCollisionParts[0]),    swapFlag);
  SwapLumpData(LUMP_COLLISIONAABBS,      bspCollisionAABBs,     MAX_MAP_COLLISION_AABBS,   numBSPCollisionAABBs,   sizeof(bspCollisionAABBs[0]),    swapFlag);
  SwapLumpData(LUMP_MODELS,              bspModels,             MAX_MAP_MODELS,            numBSPModels,           sizeof(bspModels[0]),            swapFlag);
  SwapLumpData(LUMP_VISIBILITY,          bspVisBytes,           MAX_MAP_VISIBILITY,        numBSPVisBytes,         sizeof(bspVisBytes[0]),          swapFlag);
  SwapLumpData(LUMP_ENTITIES,            bspEntData,            MAX_MAP_ENTSTRING,         bspEntDataSize,         sizeof(bspEntData[0]),           swapFlag);
  SwapLumpData(LUMP_PATHCONNECTIONS,     bspPaths,              MAX_MAP_PATHS,             numBSPPaths,            sizeof(bspPaths[0]),             swapFlag);

  return Swap_InitByteSwap();
}

/*
================
SwapAndLoadLump

Swap and load a single BSP lump from file header
================
*/
void SwapAndLoadLump(BspFileHeader_t *header, int lumpIdx, int elemSize, int swapFlag)
{
  byte *fileBase = (byte *)header;
  int lumpSize;
  int count;
  void *data;

  lumpSize = header->lumps[lumpIdx].size;
  
  /* base pointer uses lump 0's offset — first lump data in file */
  data = fileBase + header->lumps[0].offset;

  /* empty lump */
  if ( !header->lumps[lumpIdx].offset )
  {
    count = 0;
  }
  else
  {
    if ( lumpSize % elemSize )
      Com_Error("LoadBspFile: odd lump size");
    count = lumpSize / elemSize;
  }

  SwapLumpData(lumpIdx, data, count, count, elemSize, swapFlag);
}

/*
================
CopyLump

Copy a BSP lump from file into destination buffer
================
*/
int CopyLump(BspFileHeader_t *header, int lumpIdx, void *dest, int elemSize, int maxSize)
{
  byte *fileBase = (byte *)header;
  int length;
  int offset;

  length = header->lumps[lumpIdx].size;
  offset = header->lumps[lumpIdx].offset;

  if ( length % elemSize )
    Com_Error("LoadBspFile: odd lump size");
  if ( length > maxSize )
    Com_Error("LoadBspFile: buffer for lump %i is too small (%i < %i)", lumpIdx, maxSize, length);

  /* copy lump data from file into buffer */
  memcpy(dest, fileBase + offset, length);
  return length / elemSize;
}

/*
================
LoadBSPFile

Load a BSP file from disk into memory
================
*/
void LoadBSPFile(char *filename)
{
  BspFileHeader_t *header;
  int needSwap;

  if ( LoadFile(filename, (void **)&header) < 0 )
    Com_Error("Could not load file '%s'\n", filename);

  /* byte-swap file header */
  SwapBlock(sizeof(BspFileHeader_t), header);

  /* verify BSP magic */
  needSwap = 0;
  if ( header->magic != BSP_IDENT )
  {
    if ( header->magic == BSP_IDENT_SWAP )
    {
      /* opposite endian — re-swap header with correct byte order */
      Swap_Init();
      SwapBlock(sizeof(BspFileHeader_t), header);
      Swap_InitByteSwap();
      needSwap = 1;
    }
    else
    {
      Com_Error("%s is not a IBSP file", filename);
    }
  }

  /* verify BSP version */
  if ( header->version != BSP_VERSION )
    Com_Error("%s is version %i, not %i", filename, header->version, BSP_VERSION);

  /*                                        lump                      dest                   elemSize                          maxBytes                    */
  numBSPMaterials        = CopyLump(header, LUMP_MATERIALS,           bspMaterials,          sizeof(bspMaterials[0]),          sizeof(bspMaterials));
  numBSPLightBytes       = CopyLump(header, LUMP_LIGHTBYTES,          bspLightmapData,       sizeof(bspLightmapData[0]),       sizeof(bspLightmapData));
  numBSPLightGridHash    = CopyLump(header, LUMP_LIGHTGRIDENTRIES,    bspLightGridHash,      sizeof(bspLightGridHash[0]),      sizeof(bspLightGridHash));
  numBSPLightGridColors  = CopyLump(header, LUMP_LIGHTGRIDCOLORS,     bspLightGridColors,    sizeof(bspLightGridColors[0]),    sizeof(bspLightGridColors));
  numBSPPlanes           = CopyLump(header, LUMP_PLANES,              bspPlanes,             sizeof(bspPlanes[0]),             sizeof(bspPlanes));
  numBSPBrushSides       = CopyLump(header, LUMP_BRUSHSIDES,          bspBrushSidesData,     sizeof(bspBrushSidesData[0]),     sizeof(bspBrushSidesData));
  numBSPBrushes          = CopyLump(header, LUMP_BRUSHES,             bspBrushes,            sizeof(bspBrushes[0]),            sizeof(bspBrushes));
  numBSPTriSoups         = CopyLump(header, LUMP_TRIANGLES,           bspTriangles,          sizeof(bspTriangles[0]),          sizeof(bspTriangles));
  numBSPDrawVerts        = CopyLump(header, LUMP_DRAWVERTS,           bspDrawVerts,          sizeof(bspDrawVerts[0]),          sizeof(bspDrawVerts));
  numBSPDrawIndexes      = CopyLump(header, LUMP_DRAWINDICES,         bspDrawIndexes,        sizeof(bspDrawIndexes[0]),        sizeof(bspDrawIndexes));
  numBSPCullGroups       = CopyLump(header, LUMP_CULLGROUPS,          bspCullGroups,         sizeof(bspCullGroups[0]),         sizeof(bspCullGroups));
  numBSPCullGroupIndexes = CopyLump(header, LUMP_CULLGROUPINDICES,    bspCullGroupIndexes,   sizeof(bspCullGroupIndexes[0]),   sizeof(bspCullGroupIndexes));
  numBSPShadowVerts      = CopyLump(header, LUMP_OBSOLETE_1,          bspShadowVerts,        sizeof(bspShadowVerts[0]),        sizeof(bspShadowVerts));
  numBSPShadowIndices    = CopyLump(header, LUMP_OBSOLETE_2,          bspShadowIndexes,      sizeof(bspShadowIndexes[0]),      sizeof(bspShadowIndexes));
  numBSPShadowClusters   = CopyLump(header, LUMP_OBSOLETE_3,          bspShadowClusters,     sizeof(bspShadowClusters[0]),     sizeof(bspShadowClusters));
  numBSPShadowAabbTrees  = CopyLump(header, LUMP_OBSOLETE_4,          bspShadowData,         sizeof(bspShadowData[0]),         sizeof(bspShadowData));
  numBSPShadowSources    = CopyLump(header, LUMP_OBSOLETE_5,          bspShadowSources,      sizeof(bspShadowSources[0]),      sizeof(bspShadowSources));
  numBSPPortalVerts      = CopyLump(header, LUMP_PORTALVERTS,         bspPortalVerts,        sizeof(bspPortalVerts[0]),        sizeof(bspPortalVerts));
  numBSPOccluders        = CopyLump(header, LUMP_OCCLUDER,            bspOccluders,          sizeof(bspOccluders[0]),          sizeof(bspOccluders));
  numBSPOccluderPlanes   = CopyLump(header, LUMP_OCCLUDERPLANES,      bspOccluderPlanes,     sizeof(bspOccluderPlanes[0]),     sizeof(bspOccluderPlanes));
  numBSPOccluderEdges    = CopyLump(header, LUMP_OCCLUDEREDGES,       bspOccluderEdges,      sizeof(bspOccluderEdges[0]),      sizeof(bspOccluderEdges));
  numBSPOccluderIndexes  = CopyLump(header, LUMP_OCCLUDERINDICES,     bspOccluderIndexes,    sizeof(bspOccluderIndexes[0]),    sizeof(bspOccluderIndexes));
  numBSPAabbTrees        = CopyLump(header, LUMP_AABBTREES,           bspAabbTrees,          sizeof(bspAabbTrees[0]),          sizeof(bspAabbTrees));
  numBSPCells            = CopyLump(header, LUMP_CELLS,               bspCells,              sizeof(bspCells[0]),              sizeof(bspCells));
  numBSPPortals          = CopyLump(header, LUMP_PORTALS,             bspPortals,            sizeof(bspPortals[0]),            sizeof(bspPortals));
  numBSPNodes            = CopyLump(header, LUMP_NODES,               bspNodes,              sizeof(bspNodes[0]),              sizeof(bspNodes));
  numBSPLeafs            = CopyLump(header, LUMP_LEAFS,               bspLeafs,              sizeof(bspLeafs[0]),              sizeof(bspLeafs));
  numBSPLeafBrushes      = CopyLump(header, LUMP_LEAFBRUSHES,         bspLeafBrushes,        sizeof(bspLeafBrushes[0]),        sizeof(bspLeafBrushes));
  numBSPLeafSurfaces     = CopyLump(header, LUMP_LEAFSURFACES,        bspLeafSurfaces,       sizeof(bspLeafSurfaces[0]),       sizeof(bspLeafSurfaces));
  numBSPCollisionVerts   = CopyLump(header, LUMP_COLLISIONVERTS,      bspCollisionVerts,     sizeof(bspCollisionVerts[0]),     sizeof(bspCollisionVerts));
  numBSPCollisionEdges   = CopyLump(header, LUMP_COLLISIONEDGES,      bspCollisionEdgeData,  sizeof(bspCollisionEdgeData[0]),  sizeof(bspCollisionEdgeData));
  numBSPCollisionTris    = CopyLump(header, LUMP_COLLISIONTRIS,       bspCollisionTriData,   sizeof(bspCollisionTriData[0]),   sizeof(bspCollisionTriData));
  numBSPCollisionBorders = CopyLump(header, LUMP_COLLISIONBORDERS,    bspCollisionBorders,   sizeof(bspCollisionBorders[0]),   sizeof(bspCollisionBorders));
  numBSPCollisionParts   = CopyLump(header, LUMP_COLLISIONPARTITIONS, bspCollisionParts,     sizeof(bspCollisionParts[0]),     sizeof(bspCollisionParts));
  numBSPCollisionAABBs   = CopyLump(header, LUMP_COLLISIONAABBS,      bspCollisionAABBs,     sizeof(bspCollisionAABBs[0]),     sizeof(bspCollisionAABBs));
  numBSPModels           = CopyLump(header, LUMP_MODELS,              bspModels,             sizeof(bspModels[0]),             sizeof(bspModels));
  numBSPVisBytes         = CopyLump(header, LUMP_VISIBILITY,          bspVisBytes,           sizeof(bspVisBytes[0]),           sizeof(bspVisBytes));
  bspEntDataSize         = CopyLump(header, LUMP_ENTITIES,            bspEntData,            sizeof(bspEntData[0]),            sizeof(bspEntData));
  numBSPPaths            = CopyLump(header, LUMP_PATHCONNECTIONS,     bspPaths,              sizeof(bspPaths[0]),              sizeof(bspPaths));

  free(header);
  if ( needSwap )
    SwapBSPFile(1);
}

/*
================
AddLump

Add a lump to the BSP file being written
================
*/
void AddLump(FILE *fp, BspFileHeader_t *header, int lumpIdx, void *data, int length)
{
  /* set lump offset on first write */
  if ( !header->lumps[lumpIdx].offset )
    header->lumps[lumpIdx].offset = BigLong(ftell(fp));

  /* accumulate lump size */
  header->lumps[lumpIdx].size += BigLong(length);

  /* write data, padded to 4-byte boundary */
  SafeWrite(fp, data, (length + 3) & ~3);
}

/*
================
WriteBSPFile

Write the BSP file to disk
================
*/
int WriteBSPFile(const char *filename, int swapFlag)
{
  FILE *fp;
  unsigned int fileAttrs;
  int result;
  BspFileHeader_t header;
  char text[MAX_OS_PATH];
  int needReswap;

  if ( numBSPNodes > SHRT_MAX )
    Com_Error(
      "numnodes is %d, exceeds limit of %d.\n"
      "Blocksize is a possible cause, try increasing it (0 will set it as large as possible).\n"
      "Also, you might try making geometry detail.\n",
      numBSPNodes, SHRT_MAX);

  needReswap = (swapFlag == 0);
  memset(&header, 0, sizeof(header));

  if ( !swapFlag )
    SwapBSPFile(swapFlag);

  /* open output file */
  fp = fopen(filename, "wb");
  if ( !fp )
  {
    fileAttrs = GetFileAttributesA(filename);
    if ( fileAttrs == INVALID_FILE_ATTRIBUTES || !(fileAttrs & FILE_ATTRIBUTE_READONLY) )
      Com_Error("could not open '%s' for writing\n", filename);

    /* prompt to replace read-only file */
    Com_sprintf(text, sizeof(text), "could not open '%s' for writing; replace read-only file?", filename);
    result = MessageBoxA(0, text, "OUTPUT FILE IS READ ONLY", MB_OKCANCEL | MB_ICONEXCLAMATION);
    if ( result == IDOK )
    {
      SetFileAttributesA(filename, fileAttrs & ~FILE_ATTRIBUTE_READONLY);
      fp = fopen(filename, "wb");
      if ( !fp )
        Com_Error("could not open '%s' for writing\n", filename);
    }
  }
  if ( fp )
  {
    /* write BSP header placeholder */
    header.magic = BigLong(BSP_IDENT);
    header.version = BigLong(BSP_VERSION);
    SafeWrite(fp, &header, sizeof(header));
	
    /*                   lump                      data                   length(elemSize                 * count)                */
    AddLump(fp, &header, LUMP_MATERIALS,           bspMaterials,          sizeof(bspMaterials[0])         * numBSPMaterials);
    AddLump(fp, &header, LUMP_LIGHTBYTES,          bspLightmapData,       sizeof(bspLightmapData[0])      * numBSPLightBytes);
    AddLump(fp, &header, LUMP_LIGHTGRIDENTRIES,    bspLightGridHash,      sizeof(bspLightGridHash[0])     * numBSPLightGridHash);
    AddLump(fp, &header, LUMP_LIGHTGRIDCOLORS,     bspLightGridColors,    sizeof(bspLightGridColors[0])   * numBSPLightGridColors);
    AddLump(fp, &header, LUMP_PLANES,              bspPlanes,             sizeof(bspPlanes[0])            * numBSPPlanes);
    AddLump(fp, &header, LUMP_BRUSHSIDES,          bspBrushSidesData,     sizeof(bspBrushSidesData[0])    * numBSPBrushSides);
    AddLump(fp, &header, LUMP_BRUSHES,             bspBrushes,            sizeof(bspBrushes[0])           * numBSPBrushes);
    AddLump(fp, &header, LUMP_TRIANGLES,           bspTriangles,          sizeof(bspTriangles[0])         * numBSPTriSoups);
    AddLump(fp, &header, LUMP_DRAWVERTS,           bspDrawVerts,          sizeof(bspDrawVerts[0])         * numBSPDrawVerts);
    AddLump(fp, &header, LUMP_DRAWINDICES,         bspDrawIndexes,        sizeof(bspDrawIndexes[0])       * numBSPDrawIndexes);
    AddLump(fp, &header, LUMP_CULLGROUPS,          bspCullGroups,         sizeof(bspCullGroups[0])        * numBSPCullGroups);
    AddLump(fp, &header, LUMP_CULLGROUPINDICES,    bspCullGroupIndexes,   sizeof(bspCullGroupIndexes[0])  * numBSPCullGroupIndexes);
    AddLump(fp, &header, LUMP_OBSOLETE_1,          bspShadowVerts,        sizeof(bspShadowVerts[0])       * numBSPShadowVerts);
    AddLump(fp, &header, LUMP_OBSOLETE_2,          bspShadowIndexes,      sizeof(bspShadowIndexes[0])     * numBSPShadowIndices);
    AddLump(fp, &header, LUMP_OBSOLETE_3,          bspShadowClusters,     sizeof(bspShadowClusters[0])    * numBSPShadowClusters);
    AddLump(fp, &header, LUMP_OBSOLETE_4,          bspShadowData,         sizeof(bspShadowData[0])        * numBSPShadowAabbTrees);
    AddLump(fp, &header, LUMP_OBSOLETE_5,          bspShadowSources,      sizeof(bspShadowSources[0])     * numBSPShadowSources);
    AddLump(fp, &header, LUMP_PORTALVERTS,         bspPortalVerts,        sizeof(bspPortalVerts[0])       * numBSPPortalVerts);
    AddLump(fp, &header, LUMP_OCCLUDER,            bspOccluders,          sizeof(bspOccluders[0])         * numBSPOccluders);
    AddLump(fp, &header, LUMP_OCCLUDERPLANES,      bspOccluderPlanes,     sizeof(bspOccluderPlanes[0])    * numBSPOccluderPlanes);
    AddLump(fp, &header, LUMP_OCCLUDEREDGES,       bspOccluderEdges,      sizeof(bspOccluderEdges[0])     * numBSPOccluderEdges);
    AddLump(fp, &header, LUMP_OCCLUDERINDICES,     bspOccluderIndexes,    sizeof(bspOccluderIndexes[0])   * numBSPOccluderIndexes);
    AddLump(fp, &header, LUMP_AABBTREES,           bspAabbTrees,          sizeof(bspAabbTrees[0])         * numBSPAabbTrees);
    AddLump(fp, &header, LUMP_CELLS,               bspCells,              sizeof(bspCells[0])             * numBSPCells);
    AddLump(fp, &header, LUMP_PORTALS,             bspPortals,            sizeof(bspPortals[0])           * numBSPPortals);
    AddLump(fp, &header, LUMP_NODES,               bspNodes,              sizeof(bspNodes[0])             * numBSPNodes);
    AddLump(fp, &header, LUMP_LEAFS,               bspLeafs,              sizeof(bspLeafs[0])             * numBSPLeafs);
    AddLump(fp, &header, LUMP_LEAFBRUSHES,         bspLeafBrushes,        sizeof(bspLeafBrushes[0])       * numBSPLeafBrushes);
    AddLump(fp, &header, LUMP_LEAFSURFACES,        bspLeafSurfaces,       sizeof(bspLeafSurfaces[0])      * numBSPLeafSurfaces);
    AddLump(fp, &header, LUMP_COLLISIONVERTS,      bspCollisionVerts,     sizeof(bspCollisionVerts[0])    * numBSPCollisionVerts);
    AddLump(fp, &header, LUMP_COLLISIONEDGES,      bspCollisionEdgeData,  sizeof(bspCollisionEdgeData[0]) * numBSPCollisionEdges);
    AddLump(fp, &header, LUMP_COLLISIONTRIS,       bspCollisionTriData,   sizeof(bspCollisionTriData[0])  * numBSPCollisionTris);
    AddLump(fp, &header, LUMP_COLLISIONBORDERS,    bspCollisionBorders,   sizeof(bspCollisionBorders[0])  * numBSPCollisionBorders);
    AddLump(fp, &header, LUMP_COLLISIONPARTITIONS, bspCollisionParts,     sizeof(bspCollisionParts[0])    * numBSPCollisionParts);
    AddLump(fp, &header, LUMP_COLLISIONAABBS,      bspCollisionAABBs,     sizeof(bspCollisionAABBs[0])    * numBSPCollisionAABBs);
    AddLump(fp, &header, LUMP_MODELS,              bspModels,             sizeof(bspModels[0])            * numBSPModels);
    AddLump(fp, &header, LUMP_VISIBILITY,          bspVisBytes,           sizeof(bspVisBytes[0])          * numBSPVisBytes);
    AddLump(fp, &header, LUMP_ENTITIES,            bspEntData,            sizeof(bspEntData[0])           * bspEntDataSize);
    if ( numBSPPaths > 0 )
      AddLump(fp, &header, LUMP_PATHCONNECTIONS,   bspPaths,              sizeof(bspPaths[0])             * numBSPPaths);

    /* re-swap header if needed and write it at file start */
    if ( needReswap )
    {
      Swap_Init();
      SwapBlock(sizeof(header), &header);
      Swap_InitByteSwap();
    }
    fseek(fp, 0, SEEK_SET);
    SafeWrite(fp, &header, sizeof(header));
    return fclose(fp);
  }
  return 0;
}

/*
================
PrintBSPLumpSize

Print a single BSP lump's name, count, and size
================
*/
int PrintBSPLumpSize(const char *name, int count, int size, float pctMultiplier)
{
  char buf[16];

  buf[0] = '\0';
  if ( count >= 0 )
    _itoa(count, buf, 10);
  return printf("%6s %-17s %8i B %6.0f KB %5.1f%%\n", buf, name, size, size / 1024.0, pctMultiplier * size);
}

/*
================
ParseEPair

Parse a key-value epair from entity text.
Allocates a structure: [0]=next ptr, [1]=key string, [2]=value string.
================
*/
Epair_t *ParseEPair(char *str, char **parsePos)
{
  Epair_t *ep;
  char *val;
  char *p;

  ep = malloc(sizeof(*ep));
  ep->next = NULL;
  ep->key = NULL;
  ep->value = NULL;

  /* validate key */
  if ( strlen(str) >= MAX_KEY - 1 )
    Com_Error("ParseEpair: token too long");
  if ( str[strlen(str) - 2] == '\\' )
    Com_Error("ParseEpair: key '%s' ends with a '\\'\n", str);
  if ( strchr(str, '\n') || strchr(str, '\r') )
    Com_Error("ParseEpair: key '%s' contains a newline character\n", str);
  if ( strchr(str, '"') )
    Com_Error("ParseEpair: key '%s' contains a \" character, will cause parsing errors\n", str);
  ep->key = CopyString(str);

  /* validate value */
  val = COM_ParseExt(parsePos);
  if ( strlen(val) >= MAX_TOKEN_CHARS - 1 )
    Com_Error("ParseEpair: token too long");
  if ( val[strlen(val) - 2] == '\\' )
    Com_Error("ParseEpair: value '%s' ends with a '\\'\n", val);
  if ( strchr(val, '\n') || strchr(val, '\r') )
    Com_Error("ParseEpair: value '%s' contains a newline character (use of '\\' at end of value?)\n", val);
  if ( strchr(val, '"') )
    Com_Error("ParseEpair: value '%s' contains a \" character, will cause parsing errors\n", val);
  ep->value = CopyString(val);

  /* strip trailing whitespace from key */
  for ( p = (char *)&ep->key[strlen(ep->key) - 1]; p >= (char *)ep->key; *p-- = '\0' )
  {
    if ( *p > ' ' )
      break;
  }

  /* strip trailing whitespace from value */
  for ( p = (char *)&ep->value[strlen(ep->value) - 1]; p >= (char *)ep->value; *p-- = '\0' )
  {
    if ( *p > ' ' )
      break;
  }
  return ep;
}

/*
================
ParseEntity

Parse a single entity from BSP entity data
================
*/
int ParseEntity(char **parsePos)
{
  char *token;
  Epair_t *ep;
  Entity_t *ent;

  token = COM_Parse(parsePos);
  if ( !*token )
    return 0;
  if ( strcmp(token, "{") )
    Com_Error("ParseEntity: { not found");
  if ( num_entities == MAX_MAP_ENTITIES )
    Com_Error("num_entities == MAX_MAP_ENTITIES");

  ent = &g_entities[num_entities];
  num_entities++;

  /* parse key-value pairs until closing brace */
  while ( 1 )
  {
    token = COM_Parse(parsePos);
    if ( !*token )
      Com_Error("ParseEntity: EOF without closing brace");
    if ( !strcmp(token, "}") )
      break;
    ep = ParseEPair(token, parsePos);
    ep->next = ent->epairs;
    ent->epairs = ep;
  }
  return 1;
}

/*
================
ParseEntities

Parse all entities from BSP entity data string
================
*/
int ParseEntities()
{
  char *parsePos;

  num_entities = 0;
  Com_BeginParseSession("LUMP_ENTITIES");
  parsePos = bspEntData;
  while ( ParseEntity(&parsePos) )
    ;
  return 0;
}

/*
================
UnparseEntities

Serialize all entities back to BSP entity data string
================
*/
int UnparseEntities()
{
  char *end;
  Entity_t *ent;
  Epair_t *ep;
  char *p;
  int i;
  size_t lineLen;
  
  /* contiguous buffer: line(2048) + key(1024) + value(1024) = 4096 bytes
     trim loops use cross-array pointer math — layout must not change */
  char buf[MAXPRINTMSG];
  #define lineBuf  buf[0]
  #define linePad  ((char *)&buf[1])
  #define keyBuf   (&buf[2048])
  #define valueBuf (&buf[3072])

  end = bspEntData;
  bspEntData[0] = '\0';

  for ( i = 0; i < num_entities; i++ )
  {
    ent = &g_entities[i];
    if ( !ent->epairs )
      continue;

    /* open entity block */
    memcpy(end, "{\n", 3);
    end += 2;

    /* write key-value pairs */
    for ( ep = ent->epairs; ep; ep = ep->next )
    {
      /* copy and trim key */
      I_strncpyz(keyBuf, ep->key, 1024);
      for ( p = &linePad[strlen(keyBuf) + 2046]; p >= keyBuf; *p-- = '\0' )
      {
        if ( *p > ' ' )
          break;
      }

      /* copy and trim value */
      I_strncpyz(valueBuf, ep->value, 1024);
      for ( p = &keyBuf[strlen(valueBuf) + 1023]; p >= valueBuf; *p-- = '\0' )
      {
        if ( *p > ' ' )
          break;
      }

      Com_sprintf(&lineBuf, 2048, "\"%s\" \"%s\"\n", keyBuf, valueBuf);
      lineLen = strlen(&lineBuf);
      memcpy(end, &lineBuf, lineLen + 1);
      end += lineLen;
    }

    /* close entity block */
    memcpy(end, "}\n", 3);
    end += 2;
    Assert(end == &bspEntData[strlen(bspEntData)], s_assertDisable_UnparseEntities);
    if ( end > bspEntData + MAX_MAP_ENTSTRING )
      Com_Error("Entity string buffer overflow.  This is caused by too many entities and/or too many key pairs."
                      "  Max may need to be increased.");
  }
  bspEntDataSize = (int)(end - bspEntData + 1);
  return num_entities;
}
#undef lineBuf
#undef linePad
#undef keyBuf
#undef valueBuf

/*
================
UnparseEntitiesWithOrigins

Serialize entities back to BSP data, injecting "origin" keys for brush models
================
*/
int UnparseEntitiesWithOrigins()
{
  Entity_t *ent;
  Epair_t *ep, *newEp;
  char *p;
  char *modelLine, *afterLine;
  char *end, *writePos;
  int i;
  size_t lineLen;
  
  /* contiguous buffer: line(2304) + key(1024) + value(1024) = 4352 bytes
     trim loops use cross-array pointer math — layout must not change */
  char buf[4352];
  #define line   buf
  #define key2   (&buf[2304])
  #define value2 (&buf[3328])
  char origin[MAX_OS_PATH_SHORT];
  char originBuf[32];

  /* pass 1: find brush model entities and inject "origin" key */
  for ( i = 0; i < num_entities; i++ )
  {
    ent = &g_entities[i];
    if ( !ent->epairs )
      continue;

    /* find the "model" "*N" epair */
    for ( ep = ent->epairs; ep; ep = ep->next )
    {
      if ( !strstr(ep->key, "model") || !strstr(ep->value, "*") )
        continue;

      /* trim key */
      I_strncpyz(value2, ep->key, 1024);
      for ( p = &key2[strlen(value2) + 1023]; p >= value2; *p-- = '\0' )
      {
        if ( *p > ' ' )
          break;
      }

      /* trim value */
      I_strncpyz(key2, ep->value, 1024);
      for ( p = &line[strlen(key2) + 2303]; p >= key2; *p-- = '\0' )
      {
        if ( *p > ' ' )
          break;
      }

      /* find this key-value pair in the entity string */
      Com_sprintf(origin, sizeof(origin), "\"%s\" \"%s\"\n", value2, key2);
      modelLine = strstr(bspEntData, origin);
      if ( modelLine )
        break;
    }

    if ( ep )
    {
      /* extract origin value from the line after the model key */
      memset(originBuf, 0, sizeof(originBuf));
      afterLine = &modelLine[strlen(origin) + 1];
      if ( *afterLine != '"' )
      {
        int idx = 0;
        do
        {
          originBuf[idx] = *afterLine;
          ++afterLine;
          ++idx;
          line[idx + 2048] = *afterLine;
        }
        while ( *afterLine != '"' );
      }

      /* inject origin epair */
      newEp = malloc(sizeof(*newEp));
      newEp->next = ent->epairs->next;
      ent->epairs->next = newEp;
      newEp->key = CopyString("origin");
      newEp->value = CopyString(originBuf);
    }
  }

  /* pass 2: serialize all entities back to string */
  end = bspEntData;
  bspEntData[0] = '\0';

  for ( i = 0; i < num_entities; i++ )
  {
    ent = &g_entities[i];
    if ( !ent->epairs )
      continue;

    /* open entity block */
    memcpy(end, "{\n", 3);
    writePos = end + 2;

    /* write key-value pairs */
    for ( ep = ent->epairs; ep; ep = ep->next )
    {
      /* trim key */
      I_strncpyz(value2, ep->key, 1024);
      for ( p = &key2[strlen(value2) + 1023]; p >= value2; *p-- = '\0' )
      {
        if ( *p > ' ' )
          break;
      }

      /* trim value */
      I_strncpyz(key2, ep->value, 1024);
      for ( p = &line[strlen(key2) + 2303]; p >= key2; *p-- = '\0' )
      {
        if ( *p > ' ' )
          break;
      }

      Com_sprintf(line, 2304, "\"%s\" \"%s\"\n", value2, key2);
      lineLen = strlen(line);
      memcpy(writePos, line, lineLen + 1);
      writePos += lineLen;
    }

    /* close entity block */
    memcpy(writePos, "}\n", 3);
    end = writePos + 2;
    if ( end > bspEntData + MAX_MAP_ENTSTRING )
      Com_Error("Entity string buffer overflow.  This is caused by too many entities and/or too many key pairs."
                      "  Max may need to be increased.");
  }

  bspEntDataSize = (int)(end - bspEntData + 1);
  return num_entities;
}
#undef line
#undef key2
#undef value2

/*
================
SetKeyValue

Set a key-value pair on an entity, replacing existing or creating new
================
*/
char *SetKeyValue(Entity_t *entity, const char *key, const char *value)
{
  Epair_t *ep;

  /* check for existing key */
  for ( ep = entity->epairs; ep; ep = ep->next )
  {
    if ( !strcmp(ep->key, key) )
    {
      free((char *)ep->value);
      ep->value = CopyString(value);
      return (char *)ep->value;
    }
  }

  /* create new epair */
  ep = malloc(sizeof(*ep));
  ep->next = entity->epairs;
  entity->epairs = ep;
  ep->key = CopyString(key);
  ep->value = CopyString(value);
  return (char *)ep->value;
}

/*
================
ValueForKey

Get the string value for a key on an entity
================
*/
const char *ValueForKey(Entity_t *entity, const char *key)
{
  Epair_t *ep;

  for ( ep = entity->epairs; ep; ep = ep->next )
  {
    if ( !strcmp(ep->key, key) )
      return ep->value;
  }
  return g_emptyString;
}

/*
================
FloatForKey

Get a float value for a key on an entity.
================
*/
double FloatForKey(Entity_t *ent, const char *key)
{
  return atof(ValueForKey(ent, key));
}

/*
================
GetVectorForKey

Parse a vector from an entity's key-value pair.
================
*/
float *GetVectorForKey(Entity_t *entity, const char *key, float *out)
{
  const char *val;
  double parsed[3];
  int n;

  val = ValueForKey(entity, key);
  parsed[0] = 0.0;
  parsed[1] = 0.0;
  parsed[2] = 0.0;

  n = sscanf(val, "%lf %lf %lf", &parsed[0], &parsed[1], &parsed[2]);

  /* warn if incomplete vector */
  if ( n == 1 || n == 2 )
    printf(
      "WARNING: entity %i: key '%s' with value '%s', when it should be a vector; treating as '%g %g %g'\n",
      (int)(entity - g_entities), key, val, parsed[0], parsed[1], parsed[2]);

  out[0] = (float)parsed[0];
  out[1] = (float)parsed[1];
  out[2] = (float)parsed[2];
  return out;
}

/*
================
LoadAndWriteBSPFile

Load a BSP file and immediately write it back out (for format conversion)
================
*/
int LoadAndWriteBSPFile(char *inputPath, const char *lpFileName, int swapFlag)
{
  LoadBSPFile(inputPath);
  return WriteBSPFile(lpFileName, swapFlag);
}

/*
================
SetBSPFileExtensions

Set BSP/PRT/poly file extensions based on root string (e.g., "d" -> ".dbsp", ".dprt", ".dpoly")
================
*/
int SetBSPFileExtensions(const char *root)
{
  Assert(strlen(root) + 3 < 10, s_assertDisable_SetBSPFileExtensions);
  Assert(strlen(root) + 3 < 10, s_assertDisable_SetBSPFileExtensions);
  Assert(strlen(root) + 4 < 10, s_assertDisable_SetBSPFileExtensions);
  Com_sprintf(Buffer, sizeof(Buffer), ".%sbsp", root);
  Com_sprintf(g_prtFileExt, sizeof(g_prtFileExt), ".%sprt", root);
  return Com_sprintf(g_polyFileExt, sizeof(g_polyFileExt), ".%spoly", root);
}

/*
================
GetBSPFileExtension / GetPRTFileExtension / GetPolyFileExtension

MUST NOT be inlined — MSVC /O2 inlines into callers with local Buffer[1024],
shadowing the global Buffer[16]. Generated from macro matching original pattern.
================
*/
#define DEFINE_GET_FILE_EXT(funcName, extVar, assertStr, assertLine) \
char *funcName() \
{ \
  Assert(extVar[0], s_assertDisable_##funcName); \
  return extVar; \
}

DEFINE_GET_FILE_EXT(GetBSPFileExtension,  Buffer,         "bspFileExtension[0]",  1623)
DEFINE_GET_FILE_EXT(GetPRTFileExtension,  g_prtFileExt,   "prtFileExtension[0]",  1630)
DEFINE_GET_FILE_EXT(GetPolyFileExtension, g_polyFileExt,  "polyFileExtension[0]", 1637)

#undef DEFINE_GET_FILE_EXT

/*
================
PrintBSPFileSizes

Print sizes of all BSP lumps
================
*/
int PrintBSPFileSizes(int totalSize)
{
  float pct;

  if ( !num_entities )
    ParseEntities();
  pct = 100.0f / totalSize;

  /*                name                count                              % of BSP(elemSize                 * count, scale )             */
  /* geometry */
  PrintBSPLumpSize("models",            numBSPModels,                        sizeof(bspModels[0])            * numBSPModels, pct);
  PrintBSPLumpSize("materials",         numBSPMaterials,                     sizeof(bspMaterials[0])         * numBSPMaterials, pct);
  PrintBSPLumpSize("brushes",           numBSPBrushes,                       sizeof(bspBrushes[0])           * numBSPBrushes, pct);
  PrintBSPLumpSize("brushsides",        numBSPBrushSides,                    sizeof(bspBrushSidesData[0])    * numBSPBrushSides, pct);
  PrintBSPLumpSize("planes",            numBSPPlanes,                        sizeof(bspPlanes[0])            * numBSPPlanes, pct);
  PrintBSPLumpSize("entdata",           num_entities,                        bspEntDataSize, pct);
  printf("\n");

  /* BSP tree */
  PrintBSPLumpSize("nodes",             numBSPNodes,                         sizeof(bspNodes[0])             * numBSPNodes, pct);
  PrintBSPLumpSize("leafs",             numBSPLeafs,                         sizeof(bspLeafs[0])             * numBSPLeafs, pct);
  PrintBSPLumpSize("leafbrushes",       numBSPLeafBrushes,                   sizeof(bspLeafBrushes[0])       * numBSPLeafBrushes, pct);
  PrintBSPLumpSize("leafsurfaces",      numBSPLeafSurfaces,                  sizeof(bspLeafSurfaces[0])      * numBSPLeafSurfaces, pct);

  /* collision */
  PrintBSPLumpSize("collisionverts",    numBSPCollisionVerts,                sizeof(bspCollisionVerts[0])    * numBSPCollisionVerts, pct);
  PrintBSPLumpSize("collisionedges",    numBSPCollisionEdges,                sizeof(bspCollisionEdgeData[0]) * numBSPCollisionEdges, pct);
  PrintBSPLumpSize("collisiontris",     numBSPCollisionTris,                 sizeof(bspCollisionTriData[0])  * numBSPCollisionTris, pct);
  PrintBSPLumpSize("collisionborders",  numBSPCollisionBorders,              sizeof(bspCollisionBorders[0])  * numBSPCollisionBorders, pct);
  PrintBSPLumpSize("collisionparts",    numBSPCollisionParts,                sizeof(bspCollisionParts[0])    * numBSPCollisionParts, pct);
  PrintBSPLumpSize("collisionaabbs",    numBSPCollisionAABBs,                sizeof(bspCollisionAABBs[0])    * numBSPCollisionAABBs, pct);

  /* draw surfaces */
  PrintBSPLumpSize("drawverts",         numBSPDrawVerts,                     sizeof(bspDrawVerts[0])         * numBSPDrawVerts, pct);
  PrintBSPLumpSize("drawindexes",       numBSPDrawIndexes,                   sizeof(bspDrawIndexes[0])       * numBSPDrawIndexes, pct);
  PrintBSPLumpSize("trianglesoups",     numBSPTriSoups,                      sizeof(bspTriangles[0])         * numBSPTriSoups, pct);

  /* shadows */
  PrintBSPLumpSize("shadowverts",       numBSPShadowVerts,                   sizeof(bspShadowVerts[0])       * numBSPShadowVerts, pct);
  PrintBSPLumpSize("shadowindices",     numBSPShadowIndices,                 sizeof(bspShadowIndexes[0])     * numBSPShadowIndices, pct);
  PrintBSPLumpSize("shadowclusters",    numBSPShadowClusters,                sizeof(bspShadowClusters[0])    * numBSPShadowClusters, pct);
  PrintBSPLumpSize("shadowaabbtrees",   numBSPShadowAabbTrees,               sizeof(bspShadowData[0])        * numBSPShadowAabbTrees, pct);
  PrintBSPLumpSize("shadowsources",     numBSPShadowSources,                 sizeof(bspShadowSources[0])     * numBSPShadowSources, pct);

  /* lighting */
  PrintBSPLumpSize("lightmaps",         numBSPLightBytes / LIGHTMAP_BYTES,   numBSPLightBytes, pct);
  PrintBSPLumpSize("light grid hash",   numBSPLightGridHash,                 sizeof(bspLightGridHash[0])     * numBSPLightGridHash, pct);
  PrintBSPLumpSize("light grid values", numBSPLightGridColors,               sizeof(bspLightGridColors[0])   * numBSPLightGridColors, pct);
  PrintBSPLumpSize("lights",            numBSPLights,                        sizeof(bspLights[0])            * numBSPLights, pct);
  PrintBSPLumpSize("visibility",        -1,                                  numBSPVisBytes, pct);

  /* portals and occluders */
  PrintBSPLumpSize("portalverts",       numBSPPortalVerts,                   sizeof(bspPortalVerts[0])       * numBSPPortalVerts, pct);
  PrintBSPLumpSize("occluders",         numBSPOccluders,                     sizeof(bspOccluders[0])         * numBSPOccluders, pct);
  PrintBSPLumpSize("occluderplanes",    numBSPOccluderPlanes,                sizeof(bspOccluderPlanes[0])    * numBSPOccluderPlanes, pct);
  PrintBSPLumpSize("occluderedges",     numBSPOccluderEdges,                 sizeof(bspOccluderEdges[0])     * numBSPOccluderEdges, pct);
  PrintBSPLumpSize("occluderindexes",   numBSPOccluderIndexes,               sizeof(bspOccluderIndexes[0])   * numBSPOccluderIndexes, pct);
  PrintBSPLumpSize("aabbtrees",         numBSPAabbTrees,                     sizeof(bspAabbTrees[0])         * numBSPAabbTrees, pct);
  PrintBSPLumpSize("cells",             numBSPCells,                         sizeof(bspCells[0])             * numBSPCells, pct);
  PrintBSPLumpSize("portals",           numBSPPortals,                       sizeof(bspPortals[0])           * numBSPPortals, pct);
  PrintBSPLumpSize("cullgroups",        numBSPCullGroups,                    sizeof(bspCullGroups[0])        * numBSPCullGroups, pct);
  PrintBSPLumpSize("cullgroupindexes",  numBSPCullGroupIndexes,              sizeof(bspCullGroupIndexes[0])  * numBSPCullGroupIndexes, pct);
  printf("\n");
  return PrintBSPLumpSize("paths",      numBSPPaths != 0,                    numBSPPaths, pct);
}

/*
shadows_midpoint.c — Shadow midpoint auxiliary vertex system

Reconstructed from cod2map.exe by Rose.

The midpoint shadow system projects caster windings through the shadow
view-projection matrix, clips them against occluders, fixes T-junctions,
simplifies concave holes, and triangulates the results for shadow mesh
emission. Uses SmAuxVert_t for projected vertex data.
*/

#include "cod2map.h"

static void ShadowMid_TJuncProcessSurfaceGridCallback(TriSurf_t *ts)
{
  (void)TJunc_ProcessSurface(ts);
}

static void ShadowMid_TjuncFixSurfaceEdgesGridCallback(TriSurf_t *ts)
{
  (void)TjuncFixSurfaceEdges(ts);
}

static int ShadowMid_ProjectWindingThroughOccludersCallback(Winding_t *winding)
{
  return SM_ProjectWindingThroughOccluders(winding) != NULL;
}

static int ShadowMid_FreeWindingCallback(Winding_t *winding)
{
  FreeWinding(winding);
  return 0;
}

static int ShadowMid_CanMergeSurfacesCallback(TriSurf_t *tsA, TriSurf_t *tsB)
{
  return SM_CanMergeSurfaces(tsA, tsB);
}

static int ShadowMid_WindingBehindPlaneCallback(TriSurf_t *tsA, TriSurf_t *tsB)
{
  return ShadowMid_WindingBehindPlane(tsA, tsB);
}

static void ShadowMid_InterpolateVertLerpCallback(float *from, float *to, double frac, float *result)
{
  (void)SM_InterpolateVert(from, to, frac, result);
}


SmCasterTri_t    *g_smCasterTriPool;
SmCasterTri_t    *g_smCurrentCasterTri;
SmOccluderNode_t *g_smOccluderTree;
SmOccluder_t     *g_smOccluderArray;
TriSurf_t        *g_smCasterWindingList;

vec3_t g_smCasterBoundsMax;
vec3_t g_smCasterBoundsMin;
vec3_t g_smGridMaxs;
vec3_t g_smGridMins;
float  g_smInvViewProjMatrix[16];
float  g_smLightDirX;
float  g_smLightDirY;
float  g_smLightDirZ;
float  g_smLightDir_x;
float  g_smLightDir_y;
float  g_smLightDir_z;
float  g_smLightDist;
float  g_smLightOriginX;
float  g_smLightOriginY;
float  g_smLightOriginZ;
int    g_smNumCasterTris;
int    g_smNumOccluders;
char   g_smPerspectiveEnabled;
float  g_smTransposedInvMatrix[17];
float  g_smViewProjMatrix;
float  g_smViewProjMatrix_01;
float  g_smViewProjMatrix_02;
float  g_smViewProjMatrix_03;
float  g_smViewProjMatrix_10;
float  g_smViewProjMatrix_11;
float  g_smViewProjMatrix_12;
float  g_smViewProjMatrix_13;
float  g_smViewProjMatrix_20;
float  g_smViewProjMatrix_21;
float  g_smViewProjMatrix_22;
float  g_smViewProjMatrix_23;
float  g_smViewProjMatrix_30;
float  g_smViewProjMatrix_31;
float  g_smViewProjMatrix_32;
float  g_smViewProjMatrix_33;

char s_assertDisable_SM_AddCasterTriangle;
char s_assertDisable_SM_CreateFragment;
char s_assertDisable_SM_FlattenSurfToWinding;
char s_assertDisable_SM_InitOccluder;
char s_assertDisable_SM_InterpolateVert;
char s_assertDisable_SM_ProjectPoint;
char s_assertDisable_SM_ProjectWindingThroughOccluders;
char s_assertDisable_SM_RayPlaneIntersect;
char s_assertDisable_SM_RestoreSurfFromWinding;
char s_assertDisable_ShadowMid_ClipWindingByOccluders_r;
char s_assertDisable_ShadowMid_FindBestNotch;
char s_assertDisable_ShadowMid_FixTJunctions;
char s_assertDisable_ShadowMid_IsConvex;
char s_assertDisable_ShadowMid_PlugNotchesInWinding;
char s_assertDisable_ShadowMid_Shutdown;
char s_assertDisable_ShadowMid_WindingBehindPlane;


/*
================
ShadowMid_Shutdown

Frees shadow midpoint global buffers (tri array, tree nodes).
Asserts that all fragments have been freed.
================
*/
void ShadowMid_Shutdown(void)
{
  free(g_smCasterTriPool);
  g_smCasterTriPool = NULL;
  g_smNumCasterTris = 0;
  if ( g_smCasterWindingList )
    AssertFatal(!g_smCasterWindingList, s_assertDisable_ShadowMid_Shutdown);
  free(g_smOccluderTree);
  g_smOccluderTree = NULL;
}

/*
================
ShadowCeilWrapper1

ceil() wrapper for shadow math.
================
*/
double ShadowCeilWrapper1(float value)
{
  return ceil(value);
}

/*
================
ShadowCeilWrapper2

ceil() wrapper for shadow math.
================
*/
double ShadowCeilWrapper2(float value)
{
  return ceil(value);
}

/*
================
ShadowMid_ClipWindingByOccluders_r

Recursively clips a winding against occluder side planes.
Accepted fragments go to acceptCallback, rejected to rejectCallback.
================
*/
int ShadowMid_ClipWindingByOccluders_r(Winding_t *winding, int groupId, int (*acceptCallback)(Winding_t *), int (*rejectCallback)(Winding_t *), int startIndex)
{
  SmOccluder_t *occ;
  Winding_t *polygon;
  int i, numSides, skipOccluder;
  int planeSides[4];
  Winding_t *frontOut, *backOut;

  while ( 1 )
  {
    /* find next matching occluder for this group */
    while ( startIndex < g_smNumOccluders )
    {
      if ( g_smOccluderArray[startIndex].casterTri->flag == (char)groupId )
        break;
      startIndex++;
    }
    if ( startIndex >= g_smNumOccluders )
      return acceptCallback(winding);

    occ = &g_smOccluderArray[startIndex];
    polygon = occ->polygon;
    occ->active = 0;
    numSides = polygon->numpoints;
    AssertFatal(numSides >= 3 && numSides <= 4, s_assertDisable_ShadowMid_ClipWindingByOccluders_r);

    /* classify winding against each occluder side plane */
    skipOccluder = 0;
    for ( i = 0; i < numSides; i++ )
    {
      planeSides[i] = WindingPlaneSide(winding, occ->planes[i], occ->planes[i][3]);
      if ( planeSides[i] == SIDE_FRONT )
      { skipOccluder = 1; break; }
    }

    if ( !skipOccluder )
    {
      /* clip winding against each crossing occluder side */
      for ( i = 0; i < numSides; i++ )
      {
        if ( planeSides[i] == SIDE_BACK )
          continue;

        ClipWindingEpsilon(winding, occ->planes[i], occ->planes[i][3], ON_EPSILON, &frontOut, &backOut, 0);
        FreeWinding(winding);

        /* front fragment recurses past this occluder */
        if ( frontOut )
          ShadowMid_ClipWindingByOccluders_r(frontOut, groupId, acceptCallback, rejectCallback, startIndex + 1);

        winding = backOut;
        if ( !winding )
          return 0;
      }

      /* entire winding is behind all sides — rejected by this occluder */
      if ( rejectCallback )
        return rejectCallback(winding);
      occ->active = 1;
    }

    startIndex++;
  }
}

/*
================
ShadowMid_CompareOccluderEntries

qsort comparator for occluder entries: sorts by group ID, then by area.
================
*/
int ShadowMid_CompareOccluderEntries(SmOccluder_t *lhs, SmOccluder_t *rhs)
{
  double areaDiff;

  /* primary sort: group (flag) */
  if ( lhs->casterTri->flag != rhs->casterTri->flag )
    return 1;

  /* secondary sort: area */
  areaDiff = lhs->area - rhs->area;
  if ( areaDiff >= 0.0 )
    return areaDiff > 0.0;
  return -1;
}

/*
================
ShadowMid_ComputeEdgePlane

Computes an edge plane from two projected vertices and a face normal.
Optionally expands the plane distance for shadow volume extrusion.
================
*/
char ShadowMid_ComputeEdgePlane(int vertIdxA, Winding_t *w, int vertIdxB, float *outPlane, float *faceNormal, char expandFlag)
{
  vec3_t edge;

  VectorSubtract(w->points[vertIdxA], w->points[vertIdxB], edge);
  CrossProduct(edge, faceNormal, outPlane);
  VecNormalize(outPlane);
  outPlane[3] = DotProduct210(outPlane, w->points[vertIdxB]);

  /* expand for shadow volume extrusion */
  if ( expandFlag )
    outPlane[3] -= SM_EDGE_EXPAND_DIST;

  return expandFlag;
}

/*
================
ShadowMid_ComputeCornerVertex

Intersects 3 planes to find a corner vertex. Falls back to
bisector projection if the planes don't intersect cleanly.
================
*/
char ShadowMid_ComputeCornerVertex(float *planeA, float *planeB, float *outPoint, float *planeC, float *fallbackPos)
{
  double projDist;
  float *planes[3];
  vec3_t bisector;

  planes[0] = planeC;
  planes[1] = planeA;
  planes[2] = planeB;

  if ( !PlaneIntersection3(planes, outPoint) )
  {
    if ( !fallbackPos )
      return 0;

    /* fallback: bisect planeA and planeC normals, offset from fallbackPos */
    VectorAdd(planeC, planeA, bisector);
    VecNormalize(bisector);
    VectorMA(fallbackPos, -SM_EDGE_EXPAND_DIST, bisector, outPoint);

    /* project onto planeB */
    projDist = planeB[3] - DotProduct(planeB, outPoint);
    VectorMA(outPoint, projDist, planeB, outPoint);
  }

  return 1;
}

/*
================
ShadowMid_PointsMatch3D

Compares two 3D points within a small epsilon (0.001).
================
*/
int ShadowMid_PointsMatch3D(float *pointA, float *pointB)
{
  return VectorCompareEpsilon(pointA, pointB, PLANESIDE_EPSILON, 3);
}

/*
================
ShadowMid_FixTJunctions

Fixes T-junctions in shadow casters by subdividing into a 3D grid
and processing each cell.
================
*/
int ShadowMid_FixTJunctions(void)
{
  float dims[3], stepSize[3];
  int steps[3];
  int ix, iy, iz, i;
  float cellMins[3], cellMaxs[3];

  printf("fixing shadow t-junctions...\n");
  SetGridDivisionPoints(g_smGridMins, g_smGridMaxs);
  TJunc_ProcessBrushList(g_smCasterWindingList);

  /* compute grid dimensions and step counts (128-unit cells) */
  dims[0] = g_smGridMaxs[0] - g_smGridMins[0];
  dims[1] = g_smGridMaxs[1] - g_smGridMins[1];
  dims[2] = g_smGridMaxs[2] - g_smGridMins[2];

  for ( i = 0; i < 3; i++ )
  {
    steps[i] = (int)ceil(dims[i] / SM_GRID_CELL_SIZE + 0.001f);
    AssertFatal(steps[i] > 0, s_assertDisable_ShadowMid_FixTJunctions);
    stepSize[i] = (float)(dims[i] / (double)steps[i] + 0.001f);
  }

  /* subdivide into 3D grid and fix t-junctions per cell */
  for ( iz = 0; iz < steps[2]; iz++ )
  {
    cellMins[2] = FMA1(g_smGridMins[2], (float)iz, stepSize[2]);
    cellMaxs[2] = cellMins[2] + stepSize[2];

    for ( iy = 0; iy < steps[1]; iy++ )
    {
      cellMins[1] = FMA1(g_smGridMins[1], (float)iy, stepSize[1]);
      cellMaxs[1] = cellMins[1] + stepSize[1];

      for ( ix = 0; ix < steps[0]; ix++ )
      {
        cellMins[0] = FMA1(g_smGridMins[0], (float)ix, stepSize[0]);
        cellMaxs[0] = cellMins[0] + stepSize[0];

        GridTree_ForEach(cellMins, cellMaxs, ShadowMid_TJuncProcessSurfaceGridCallback);
        GridTree_ForEach(cellMins, cellMaxs, ShadowMid_TjuncFixSurfaceEdgesGridCallback);
        TjuncReset();
      }
    }
  }

  return 0;
}

/*
================
ShadowMid_SimplifyConcaveHoles

Merges holes in concave shadow casters.
================
*/
void ShadowMid_SimplifyConcaveHoles(void)
{
  TriSurf_t *caster;

  printf("simplifying holes in concave shadow casters...\n");
  for ( caster = g_smCasterWindingList; caster; caster = caster->next )
    MergeHoles(caster);
}

/*
================
ShadowMid_IsPointInsideTriangle2D

2D point-in-triangle test used for notch detection in shadow casters.
================
*/
bool ShadowMid_IsPointInsideTriangle2D(float *prevVert, float *testVert, float *nextVert, float *tipVert)
{
  int minSide, sideB;
  double dotA;
  float edgeNormalA[2], edgeNormalB[2];
  float distA, distB;

  /* edge normal A: tip → prev (2D perpendicular) */
  edgeNormalA[0] = tipVert[1] - prevVert[1];
  edgeNormalA[1] = prevVert[0] - tipVert[0];
  Vec2Normalize(edgeNormalA);
  distA = edgeNormalA[0] * tipVert[0] + edgeNormalA[1] * tipVert[1] + SM_EDGE_PLANE_EPSILON;

  /* edge normal B: next → tip (2D perpendicular) */
  edgeNormalB[0] = nextVert[1] - tipVert[1];
  edgeNormalB[1] = tipVert[0] - nextVert[0];
  Vec2Normalize(edgeNormalB);

  /* determine minimum side count needed */
  minSide = 2;
  if ( edgeNormalA[0] * nextVert[0] + edgeNormalA[1] * nextVert[1] - distA <= 0.0 )
    minSide = 1;

  /* classify test point against both edges */
  dotA = edgeNormalA[0] * testVert[0] + edgeNormalA[1] * testVert[1] - distA;
  distB = edgeNormalB[0] * testVert[0] + edgeNormalB[1] * testVert[1]
        - (edgeNormalB[0] * tipVert[0] + edgeNormalB[1] * tipVert[1] + SM_EDGE_PLANE_EPSILON);
  sideB = dotA > 0.0;

  if ( distB <= 0.0 )
    return sideB >= minSide;
  return sideB + 1 >= minSide;
}

/*
================
ShadowMid_ComputeNotchArea

Computes the signed area of a notch between two windings using
2D cross products. Returns true if the area exceeds minArea.
================
*/
bool ShadowMid_ComputeNotchArea(Winding_t *windingA, Winding_t *windingB, int startIdxB, int startIdxA, int notchSize, float minArea, float *outArea)
{
  int i;
  float *tipVert;
  float *lastVertB;
  float *prevVert;
  float *nextVertA;
  int prevIdx, curIdx;
  double prevX, prevY;

  tipVert = windingB->points[startIdxB];
  lastVertB = windingB->points[(startIdxB + notchSize - 1) % windingB->numpoints];
  prevVert = windingA->points[(windingA->numpoints + startIdxA - 1) % windingA->numpoints];
  nextVertA = windingA->points[(startIdxA + 1) % windingA->numpoints];

  /* reject if edges cross or tip is outside the triangle */
  if ( WindingHasCrossingEdge(windingB, tipVert, lastVertB, 0, 1)
    || WindingHasCrossingEdge(windingA, tipVert, lastVertB, 0, 1)
    || !ShadowMid_IsPointInsideTriangle2D(prevVert, lastVertB, nextVertA, tipVert) )
    return 0;

  /* accumulate signed area using 2D cross products (shoelace formula) */
  *outArea = 0.0;
  for ( i = 2; i < notchSize; i++ )
  {
    prevIdx = (startIdxB + i - 1) % windingB->numpoints;
    curIdx = (startIdxB + i) % windingB->numpoints;
    prevX = windingB->points[prevIdx][0] - tipVert[0];
    prevY = windingB->points[prevIdx][1] - tipVert[1];
    *outArea += (float)((windingB->points[curIdx][1] - tipVert[1]) * prevX
              - (windingB->points[curIdx][0] - tipVert[0]) * prevY);
  }

  return *outArea > minArea;
}

/*
================
ShadowMid_FindBestNotch

Finds the largest-area notch between two windings by iterating
over all possible notch sizes and positions.
================
*/
int ShadowMid_FindBestNotch(Winding_t *windingA, Winding_t *windingB, int startIdxA, int startIdxB, int *outStartIdx, int vertCount)
{
  int outerIdx, innerSize, idxA, idxB;
  int bestNotchSize;
  float notchArea, bestArea;

  bestArea = 0.0f;
  bestNotchSize = 0;

  /* try all notch positions and sizes, largest area wins */
  for ( outerIdx = 0; outerIdx <= vertCount - 3; outerIdx++ )
  {
    idxA = (outerIdx + startIdxA) % windingA->numpoints;
    idxB = (startIdxB + windingB->numpoints - outerIdx) % windingB->numpoints;

    for ( innerSize = vertCount - outerIdx; innerSize >= 3; innerSize-- )
    {
      if ( ShadowMid_ComputeNotchArea(windingB, windingA, idxA, idxB, innerSize, bestArea, &notchArea) )
      {
        Assert(notchArea > (double)bestArea, s_assertDisable_ShadowMid_FindBestNotch);
        bestArea = notchArea;
        bestNotchSize = innerSize;
        *outStartIdx = idxA;
      }
    }
  }

  return bestNotchSize;
}

/*
================
ShadowMid_WindingFromAuxData

Creates a 2D winding from auxiliary vertex data (XY coords, Z=0).
================
*/
Winding_t *ShadowMid_WindingFromAuxData(int numPoints, SmAuxVert_t *auxVerts)
{
  Winding_t *w;
  int i;

  /* extract projected XY into a 2D winding (Z=0) */
  w = AllocWinding(numPoints);
  w->numpoints = numPoints;

  for ( i = 0; i < numPoints; i++ )
  {
    w->points[i][0] = auxVerts[i].proj[0];
    w->points[i][1] = auxVerts[i].proj[1];
    w->points[i][2] = 0.0f;
  }

  return w;
}

/*
================
ShadowMid_PlugNotchesInWinding

Plugs notches from neighbor windings into the base winding.
Iterates neighbors, finds shared edges, and removes notch geometry.
================
*/
int ShadowMid_PlugNotchesInWinding(TriSurf_t **_unused, IntWinding_t **neighbors, int neighborCount, int *auxStride, IntWinding_t *baseWinding)
{
  int i, dupCount, bestNotchSize, bestNotchStart;
  int sharedIdxA, sharedIdxB;
  IntWinding_t *neighborWinding;
  Winding_t *tempWinding, *neighborCopy;
  void *ptsBase;

  ptsBase = baseWinding->auxData;
  tempWinding = ShadowMid_WindingFromAuxData(baseWinding->numpoints, (SmAuxVert_t *)ptsBase);

  for ( i = 0; i < neighborCount; i++ )
  {
    dupCount = FindSharedEdge(baseWinding, neighbors[i], &sharedIdxA, &sharedIdxB);

    Assert(dupCount <= baseWinding->numpoints, s_assertDisable_ShadowMid_PlugNotchesInWinding);

    /* skip if not enough shared edges or entire winding is shared */
    if ( dupCount < 3 || dupCount == baseWinding->numpoints )
      continue;

    /* build 2D winding from neighbor's projected aux data */
    neighborWinding = neighbors[i];
    neighborCopy = ShadowMid_WindingFromAuxData(neighborWinding->numpoints, (SmAuxVert_t *)neighborWinding->auxData);

    /* find best notch to plug */
    bestNotchSize = ShadowMid_FindBestNotch(tempWinding, neighborCopy, sharedIdxA, sharedIdxB, &bestNotchStart, dupCount);
    FreeWinding(neighborCopy);

    if ( bestNotchSize )
    {
      /* remove notch from base winding and rebuild temp */
      FreeWinding(tempWinding);
      RemoveSharedBoundaryPoints(baseWinding, *auxStride, bestNotchStart, bestNotchSize);
      AssertFatal(ptsBase == baseWinding->auxData, s_assertDisable_ShadowMid_PlugNotchesInWinding);
      tempWinding = ShadowMid_WindingFromAuxData(baseWinding->numpoints, (SmAuxVert_t *)ptsBase);
    }
  }

  FreeWinding(tempWinding);
  return neighborCount;
}

/*
================
ShadowMid_WindingBehindPlane

Tests if casterB's winding vertices are all behind casterA's plane.
Returns 1 if all behind, 0 if any vertex is in front.
================
*/
char ShadowMid_WindingBehindPlane(TriSurf_t *tsA, TriSurf_t *tsB)
{
  float *planeA;
  SmAuxVert_t *auxVerts;
  int i;

  if ( tsA == tsB )
    return 0;

  /* casterA's original plane was saved into props->texVecs[0..3] by SM_FlattenSurfToWinding */
  planeA = tsA->props->texVecs;

  Assert(tsB->winding->numpoints > 0, s_assertDisable_ShadowMid_WindingBehindPlane);

  /* test each of casterB's original vertices (from aux data) against casterA's saved plane */
  auxVerts = tsB->auxData;
  for ( i = 0; i < tsB->winding->numpoints; i++ )
  {
    if ( DotProduct201(auxVerts[i].orig, planeA) - planeA[3] > 0.0 )
      return 0;
  }

  return 1;
}

/*
================
ShadowMid_IsConvex

Tests if a surface's winding is convex via 2D cross products.
Returns 1 if convex, 0 if any concave angle is found.
================
*/
char ShadowMid_IsConvex(TriSurf_t *ts)
{
  Winding_t *windingData;
  int numPoints, ptIdx;
  int prevPrevIdx, prevIdx;

  Assert(ts, s_assertDisable_ShadowMid_IsConvex);
  Assert(ts->winding, s_assertDisable_ShadowMid_IsConvex);
  windingData = ts->winding;
  numPoints = windingData->numpoints;
  Assert(numPoints >= 3, s_assertDisable_ShadowMid_IsConvex);

  if ( numPoints == 3 )
    return 1;

  /* check 2D convexity via cross product sign at each vertex */
  prevPrevIdx = numPoints - 2;
  prevIdx = numPoints - 1;
  for ( ptIdx = 0; ptIdx < numPoints; ptIdx++ )
  {
    float *cur = windingData->points[ptIdx];
    float *prev = windingData->points[prevIdx];
    float *prevPrev = windingData->points[prevPrevIdx];

    AssertFatal(prev[2] == 0.0f, s_assertDisable_ShadowMid_IsConvex);

    /* 2D cross product: (cur-prev) x (prevPrev-prev) */
    if ( Det2x2(cur[1] - prev[1], prevPrev[0] - prev[0], cur[0] - prev[0], prevPrev[1] - prev[1]) < -PLANESIDE_EPSILON )
      return 0;

    prevPrevIdx = prevIdx;
    prevIdx = ptIdx;
  }

  return 1;
}

/*
================
ShadowMid_EmitTriCallback

Tessellation callback that emits a shadow triangle from a surface.
Extracts 3 vertices from the winding, validates the plane, and emits indices.
================
*/
size_t ShadowMid_EmitTriCallback(TriSurf_t *ts, int vertIdx0, int vertIdx1, int vertIdx2, int cellIndex, int cullGroupIndex)
{
  Winding_t *w;
  float triVerts[9];
  float plane[4];

  Assert(ts, s_assertDisable_ShadowMid_IsConvex);
  Assert(ts->winding, s_assertDisable_ShadowMid_IsConvex);
  Assert(ts->auxData, s_assertDisable_ShadowMid_IsConvex);

  w = ts->winding;
  VectorCopy(w->points[vertIdx0], &triVerts[0]);
  VectorCopy(w->points[vertIdx1], &triVerts[3]);
  VectorCopy(w->points[vertIdx2], &triVerts[6]);

  if ( !PlaneFromPoints(plane, &triVerts[0], &triVerts[3], &triVerts[6]) )
    return 0;

  return EmitShadowTriIndices(triVerts);
}

/* WebAssembly traps on indirect calls whose function-pointer signatures
   do not match exactly. Keep the size_t-returning implementation for
   callers that use the result and pass this void adapter to TesselateWinding. */
static void ShadowMid_EmitTriCallbackAdapter(TriSurf_t *ts, int vertIdx0, int vertIdx1, int vertIdx2, int cellIndex, int cullGroupIndex)
{
  (void)ShadowMid_EmitTriCallback(ts, vertIdx0, vertIdx1, vertIdx2, cellIndex, cullGroupIndex);
}

/*
================
ShadowMid_TriangulateConcaveCasters

Triangulates concave shadow casters via tessellation callbacks.
================
*/
void ShadowMid_TriangulateConcaveCasters(void)
{
  TriSurf_t *ts, *nextTs;

  printf("triangulating concave shadow casters...\n");
  SetTrisTransientMode(1, 1);

  for ( ts = g_smCasterWindingList; ts; ts = nextTs )
  {
    nextTs = ts->next;
    ts->origWinding = CopyWinding(ts->winding);
    TesselateWinding(ts, TESS_CELL_SHADOW, 0, ShadowMid_EmitTriCallbackAdapter);
    FreeTriSurf(ts);
  }

  g_smCasterWindingList = NULL;
  SetTrisTransientMode(0, 1);
}

/*
================
ShadowMid_ComputeNodeBounds

Recursively computes AABB bounds for a shadow tree node from its
children or leaf tri records.
================
*/
int ShadowMid_ComputeNodeBounds(SmOccluderNode_t *node)
{
  SmOccluderNode_t *child;
  SmCasterTri_t *tri;
  int i;

  ClearBounds(node->mins, node->maxs);

  if ( node->childCount > 0 )
  {
    /* internal node: recurse into children and merge bounds */
    child = node->childPtr;
    for ( i = 0; i < node->childCount; i++ )
    {
      ShadowMid_ComputeNodeBounds(&child[i]);
      AddBoundsToBounds(child[i].mins, child[i].maxs, node->mins, node->maxs);
    }
  }
  else if ( node->triCount > 0 )
  {
    /* leaf node: compute bounds from tri projected extents */
    tri = node->triPtr;
    for ( i = 0; i < node->triCount; i++ )
      AddBoundsToBounds(tri[i].projMins, tri[i].projMaxs, node->mins, node->maxs);
  }

  return 0;
}

/*
================
ShadowMid_SortShadowTris

Sorts shadow triangles and builds an AABB tree from them.
Extracts bounds from tri records, builds tree, then converts
to the shadow node format.
================
*/
void ShadowMid_SortShadowTris(void)
{
  AabbTreeBuilder_t builder;
  int i, maxNodes, treeNodeCount;
  SmOccluderNode_t *occNodes;
  ShadowBuildNode_t *buildNodes;

  printf("sorting shadow tris...\n");

  /* set up AABB tree builder for caster tris */
  builder.itemData = g_smCasterTriPool;
  builder.itemCount = g_smNumCasterTris;
  builder.itemStride = sizeof(SmCasterTri_t);
  builder.hasBoundsData = 0;
  builder.itemMins = malloc(g_smNumCasterTris * sizeof(vec3_t));
  builder.itemMaxs = malloc(g_smNumCasterTris * sizeof(vec3_t));
  if ( !builder.itemMins || !builder.itemMaxs )
    Com_Error("Out of memory in SortShadowCasters getting mins/maxs\n");

  /* extract projected bounds from each tri */
  for ( i = 0; i < g_smNumCasterTris; i++ )
  {
    VectorCopy(g_smCasterTriPool[i].projMins, &builder.itemMins[i * 3]);
    VectorCopy(g_smCasterTriPool[i].projMaxs, &builder.itemMaxs[i * 3]);
  }

  /* compute max nodes: max(1, numTris/8) * 2 - 1 */
  maxNodes = g_smNumCasterTris / 8;
  if ( maxNodes < 1 )
    maxNodes = 1;
  builder.maxNodes = 2 * maxNodes - 1;
  builder.nodes = malloc(builder.maxNodes * sizeof(AabbTreeNode_t));
  builder.minPartitionSize = SM_CASTER_TREE_MIN_PARTITION;
  builder.minLeafItems = AABB_MIN_LEAF_ITEMS;

  treeNodeCount = AabbBuildTree(&builder);
  free(builder.itemMins);
  free(builder.itemMaxs);

  /* allocate and convert to SmOccluderNode_t format */
  g_smOccluderTree = malloc(treeNodeCount * sizeof(SmOccluderNode_t));
  if ( !g_smOccluderTree )
    Com_Error("Out of memory in SortShadowCasters getting aabb nodes\n");

  occNodes = g_smOccluderTree;
  buildNodes = (ShadowBuildNode_t *)builder.nodes;
  for ( i = 0; i < treeNodeCount; i++ )
  {
    occNodes[i].triCount = buildNodes[i].triCount;
    occNodes[i].triPtr = &g_smCasterTriPool[buildNodes[i].triStartIndex];
    occNodes[i].childCount = buildNodes[i].childCount;
    occNodes[i].childPtr = &occNodes[buildNodes[i].firstChildIndex];
  }

  ShadowMid_ComputeNodeBounds(occNodes);
  free(builder.nodes);
}

/*
================
ShadowMid_SnapAndMergeVerts

Snaps and merges shadow caster vertices using a merge map.
Copies all verts into a flat buffer, builds a merge map, then
writes merged positions back into each caster's winding.
================
*/
void ShadowMid_SnapAndMergeVerts(void)
{
  TriSurf_t *ts, *nextTs;
  int totalVerts, baseIdx, newPtCount, i;
  vec3_t *vertBuf;
  int *mergeMapResult;
  int mergeMap[SM_MAX_OCCLUDERS];

  printf("snapping and merging shadow caster vertices...\n");

  /* count total vertices across all casters */
  totalVerts = 0;
  for ( ts = g_smCasterWindingList; ts; ts = ts->next )
    totalVerts += ts->winding->numpoints;

  /* copy all winding verts into a flat buffer */
  vertBuf = malloc(totalVerts * sizeof(vec3_t));
  if ( !vertBuf )
    Com_Error("Out of memory snapping %i shadow verts\n", totalVerts);

  baseIdx = 0;
  for ( ts = g_smCasterWindingList; ts; ts = ts->next )
  {
    memcpy(&vertBuf[baseIdx], ts->winding->points, ts->winding->numpoints * sizeof(vec3_t));
    baseIdx += ts->winding->numpoints;
  }

  /* build and apply merge map */
  mergeMapResult = BuildMergeMap((char *)vertBuf, 3, sizeof(vec3_t), totalVerts, ON_EPSILON);
  ApplyMergeMap((char *)vertBuf, sizeof(vec3_t), totalVerts, 0, mergeMapResult);

  /* snap each caster's winding using the merge map */
  baseIdx = 0;
  for ( ts = g_smCasterWindingList; ts; ts = nextTs )
  {
    nextTs = ts->next;
    newPtCount = SnapAndMergeWinding(baseIdx, ts->winding->numpoints, mergeMapResult, mergeMap, (char *)ts->auxData, sizeof(SmAuxVert_t));
    baseIdx += ts->winding->numpoints;

    if ( newPtCount < 3 )
    {
      /* degenerate after merge — remove */
      UnlinkAndFreeSurf(ts, &g_smCasterWindingList);
      FreeTriSurf(ts);
    }
    else
    {
      /* write merged positions back into winding */
      for ( i = 0; i < newPtCount; i++ )
        VectorCopy(vertBuf[mergeMap[i]], ts->winding->points[i]);

      ts->winding->numpoints = newPtCount;
    }
  }

  FreeSurface(mergeMapResult);
  free(vertBuf);
}

/*
================
ShadowMid_AlwaysAccept

Trivial accept callback that always returns 1.
================
*/
char ShadowMid_AlwaysAccept(void)
{
  return 1;
}

/*
================
ShadowMid_TryMergePair

Shared implementation used by concave-caster merging and notch plugging.
================
*/
static int ShadowMid_TryMergePair(int primaryIdx, TriSurf_t **surfArray, IntWinding_t **windingArray, int count, int auxStride)
{
  IntWinding_t *primaryWinding, *secondaryWinding, *splicedWinding;
  TriSurf_t *primarySurf, *secondarySurf;
  int innerIdx, sharedEdgeCount, adjustedIdxA;
  int sharedIdxA, sharedIdxB;

  primaryWinding = windingArray[primaryIdx];

  for ( innerIdx = primaryIdx + 1; innerIdx < count; )
  {
    primarySurf = surfArray[primaryIdx];
    secondarySurf = surfArray[innerIdx];
    secondaryWinding = windingArray[innerIdx];

    /* check bounds overlap and shared edges */
    if ( !BoundsIntersect(primarySurf->mins, primarySurf->maxs, secondarySurf->mins, secondarySurf->maxs)
      || !(sharedEdgeCount = FindSharedEdge(primaryWinding, secondaryWinding, &sharedIdxA, &sharedIdxB)) )
    {
      innerIdx++;
      continue;
    }

    /* if primary is fully shared, swap primary and secondary */
    if ( sharedEdgeCount == primaryWinding->numpoints )
    {
      surfArray[primaryIdx] = secondarySurf;
      surfArray[innerIdx] = primarySurf;
      windingArray[primaryIdx] = secondaryWinding;
      windingArray[innerIdx] = primaryWinding;
      primaryWinding = windingArray[primaryIdx];

      int swappedB = (sharedIdxA + sharedEdgeCount - 1) % windingArray[innerIdx]->numpoints;
      adjustedIdxA = (primaryWinding->numpoints - sharedEdgeCount + sharedIdxB + 1) % primaryWinding->numpoints;
      sharedIdxA = adjustedIdxA;
      sharedIdxB = swappedB;
      secondaryWinding = windingArray[innerIdx];
    }
    else
    {
      adjustedIdxA = sharedIdxA;
    }

    if ( sharedEdgeCount == secondaryWinding->numpoints )
    {
      /* secondary fully contained — remove shared boundary */
      RemoveSharedBoundaryPoints(primaryWinding, auxStride, adjustedIdxA, sharedEdgeCount);
      FreeTriSurf(surfArray[innerIdx]);
      FreeIntWinding(secondaryWinding);
      count--;
      memmove(&surfArray[innerIdx], &surfArray[innerIdx + 1], (count - innerIdx) * sizeof(TriSurf_t *));
      memmove(&windingArray[innerIdx], &windingArray[innerIdx + 1], (count - innerIdx) * sizeof(windingArray[0]));
    }
    else
    {
      /* partial overlap — splice windings */
      splicedWinding = SpliceWindingsAtSharedEdge(primaryWinding, secondaryWinding, auxStride, primaryIdx, innerIdx, adjustedIdxA, sharedIdxB, sharedEdgeCount);
      AddBoundsToBounds(secondarySurf->mins, secondarySurf->maxs, surfArray[primaryIdx]->mins, surfArray[primaryIdx]->maxs);
      FreeTriSurf(surfArray[innerIdx]);
      memmove(&surfArray[innerIdx], &surfArray[innerIdx + 1], (count - innerIdx - 1) * sizeof(TriSurf_t *));
      FreeIntWinding(primaryWinding);
      FreeIntWinding(secondaryWinding);
      primaryWinding = splicedWinding;
      windingArray[primaryIdx] = splicedWinding;
      count--;
      memmove(&windingArray[innerIdx], &windingArray[innerIdx + 1], (count - innerIdx) * sizeof(windingArray[0]));
    }

    windingArray[count] = NULL;
    innerIdx = primaryIdx + 1;
  }

  return count;
}

/*
================
ShadowMid_TryMergeConcavePair

Tries to merge a primary shadow caster with subsequent casters that
share edges. Handles swap, splice, and boundary removal cases.
================
*/
int ShadowMid_TryMergeConcavePair(int primaryIdx, TriSurf_t **surfArray, IntWinding_t **windingArray, int count, int auxStride)
{
  return ShadowMid_TryMergePair(primaryIdx, surfArray, windingArray, count, auxStride);
}

/*
================
ShadowMid_TryMergeNotchPair

Tries to merge two shadow casters for notch plugging. Handles
swap, splice, and boundary removal cases for notch pairs.
================
*/
int ShadowMid_TryMergeNotchPair(int primaryIdx, TriSurf_t **surfArray, IntWinding_t **windingArray, int count, int auxStride)
{
  return ShadowMid_TryMergePair(primaryIdx, surfArray, windingArray, count, auxStride);
}

/*
================
SM_BuildProjectionMatrix

Builds shadow projection matrix from light position and direction.
Constructs view matrix, applies perspective if needed, computes inverse and transpose.
================
*/
void SM_BuildProjectionMatrix(void)
{
  float perspMatrix[16];
  float viewMatrix[16];
  float rightVec[3];
  float upVec[3];
  float negDir[3];

  negDir[0] = -g_smLightDirX;
  negDir[1] = -g_smLightDirY;
  negDir[2] = -g_smLightDirZ;
  PerpendicularVector(negDir, rightVec);
  CrossProduct(negDir, rightVec, upVec);
  viewMatrix[1] = rightVec[1];
  viewMatrix[5] = upVec[1];
  viewMatrix[0] = rightVec[0];
  viewMatrix[2] = rightVec[2];
  viewMatrix[9] = negDir[1];
  viewMatrix[4] = upVec[0];
  { float smOrigin[3] = { g_smLightOriginX, g_smLightOriginY, g_smLightOriginZ };
  viewMatrix[3] = -DotProduct210(smOrigin, rightVec); }
  viewMatrix[6] = upVec[2];
  viewMatrix[8] = negDir[0];
  viewMatrix[10] = negDir[2];
  memset(&viewMatrix[12], 0, 12);
  viewMatrix[15] = 1.0;
  { float smO[3] = { g_smLightOriginX, g_smLightOriginY, g_smLightOriginZ };
  viewMatrix[7] = -DotProduct210(upVec, smO);
  viewMatrix[11] = -DotProduct210(negDir, smO); }
  if ( g_smPerspectiveEnabled )
    #define SHADOW_FOV 90.0
    SetPerspectiveProjection(perspMatrix, SHADOW_FOV, SHADOW_FOV, 1.0);
  else
    MatrixIdentity44(perspMatrix);
  MatrixMultiply44(viewMatrix, perspMatrix, &g_smViewProjMatrix);
  MatrixInverse44(&g_smViewProjMatrix, g_smInvViewProjMatrix);
  MatrixTranspose44(g_smInvViewProjMatrix, g_smTransposedInvMatrix);
}
/*
================
SM_ProjectPoint

Projects a 3D point through the shadow projection matrix into 4D homogeneous coordinates.
================
*/
void SM_ProjectPoint(float *projected, float *pt)
{
  Assert(pt, s_assertDisable_SM_ProjectPoint);
  Assert(projected, s_assertDisable_SM_ProjectPoint);
  Assert(pt != projected, s_assertDisable_SM_ProjectPoint);
#ifdef _WIN64
  projected[0] = (float)((double)g_smViewProjMatrix_01*pt[1] + (double)g_smViewProjMatrix_02*pt[2] + (double)g_smViewProjMatrix*pt[0] + (double)g_smViewProjMatrix_03);
  projected[1] = (float)((double)g_smViewProjMatrix_11*pt[1] + (double)g_smViewProjMatrix_12*pt[2] + (double)g_smViewProjMatrix_10*pt[0] + (double)g_smViewProjMatrix_13);
  projected[2] = (float)((double)g_smViewProjMatrix_20*pt[0] + (double)g_smViewProjMatrix_21*pt[1] + (double)g_smViewProjMatrix_22*pt[2] + (double)g_smViewProjMatrix_23);
  projected[3] = (float)((double)g_smViewProjMatrix_31*pt[1] + (double)g_smViewProjMatrix_32*pt[2] + (double)g_smViewProjMatrix_30*pt[0] + (double)g_smViewProjMatrix_33);
#else
  projected[0] = g_smViewProjMatrix_01 * pt[1] + g_smViewProjMatrix_02 * pt[2] + g_smViewProjMatrix * pt[0] + g_smViewProjMatrix_03;
  projected[1] = g_smViewProjMatrix_11 * pt[1] + g_smViewProjMatrix_12 * pt[2] + g_smViewProjMatrix_10 * pt[0] + g_smViewProjMatrix_13;
  projected[2] = g_smViewProjMatrix_20 * pt[0] + g_smViewProjMatrix_21 * pt[1] + g_smViewProjMatrix_22 * pt[2] + g_smViewProjMatrix_23;
  projected[3] = g_smViewProjMatrix_31 * pt[1] + g_smViewProjMatrix_32 * pt[2] + g_smViewProjMatrix_30 * pt[0] + g_smViewProjMatrix_33;
#endif
}

/*
================
SM_ProjectTriRecord

Projects a triangle record's 3 vertices through the shadow projection
matrix and computes the projected bounding box.
================
*/
int SM_ProjectTriRecord(SmCasterTri_t *rec)
{
  float projected[4];
  vec3_t normalized;
  double invW;
  int i;

  ClearBounds(rec->projMins, rec->projMaxs);

  /* project each of the 3 corner vertices and compute normalized bounds */
  for ( i = 0; i < 3; i++ )
  {
    SM_ProjectPoint(projected, &rec->corners[i * 3]);
    invW = 1.0 / projected[3];
    VectorScale(projected, invW, normalized);
    AddPointToBounds(normalized, rec->projMins, rec->projMaxs);
  }

  return 0;
}

/*
================
SM_AddCasterTriangle

Adds a shadow caster triangle record. Computes plane from triangle,
dot-products with shadow direction, stores 80-byte record.
DEAD CODE: Zero cross-references in original exe.
================
*/
void SM_AddCasterTriangle(int usage, float *corners)
{
  float plane[4];
  char flag;
  int count;
  SmCasterTri_t *rec;
  double dot;

  AssertFatal(corners, s_assertDisable_SM_AddCasterTriangle);
  AssertFatal(usage, s_assertDisable_SM_AddCasterTriangle);

  /* compute plane from triangle vertices */
  if ( !PlaneFromPoints(plane, corners, &corners[3], &corners[6]) )
    return;

  /* classify triangle facing relative to shadow light */
  { float smDir[3] = { g_smLightDir_x, g_smLightDir_y, g_smLightDir_z };
  dot = DotProduct(smDir, plane) - g_smLightDist * plane[3]; }

  if ( dot < -PLANESIDE_EPSILON )
  {
    flag = 1;  /* backfacing */
    if ( usage == 2 )
      return;  /* usage 2 skips backfacing */
  }
  else if ( dot > PLANESIDE_EPSILON )
  {
    flag = 0;  /* frontfacing */
  }
  else
  {
    return;    /* edge-on — skip */
  }

  count = g_smNumCasterTris;
  if ( count == MAX_MAP_TRIANGLES )
  {
    Com_Error("SHADOW_CASTER_TRIS_LIMIT (%i) exceeded\n", count);
    count = g_smNumCasterTris;
  }

  /* store caster record */
  rec = &g_smCasterTriPool[count];
  VectorCopy(plane, rec->plane);
  rec->plane[3] = plane[3];
  rec->flag = flag;
  memcpy(rec->corners, corners, 9 * sizeof(float));

  SM_ProjectTriRecord(rec);

  if ( rec->projMaxs[2] > 0.0f )
    g_smNumCasterTris++;
}

/*
================
SM_CreateFragment

Creates a shadow caster fragment from a winding and projected points.
Computes local and global bounds for the fragment.
================
*/
TriSurf_t *SM_CreateFragment(Winding_t *winding, TriSurfProps_t *material, SmAuxVert_t *auxData, double lightDist)
{
  int idx;
  float localMins[3];
  float localMaxs[3];
  TriSurf_t *casterSurf;

  Assert(winding, s_assertDisable_SM_CreateFragment);
  Assert(winding->numpoints >= 3, s_assertDisable_SM_CreateFragment);
  if ( !material )
    material = EmitShadowcasterTriSurface(lightDist, winding);
  casterSurf = PrependTriSurf(winding, auxData, sizeof(SmAuxVert_t), material, &g_smCasterWindingList);
  ClearBounds(localMins, localMaxs);
  for ( idx = 0; idx < winding->numpoints; idx++ )
  {
    SmAuxVert_t *av = &auxData[idx];
    AssertFatal(winding->points[idx][0] == av->orig[0] && winding->points[idx][1] == av->orig[1] && winding->points[idx][2] == av->orig[2], s_assertDisable_SM_CreateFragment);
    AddPointToBounds(av->orig, g_smGridMins, g_smGridMaxs);
    AddPointToBounds(av->proj, localMins, localMaxs);
  }
  AddBoundsToBounds(localMins, localMaxs, g_smCasterBoundsMin, g_smCasterBoundsMax);
  return casterSurf;
}

/*
================
SM_ComputeSidePlane

Computes a shadow volume side plane from an edge and light direction.
Handles both point and directional lights, with optional plane flip.
================
*/
char SM_ComputeSidePlane(float *edgeVert, float *outPlane, float *baseVert, char flipFlag)
{
  vec3_t edgeDir, lightDir;

  VectorSubtract(edgeVert, baseVert, edgeDir);
  if ( g_smPerspectiveEnabled )
  {
    lightDir[0] = g_smLightOriginX - baseVert[0];
    lightDir[1] = g_smLightOriginY - baseVert[1];
    lightDir[2] = g_smLightOriginZ - baseVert[2];
  }
  else
  {
    lightDir[0] = g_smLightDirX;
    lightDir[1] = g_smLightDirY;
    lightDir[2] = g_smLightDirZ;
  }
  CrossProduct(lightDir, edgeDir, outPlane);
  if ( 0.0 == VecNormalize(outPlane) )
    return 0;

  if ( flipFlag == 1 )
    VectorScale(outPlane, -1, outPlane);

  outPlane[3] = (float)((double)outPlane[2] * (double)baseVert[2] + (double)outPlane[1] * (double)baseVert[1] + (double)baseVert[0] * (double)outPlane[0]);
  return 1;
}

/*
================
SM_RayPlaneIntersect

Computes ray-plane intersection parameter t.
================
*/
double SM_RayPlaneIntersect(float *rayDir, float *rayOrigin, float *plane)
{
  float denom;

  denom = DotProduct210(rayDir, plane);
  Assert(denom != 0.0f, s_assertDisable_SM_RayPlaneIntersect);
  return (plane[3] - DotProduct210(rayOrigin, plane)) / denom;
}

/*
================
SM_AllocProjectedPoints

Allocates projected points from winding vertices. Each point is 28 bytes:
origXYZ (12), projXYZ (12), flags (2), pad (2). Projects each vertex through the shadow matrix.
================
*/
SmAuxVert_t *SM_AllocProjectedPoints(Winding_t *winding)
{
  SmAuxVert_t *buf;
  int i;

  buf = malloc(sizeof(SmAuxVert_t) * winding->numpoints);

  for ( i = 0; i < winding->numpoints; i++ )
  {
    /* copy original XYZ */
    VectorCopy(winding->points[i], buf[i].orig);

    /* project and store projXYZW */
    SM_ProjectPoint(buf[i].proj, buf[i].orig);

    buf[i].flagA = 1;
    buf[i].flagB = 1;
  }

  return buf;
}

/*
================
SM_ProjectWindingThroughOccluders

Projects a winding through occluders with vertex nudge. Finds the best
occluder by ray-plane distance, then nudges winding vertices halfway
toward it. Returns a new fragment with projected points.
================
*/
int *SM_ProjectWindingThroughOccluders(Winding_t *winding)
{
  double t = 0.0;
  int i, j;
  vec3_t rayDir;
  float sumDists[SM_MAX_OCCLUDERS];
  float maxDists[SM_MAX_OCCLUDERS];
  char validFlags[SM_MAX_OCCLUDERS];
  SmOccluder_t *bestOcc;
  float bestMaxDist, bestSumDist;
  SmAuxVert_t *projectedPts;

  Assert(winding, s_assertDisable_SM_ProjectWindingThroughOccluders);
  Assert(g_smOccluderArray, s_assertDisable_SM_ProjectWindingThroughOccluders);

  /* initialize per-occluder tracking arrays */
  memset(sumDists, 0, g_smNumOccluders * sizeof(float));
  memset(maxDists, 0, g_smNumOccluders * sizeof(float));
  for ( i = 0; i < g_smNumOccluders; i++ )
    validFlags[i] = !g_smOccluderArray[i].casterTri->flag && g_smOccluderArray[i].active;

  /* for each vertex, cast ray toward light and accumulate occluder distances */
  for ( i = 0; i < winding->numpoints; i++ )
  {
    if ( g_smPerspectiveEnabled )
    {
      rayDir[0] = g_smLightOriginX - winding->points[i][0];
      rayDir[1] = g_smLightOriginY - winding->points[i][1];
      rayDir[2] = g_smLightOriginZ - winding->points[i][2];
    }
    else
    {
      rayDir[0] = g_smLightDirX;
      rayDir[1] = g_smLightDirY;
      rayDir[2] = g_smLightDirZ;
    }
    VecNormalize(rayDir);

    for ( j = 0; j < g_smNumOccluders; j++ )
    {
      if ( !validFlags[j] )
        continue;

      t = SM_RayPlaneIntersect(rayDir, winding->points[i], (float *)g_smOccluderArray[j].casterTri);
      if ( t >= SM_RAY_NEAR_THRESHOLD )
      {
        sumDists[j] += (float)t;
        if ( t > maxDists[j] )
          maxDists[j] = (float)t;
      }
      else
      {
        validFlags[j] = 0;
      }
    }
  }

  /* find best occluder: largest max distance, tiebreak by sum */
  bestOcc = NULL;
  bestMaxDist = 0.0f;
  bestSumDist = 0.0f;
  for ( i = 0; i < g_smNumOccluders; i++ )
  {
    if ( !validFlags[i] )
      continue;

    if ( maxDists[i] + PLANESIDE_EPSILON >= bestMaxDist )
    {
      if ( maxDists[i] - PLANESIDE_EPSILON >= bestMaxDist || bestSumDist <= (double)sumDists[i] )
      {
        bestMaxDist = maxDists[i];
        bestSumDist = sumDists[i];
        bestOcc = &g_smOccluderArray[i];
      }
    }
  }

  /* nudge winding vertices halfway toward the best occluder */
  if ( bestOcc )
  {
    for ( i = 0; i < winding->numpoints; i++ )
    {
      if ( g_smPerspectiveEnabled )
      {
        rayDir[0] = g_smLightOriginX - winding->points[i][0];
        rayDir[1] = g_smLightOriginY - winding->points[i][1];
        rayDir[2] = g_smLightOriginZ - winding->points[i][2];
      }
      else
      {
        rayDir[0] = g_smLightDirX;
        rayDir[1] = g_smLightDirY;
        rayDir[2] = g_smLightDirZ;
      }
      VecNormalize(rayDir);
      t = SM_RayPlaneIntersect(rayDir, winding->points[i], (float *)bestOcc->casterTri) * 0.5;
      VectorMA(winding->points[i], (float)t, rayDir, winding->points[i]);
    }
  }

  projectedPts = SM_AllocProjectedPoints(winding);
  return (int *)SM_CreateFragment(winding, 0, projectedPts, t);
}

/*
================
SM_ProjectWindingThroughOccluders_r

Recursive wrapper that clips a winding by occluders then projects through.
================
*/
int SM_ProjectWindingThroughOccluders_r(Winding_t *winding)
{
  return ShadowMid_ClipWindingByOccluders_r(winding, 0, ShadowMid_ProjectWindingThroughOccludersCallback, 0, 0);
}

/*
================
SM_InitOccluder

Initializes an occluder record with side planes computed from the
caster winding. Tests each side against receiver points to validate.
================
*/
char SM_InitOccluder(vec3_t *receiverPts, int receiverPtCount, SmCasterTri_t *caster, Winding_t *winding, SmOccluder_t *occ)
{
  SmCasterTri_t *tri = caster;
  int i, nextIdx;
  float minDist, maxDist;

  Assert(caster, s_assertDisable_SM_InitOccluder);
  Assert(receiverPts, s_assertDisable_SM_InitOccluder);
  Assert(receiverPtCount >= 3, s_assertDisable_SM_InitOccluder);
  Assert((float *)receiverPts != caster->corners, s_assertDisable_SM_InitOccluder);
  Assert(winding, s_assertDisable_SM_InitOccluder);
  Assert(occ, s_assertDisable_SM_InitOccluder);
  #define MAX_OCCLUDER_WINDING_POINTS 4
  Assert(winding->numpoints <= MAX_OCCLUDER_WINDING_POINTS, s_assertDisable_SM_InitOccluder);

  occ->casterTri = tri;
  occ->polygon = winding;

  /* compute side planes and validate against receiver */
  for ( i = 0; i < winding->numpoints; i++ )
  {
    nextIdx = (i + 1) % winding->numpoints;
    SM_ComputeSidePlane(winding->points[nextIdx], occ->planes[i], winding->points[i], tri->flag);
    WindingPlaneDistExtent(winding, occ->planes[i], occ->planes[i][3], &minDist, &maxDist);

    Assert(maxDist <= ON_EPSILON || minDist < -maxDist, s_assertDisable_SM_InitOccluder);

    /* if all receiver points are on the front side of this plane, reject */
    if ( CheckPointsAgainstPlane(receiverPts, receiverPtCount, occ->planes[i], occ->planes[i][3], ON_EPSILON) )
    {
      FreeWinding(winding);
      return 0;
    }
  }

  occ->area = WindingSignedArea(winding, (float *)tri);
  return 1;
}

/*
================
SM_FindOccluders_r

Recursively finds occluders from the AABB tree for a given caster.
Tests bounds intersection, recurses into children, and initializes
occluder records for matching leaf triangles.

NOTE: caster parameter uses SmCasterBounds_t layout:
  plane[4], vertCount, *receiverVerts, bounds[6] (mins[3]+maxs[3])
================
*/
int SM_FindOccluders_r(SmOccluderNode_t *node, SmCasterBounds_t *caster, int foundCount, int maxOccluders, SmOccluder_t *occluderBuf)
{
  SmCasterBounds_t *casterPtr;
  SmOccluderNode_t *occ;
  int result, childCount, childIdx, leafIdx;
  SmCasterTri_t *tri;
  Winding_t *w;

  casterPtr = caster;
  occ = node;

  /* early out: no overlap between node AABB and caster projected bounds */
  if ( occ->mins[0] >= (double)caster->maxs[0]
    || occ->maxs[0] <= (double)caster->mins[0]
    || occ->mins[1] >= (double)caster->maxs[1]
    || occ->maxs[1] <= (double)caster->mins[1]
    || occ->mins[2] >= (double)caster->maxs[2] )
  {
    return foundCount;
  }
  childCount = occ->childCount;
  if ( childCount )
  {
    /* internal node: recurse into children */
    SmOccluderNode_t *childBase = occ->childPtr;
    result = foundCount;
    for ( childIdx = 0; childIdx < childCount; childIdx++ )
      result = SM_FindOccluders_r(&childBase[childIdx], casterPtr, result, maxOccluders, occluderBuf);
  }
  else
  {
    /* leaf node: test each tri against caster bounds */
    SmCasterTri_t *triBase = occ->triPtr;
    SmOccluder_t *occSlot = &occluderBuf[foundCount];
    for ( leafIdx = 0; leafIdx < occ->triCount; leafIdx++ )
    {
      tri = &triBase[leafIdx];

      /* AABB overlap test: tri projected bounds vs caster bounds */
      if ( (double)tri->projMins[0] < (double)casterPtr->maxs[0]
          && (double)tri->projMaxs[0] > (double)casterPtr->mins[0]
          && (double)tri->projMins[1] < (double)casterPtr->maxs[1]
          && (double)tri->projMaxs[1] > (double)casterPtr->mins[1]
          && (double)tri->projMins[2] < (double)casterPtr->maxs[2] )
        {
          /* build winding from tri corners and clip against caster plane */
          w = AllocWinding(3);
          w->numpoints = 3;
          memcpy(w->points, tri->corners, 9 * sizeof(float));
          ClipWindingByPlane(&w, casterPtr->plane, casterPtr->plane[3], ON_EPSILON);

          if ( w )
          {
            RemoveDuplicatePoints(w, ON_EPSILON);
            if ( w->numpoints >= 3 )
            {
              if ( foundCount == maxOccluders )
                Com_Error("More than %i potential occluders to a shadow casting poly", maxOccluders);

              /* initialize occluder from clipped winding */
              if ( SM_InitOccluder((vec3_t *)casterPtr->receiverVerts, casterPtr->vertCount, tri, w, occSlot) )
              {
                foundCount++;
                occSlot++;
              }
            }
            else
            {
              FreeWinding(w);
            }
          }
          occ = (SmOccluderNode_t *)node;
        }
    }
    return foundCount;
  }
  return result;
}

/*
================
SM_FindOccluders

Finds and sorts occluders for a shadow caster from the AABB tree.
================
*/
size_t SM_FindOccluders(SmCasterBounds_t *caster, int maxOccluders, void *occluderBuf)
{
  size_t foundCount;

  foundCount = SM_FindOccluders_r(g_smOccluderTree, caster, 0, maxOccluders, occluderBuf);
  qsort(occluderBuf, foundCount, sizeof(SmOccluder_t), (int (*)(const void*, const void*))ShadowMid_CompareOccluderEntries);
  return foundCount;
}

/*
================
SM_FindOccludersFlipped

Finds occluders with a flipped caster plane. Negates the plane normal
and distance, then searches the AABB tree and sorts results.
================
*/
size_t SM_FindOccludersFlipped(SmCasterTri_t *caster, void *occluderBuf, int maxOccluders)
{
  size_t foundCount;

  /* build flipped caster record: negate plane, copy bounds and corners pointer */
  SmCasterBounds_t flipped;

  flipped.plane[0] = -caster->plane[0];
  flipped.plane[1] = -caster->plane[1];
  flipped.plane[2] = -caster->plane[2];
  flipped.plane[3] = -caster->plane[3];
  flipped.vertCount = 3;
  flipped.receiverVerts = caster->corners;
  flipped.mins[0] = caster->projMins[0];
  flipped.mins[1] = caster->projMins[1];
  flipped.mins[2] = caster->projMins[2];
  flipped.maxs[0] = caster->projMaxs[0];
  flipped.maxs[1] = caster->projMaxs[1];
  flipped.maxs[2] = caster->projMaxs[2];

  foundCount = SM_FindOccluders_r(g_smOccluderTree, &flipped, 0, maxOccluders, occluderBuf);
  qsort(occluderBuf, foundCount, sizeof(SmOccluder_t), (int (*)(const void*, const void*))ShadowMid_CompareOccluderEntries);
  return foundCount;
}

/*
================
ShadowMid_FindMinimalCastingTris

Iterates all shadow tri records, clips each marked caster through
occluders to find the minimal set of shadow casting tris.
Expands global shadow bounds by 1.0 in each direction afterwards.
================
*/
void ShadowMid_FindMinimalCastingTris(void)
{
  SmOccluder_t occluderBuffer[SM_MAX_OCCLUDERS];  /* stack buffer for occluder entries */
  SmCasterTri_t *tri;
  Winding_t *w;
  int triIdx, i;

  printf("finding minimal shadow casting tris...\n");
  ClearBounds(g_smGridMins, g_smGridMaxs);
  ClearBounds(g_smCasterBoundsMin, g_smCasterBoundsMax);
  g_smOccluderArray = occluderBuffer;

  for ( triIdx = 0; triIdx < g_smNumCasterTris; triIdx++ )
  {
    tri = &g_smCasterTriPool[triIdx];
    if ( tri->flag != 1 )
      continue;

    /* find occluders for this backfacing tri */
    g_smNumOccluders = (int)SM_FindOccludersFlipped(tri, occluderBuffer, SM_MAX_OCCLUDERS);
    g_smCurrentCasterTri = tri;

    /* build winding from tri corners and clip through occluders */
    w = AllocWinding(3);
    w->numpoints = 3;
    memcpy(w->points, tri->corners, 9 * sizeof(float));
    ShadowMid_ClipWindingByOccluders_r(
      w, 1,
      SM_ProjectWindingThroughOccluders_r,
      ShadowMid_FreeWindingCallback, 0);

    /* free occluder windings */
    for ( i = 0; i < g_smNumOccluders; i++ )
      FreeWinding((Winding_t *)g_smOccluderArray[i].polygon);
  }

  /* expand grid bounds by 1 unit */
  g_smGridMins[0] -= 1.0f;
  g_smGridMins[1] -= 1.0f;
  g_smGridMins[2] -= 1.0f;
  g_smGridMaxs[0] += 1.0f;
  g_smGridMaxs[1] += 1.0f;
  g_smGridMaxs[2] += 1.0f;
}


/*
================
SM_CanMergeSurfaces

Tests if two shadow caster surfaces are coplanar and can be merged.
Checks normal dot product and vertex-to-plane distances.
================
*/
char SM_CanMergeSurfaces(TriSurf_t *tsA, TriSurf_t *tsB)
{
  TriSurfProps_t *propsA = tsA->props;
  TriSurfProps_t *propsB = tsB->props;
  Winding_t *w;
  int i;
  double distA, distB;

  if ( propsA == propsB )
    return 1;

  /* check if normals are close enough to merge */
  if ( DotProduct210(propsA->plane, propsB->plane) < SM_MERGE_DOT_THRESHOLD )
    return 0;

  /* check if all of tsB's vertices lie on both planes */
  w = tsB->winding;
  for ( i = 0; i < w->numpoints; i++ )
  {
    distB = DotProduct201(w->points[i], propsB->plane) - propsB->plane[3];
    if ( distB * distB > SM_COPLANAR_DIST_SQ )
      return 0;
    distA = DotProduct210(w->points[i], propsA->plane) - propsA->plane[3];
    if ( distA * distA > SM_COPLANAR_DIST_SQ )
      return 0;
  }

  return 1;
}

/*
================
SM_InterpolateVert

Interpolates between two auxiliary vertex records by fraction.
Lerps 6 float fields and copies char flags from both endpoints.
================
*/
char SM_InterpolateVert(float *from, float *to, double frac, float *result)
{
  int i;

  Assert(from != NULL, s_assertDisable_SM_InterpolateVert);
  Assert(to != NULL, s_assertDisable_SM_InterpolateVert);
  Assert(result != NULL, s_assertDisable_SM_InterpolateVert);

  /* interpolate 6 floats: origXYZ + projXYZ */
  for ( i = 0; i < 6; i++ )
    result[i] = FMA1(from[i], to[i] - from[i], frac);

  /* copy flag bytes: result gets to's flagB and from's flagA */
  ((SmAuxVert_t *)result)->flagA = ((SmAuxVert_t *)to)->flagB;
  ((SmAuxVert_t *)result)->flagB = ((SmAuxVert_t *)from)->flagA;

  return ((SmAuxVert_t *)to)->flagB;
}

/*
================
SM_FlattenSurfToWinding

Flattens a surface to 2D winding for notch plugging operations.
Copies projected XY from aux data, sets Z=0, saves original plane
and sets up a Z-up plane for 2D operations.
================
*/
float *SM_FlattenSurfToWinding(TriSurf_t *ts)
{
  Winding_t *w;
  float *vert;
  int i;

  CheckWindingInPlane(ts->winding, ts->props->plane, ts->props->plane[3]);
  w = ts->winding;
  Assert(w->numpoints > 0, s_assertDisable_SM_FlattenSurfToWinding);

  /* replace winding XYZ with projected XY from aux data, Z=0 */
  vert = (float *)ts->auxData;
  for ( i = 0; i < w->numpoints; i++ )
  {
    w->points[i][0] = vert[i * SM_AUX_VERT_STRIDE + SM_AUX_PROJ_OFS];  /* projX */
    w->points[i][1] = vert[i * SM_AUX_VERT_STRIDE + SM_AUX_PROJ_OFS + 1];  /* projY */
    w->points[i][2] = 0.0f;
  }

  /* save original plane into texVecs[0..3] for later restoration */
  VectorCopy(ts->props->plane, ts->props->texVecs);
  ts->props->texVecs[3] = ts->props->plane[3];

  /* set Z-up plane for 2D operations */
  VectorClear(ts->props->plane);
  ts->props->plane[2] = 1.0f;
  ts->props->plane[3] = 0.0f;

  return ts->props->plane;
}

/*
================
SM_RestoreSurfFromWinding

Restores a surface from 2D winding back to 3D coordinates.
Copies XYZ from aux data back to winding vertices and restores
the original plane from the saved copy.
================
*/
float *SM_RestoreSurfFromWinding(TriSurf_t *ts)
{
  Winding_t *w;
  float *vert;
  int i;

  w = ts->winding;
  Assert(w->numpoints > 0, s_assertDisable_SM_RestoreSurfFromWinding);

  /* restore original XYZ from aux data back to winding */
  vert = (float *)ts->auxData;
  for ( i = 0; i < w->numpoints; i++ )
    VectorCopy(&vert[i * SM_AUX_VERT_STRIDE], w->points[i]);

  /* restore original plane from saved copy in texVecs */
  VectorCopy(ts->props->texVecs, ts->props->plane);
  ts->props->plane[3] = ts->props->texVecs[3];

  /* clear the saved copy */
  ts->props->texVecs[0] = 0.0f;
  ts->props->texVecs[1] = 0.0f;
  ts->props->texVecs[2] = 0.0f;
  ts->props->texVecs[3] = 0.0f;

  return ts->props->texVecs;
}

/*
================
SM_MergeSurfacesCallback

Merge callback that repeatedly tries to merge concave pairs
until no more merges occur.
================
*/
int SM_MergeSurfacesCallback(TriSurf_t **surfArray, IntWinding_t **windingArray, int count, int auxStride, TriSurf_t *baseSurf, IntWinding_t *baseWinding, int flags)
{
  int currentCount;
  int idx;
  int prevCount;

  currentCount = count;
  do
  {
    idx = 0;
    prevCount = currentCount;
    if ( currentCount <= 0 )
      break;
    do
      currentCount = ShadowMid_TryMergeConcavePair(idx++, surfArray, windingArray, currentCount, auxStride);
    while ( idx < currentCount );
  }
  while ( prevCount != currentCount );
  return currentCount;
}

/*
================
SM_PlugNotchesCallback

Notch callback that merges pairs then plugs remaining notches
in the merged winding.
================
*/
int SM_PlugNotchesCallback(TriSurf_t **surfArray, IntWinding_t **neighbors, int count, int auxStride, TriSurf_t *baseSurf, IntWinding_t *baseWinding, int flags)
{
  int idx;
  int prevCount;

  do
  {
    idx = 0;
    prevCount = count;
    if ( count <= 0 )
      break;
    do
      count = ShadowMid_TryMergeNotchPair(idx++, surfArray, neighbors, count, auxStride);
    while ( idx < count );
  }
  while ( prevCount != count );
  return ShadowMid_PlugNotchesInWinding(surfArray, neighbors, count, (int *)baseSurf, baseWinding);
}

/*
================
MergeShadowCasters

Merges shadow casters into concave groups using visibility grouping.
================
*/
void MergeShadowCasters(Tree_t *visGroups)
{
  printf("merging into concave shadow casters...\n");
  SetTrisTransientMode(2, 0);
  MergeSurfaces_Init(MAX_SHADOW_MERGE_SURFS, ShadowMid_CanMergeSurfacesCallback, NULL);
  MergeVisGroupList(
    &g_smCasterWindingList,
    g_smGridMins,
    g_smGridMaxs,
    visGroups,
    SM_MergeSurfacesCallback);
  MergeSurfaces_Shutdown();
  ChopConcaveCasterWindings();
  SetTrisTransientMode(0, 1);
}

/*
================
PlugShadowCasterNotches

Plugs notches in shadow casting geometry. Flattens all casters to 2D,
runs notch plugging merge, then restores to 3D.
================
*/
void PlugShadowCasterNotches(void)
{
  TriSurf_t *caster;
  float searchMins[3], searchMaxs[3];

  printf("plugging notches in shadow casting geometry...\n");

  /* flatten all casters to 2D for notch operations */
  for ( caster = g_smCasterWindingList; caster; caster = caster->next )
    SM_FlattenSurfToWinding(caster);

  /* set up 2D search bounds and plug notches */
  MergeSurfaces_Init(MAX_SHADOW_MERGE_SURFS, ShadowMid_WindingBehindPlaneCallback, NULL);
  SetTrisTransientMode(2, 0);
  searchMins[0] = g_smCasterBoundsMin[0];
  searchMins[1] = g_smCasterBoundsMin[1];
  searchMins[2] = 0.0f;
  searchMaxs[0] = g_smCasterBoundsMax[0];
  searchMaxs[1] = g_smCasterBoundsMax[1];
  searchMaxs[2] = 0.0f;
  PlugNotchesInList(
    &g_smCasterWindingList,
    searchMins,
    searchMaxs,
    ShadowMid_IsConvex,
    SM_PlugNotchesCallback);
  SetTrisTransientMode(0, 1);
  MergeSurfaces_Shutdown();

  /* restore all casters back to 3D */
  for ( caster = g_smCasterWindingList; caster; caster = caster->next )
    SM_RestoreSurfFromWinding(caster);
}

/*
================
BuildMidpointShadowCasters

Main shadow midpoint processing pipeline. Sorts shadow tris,
finds minimal casting set, merges casters, fixes T-junctions,
plugs notches, snaps/merges verts, triangulates concave casters.
================
*/
void BuildMidpointShadowCasters(Tree_t *visGroups)
{
  if ( g_smNumCasterTris )
  {
    GridTree_Init();
    TJunc_Init(sizeof(SmAuxVert_t));  /* aux vertex stride: 7 floats per projected vertex */
    g_lerpAuxDataCallback = ShadowMid_InterpolateVertLerpCallback;
    ShadowMid_SortShadowTris();
    ShadowMid_FindMinimalCastingTris();
    MergeShadowCasters(visGroups);
    ShadowMid_SimplifyConcaveHoles();
    PlugShadowCasterNotches();
    ShadowMid_FixTJunctions();
    ShadowMid_SnapAndMergeVerts();
    ShadowMid_TriangulateConcaveCasters();
    FreeAllTriSurfs();
    g_lerpAuxDataCallback = NULL;
    TJunc_Init(0);
    GridTree_Shutdown();
  }
  ShadowMid_Shutdown();
}

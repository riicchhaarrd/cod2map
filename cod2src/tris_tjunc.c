/*
tris_tjunc.c — T-junction fixing

Reconstructed from cod2map.exe by Rose.

Detects and fixes T-junctions in triangle winding edges. When a vertex
from one triangle lies on another triangle's edge, the edge must be split
to prevent rendering cracks. Uses spatial hashing for edge line lookup
and a sorted point list per edge line for insertion order.
*/

#include "cod2map.h"

#define MAX_CONCAVE_WINDING_POINTS 0x4000

Dvar_t          *fs_copyfiles;
TjuncEdgeLine_t *tjuncNodeHash[TJUNC_HASH_TABLE_SIZE];
TjuncEdgeLine_t  tjuncEdgeLines[MAX_TJUNC_EDGE_LINES];

int    numDegenerateTrisRemoved;
int    numSelfTjunctions;
int    numUnmergeableTexVerts;
void  *tjuncAuxBuffer;
int    tjuncAuxElemSize;
float  tjuncBoundsMax[3];
float  tjuncBoundsMin[3];
char   tjuncCreateNonAxial;
float  tjuncHashTolSq;
int    tjuncLineCount;
int    tjuncPointCount;
char   tjuncPoints[sizeof(TjuncPoint_t) * MAX_TJUNC_POINTS];
float  tjuncSnapTolSq;
void  *tjuncSpatialHash[TJUNC_SPATIAL_HASH_SLOTS];
int   *triVertexIndexMap;

char s_assertDisable_FixSurfaceJunctions;
char s_assertDisable_RemoveDegenerateEdges;
char s_assertDisable_TJunc_FindEdge;
char s_assertDisable_TJunc_Init;
char s_assertDisable_TJunc_ProcessSurface;
char s_assertDisable_TJunc_ProcessWinding;
char s_assertDisable_TjuncClampNode;
char s_assertDisable_TjuncFixFaces;


/*
================
TjuncReset

Resets T-junction globals: point count, edge lines, hash table
================
*/
int TjuncReset(void)
{
  tjuncLineCount = 0;
  tjuncPointCount = 0;
  if ( tjuncCreateNonAxial )
    memset(&tjuncSpatialHash, 0, sizeof(tjuncSpatialHash));
  memset(&tjuncNodeHash, 0, sizeof(tjuncNodeHash));
  return 0;
}

/*
================
TjuncClampNode

Clamps octree (face, s, t) to valid range, returns node index
================
*/
TjuncEdgeLine_t *TjuncClampNode(unsigned int t, unsigned int face, unsigned int s)
{
  /* Cube-face coordinate wrapping: 3 faces x 8x8 grid.
     When s or t goes out of range (-1 or 8), wrap to the
     adjacent face with mirrored/rotated coordinates.
     Loops because one wrap can push the other axis out of range. */
  #define GRID_MAX 7
  #define GRID_SIZE 8

  while ( 1 )
  {
    while ( 1 )
    {
      while ( 1 )
      {
        /* s underflow: wrap to adjacent face along -s edge */
        while ( s == (unsigned)-1 )
        {
          if ( face == 0 )      { t = GRID_MAX - t; s = 0; face = 1; }
          else if ( face == 1 ) { s = 0; t = GRID_MAX - t; face = 0; }
          else                  { s = GRID_MAX - t; face = 1; t = 0; }
        }
		
        /* s overflow: wrap to adjacent face along +s edge */
        if ( s != GRID_SIZE )
          break;
        if ( face == 0 )      { s = GRID_MAX; face = 1; }
        else if ( face == 1 ) { s = GRID_MAX; face = 0; }
        else                  { s = t; face = 1; t = GRID_MAX; }
      }
	  
      /* t underflow: wrap to adjacent face along -t edge */
      if ( t != (unsigned)-1 )
        break;
      if ( face == 0 )      { t = 0; s = GRID_MAX - s; face = 2; }
      else if ( face == 1 ) { t = GRID_MAX - s; s = 0; face = 2; }
      else                  { t = 0; s = GRID_MAX - s; face = 0; }
    }
	
    /* t overflow: wrap to adjacent face along +t edge */
    if ( t != GRID_SIZE )
      break;
    if ( face == 0 )      { t = GRID_MAX; face = 2; }
    else if ( face == 1 ) { t = s; s = GRID_MAX; face = 2; }
    else                  { t = GRID_MAX; face = 0; }
  }
  Assert(face <= 2, s_assertDisable_TjuncClampNode);
  Assert(s < GRID_SIZE, s_assertDisable_TjuncClampNode);
  Assert(t < GRID_SIZE, s_assertDisable_TjuncClampNode);
  return tjuncNodeHash[GRID_SIZE * GRID_SIZE * face + GRID_SIZE * t + s];
  #undef GRID_MAX
  #undef GRID_SIZE
}

/*
================
TJunc_ProcessBrushList

Iterates brush list, inserts non-detail brushes into grid tree
================
*/
int TJunc_ProcessBrushList(TriSurf_t *brushList)
{
  TriSurf_t *brush;

  for ( brush = brushList; brush; brush = brush->next )
  {
    if ( !brush->props->surfFlagBit7 )
      GridTree_Insert(brush);
  }
  return 0;
}

/*
================
TJunc_SetBounds

Sets global bounding box for edge line hash computation
================
*/
void TJunc_SetBounds(float *boundsMin, float *boundsMax)
{
  VectorCopy(boundsMin, tjuncBoundsMin);
  VectorCopy(boundsMax, tjuncBoundsMax);
}

/*
================
TJunc_SetSnapTolerance

Sets squared snap tolerance globals
================
*/
void TJunc_SetSnapTolerance(float tolerance)
{
  tjuncSnapTolSq = tolerance * tolerance;
  tjuncHashTolSq = tolerance * TJUNC_HASH_TOLERANCE_FACTOR * (tolerance * 23.0);
}

/*
================
TJunc_SetCreateNonAxial

Sets flag for non-axial edge line creation
================
*/
char TJunc_SetCreateNonAxial( char enabled )
{
  tjuncCreateNonAxial = enabled;
  return enabled;
}

/*
================
TJunc_Init

Initializes T-junction system, allocates aux data buffer
================
*/
void *TJunc_Init( int auxElemSize )
{
  Assert(!tjuncPointCount, s_assertDisable_TJunc_Init);

  if ( auxElemSize != tjuncAuxElemSize )
  {
    tjuncAuxElemSize = auxElemSize;
    free(tjuncAuxBuffer);

    if ( tjuncAuxElemSize )
      tjuncAuxBuffer = malloc(tjuncAuxElemSize * MAX_TJUNC_AUX_POINTS);
    else
      tjuncAuxBuffer = NULL;
  }
  return tjuncAuxBuffer;
}

/*
================
TJunc_UpdateSnapPoint

Update snap point XYZ if new vertex is closer to edge line
================
*/
void TJunc_UpdateSnapPoint( float *newVertex, TjuncPoint_t *pt, int flags, TjuncEdgeLine_t *el )
{
  double dOldA, dOldB, dNewA, dNewB;

  pt->flags |= flags;

  /* perpendicular distance of existing snap point to edge planes */
  dOldA = DotProduct120(pt->pos, el->normalA) - (double)el->distA;
  dOldB = DotProduct210(pt->pos, el->normalB) - (double)el->distB;

  /* perpendicular distance of new vertex to edge planes */
  dNewA = DotProduct210(newVertex, el->normalA) - (double)el->distA;
  dNewB = DotProduct210(newVertex, el->normalB) - (double)el->distB;

  /* update if new vertex is closer to edge line */
  { double distNew = MulAdd2(dNewA,dNewA, dNewB,dNewB);
    double distOld = MulAdd2(dOldA,dOldA, dOldB,dOldB);
    if ( distNew < distOld )
      VectorCopy(newVertex, pt->pos);
  }
}

/*
================
TJunc_InsertPoint

Inserts point into edge line's sorted chain
================
*/
void TJunc_InsertPoint(float *vertex, const void *auxData, float intercept, int flags, TjuncEdgeLine_t *edgeLine)
{
  TjuncPoint_t *newPt, *scan, *prev;

  if ( tjuncPointCount == MAX_TJUNC_POINTS )
    Com_Error("MAX_TJUNC_POINTS");

  newPt = &((TjuncPoint_t *)tjuncPoints)[tjuncPointCount];
  newPt->intercept = intercept;
  VectorCopy(vertex, newPt->pos);
  newPt->auxData = NULL;
  newPt->flags = flags;

  /* walk the sorted chain to find insertion point */
  scan = edgeLine->sentinel.next;
  while ( scan != &edgeLine->sentinel )
  {
    double diff = scan->intercept - intercept;

    /* close enough to merge? */
    if ( diff * diff < edgeLine->tolerance )
    {
      if ( !tjuncAuxElemSize || memcmp(auxData, scan->auxData, tjuncAuxElemSize) == 0 )
      {
        TJunc_UpdateSnapPoint(vertex, scan, flags, edgeLine);
        return;
      }
    }

    if ( intercept < (double)scan->intercept )
      break;
    scan = scan->next;
  }

  /* insert new point before scan */
  if ( tjuncAuxElemSize )
  {
    char *dst = (char *)tjuncAuxBuffer + tjuncPointCount * tjuncAuxElemSize;
    newPt->auxData = dst;
    memcpy(dst, auxData, tjuncAuxElemSize);
  }
  tjuncPointCount++;

  prev = scan->prev;
  newPt->next = scan;
  newPt->prev = prev;
  prev->next = newPt;
  scan->prev = newPt;
}

/*
================
TJunc_ClassifyAndInsertEdge

Classifies edge direction, inserts both endpoints
================
*/
void TJunc_ClassifyAndInsertEdge(float *vertA, float *vertB, TjuncEdgeLine_t *el, const void *auxDataA, const void *auxDataB, float tolerance)
{
  float dotA = DotProduct210(vertA, el->edgeDir);
  float dotB = DotProduct210(vertB, el->edgeDir);
  double diff = (double)dotB - dotA;
  int dirFlags;

  if ( diff * diff >= tolerance )
    dirFlags = (diff >= 0.0) ? 1 : 2;
  else
    dirFlags = 4;

  if ( el->tolerance > (double)tolerance )
    el->tolerance = tolerance;
  TJunc_InsertPoint(vertA, auxDataA, dotA, dirFlags, el);
  TJunc_InsertPoint(vertB, auxDataB, dotB, dirFlags, el);
}

/*
================
TJunc_HashLookup

Computes spatial hash bucket from axis + vertex position
================
*/
float **TJunc_HashLookup(int axis, float *vertex)
{
  #define SPATIAL_DIM 128
  int ax0 = (~axis) & 1;
  int ax1 = (~axis) & 2;
  int col, row;

  /* normalize vertex position to [0, SPATIAL_DIM) grid, clamp */
  col = fistp_sub((vertex[ax0] - tjuncBoundsMin[ax0]) / (tjuncBoundsMax[ax0] - tjuncBoundsMin[ax0] + 1.0) * SPATIAL_DIM, FISTP_HALF_BIAS);
  if ( col < 0 ) col = 0; else if ( col > SPATIAL_DIM - 1 ) col = SPATIAL_DIM - 1;

  row = fistp_sub((vertex[ax1] - tjuncBoundsMin[ax1]) / (tjuncBoundsMax[ax1] - tjuncBoundsMin[ax1] + 1.0) * SPATIAL_DIM, FISTP_HALF_BIAS);
  if ( row < 0 ) row = 0; else if ( row > SPATIAL_DIM - 1 ) row = SPATIAL_DIM - 1;

  return (float **)&tjuncSpatialHash[col + SPATIAL_DIM * (row + SPATIAL_DIM * axis)];
  #undef SPATIAL_DIM
}

/*
================
TJunc_DirectionToHashKey

Converts direction to octree hash key (axis, s, t)
================
*/
int TJunc_DirectionToHashKey(int *outS, float *direction, int *outAxis, int *outT)
{
  int result;
  float normCoordT;
  float normCoordS;

  result = VecLargestAxis(direction);
  int idx1 = (~result) & 1;
  normCoordS = (direction[idx1] / direction[result] + 1.0f) * TJUNC_COORD_SCALE;
  *outS = fistp_sub(normCoordS, FISTP_HALF_BIAS);

  int idx2 = (~result) & 2;
  normCoordT = (direction[idx2] / direction[result] + 1.0f) * TJUNC_COORD_SCALE;
  *outT = fistp_sub(normCoordT, FISTP_HALF_BIAS);
  *outAxis = result;

  return result;
}

/*
================
TJunc_AddEdgeLine

Creates edge line struct, adds to hash, inserts endpoints
================
*/
void TJunc_AddEdgeLine(float *vertA, float *vertB, const void *auxDataA, const void *auxDataB, int hashAxis, float tolerance)
{
  #define MAX_EDGE_LINES 0x10000
  #define TJUNC_HASH_DIM 8
  TjuncEdgeLine_t *el;
  vec3_t edgeDir;
  int hashAxisOut, hashS, hashT, hashIdx;

  if ( tjuncLineCount == MAX_EDGE_LINES )
    Com_Error("MAX_EDGE_LINES");

  el = &tjuncEdgeLines[tjuncLineCount];
  VectorSubtract(vertB, vertA, edgeDir);
  if ( Vec3NormalizeTo(edgeDir, el->edgeDir) == 0.0 )
    return;

  VectorCopy(vertA, el->startVert);
  MakeNormalVectors(el->edgeDir, el->normalA, el->normalB);
  el->distA = DotProduct120(el->normalA, vertA);
  el->distB = DotProduct120(el->normalB, vertA);
  el->sentinel.prev = &el->sentinel;
  el->sentinel.next = &el->sentinel;
  el->tolerance = tolerance;

  TJunc_DirectionToHashKey(&hashS, edgeDir, &hashAxisOut, &hashT);
  hashIdx = (hashS >> 1) + TJUNC_HASH_DIM * ((hashT >> 1) + TJUNC_HASH_DIM * hashAxisOut);
  el->hashNext = tjuncNodeHash[hashIdx];
  tjuncNodeHash[hashIdx] = el;

  TJunc_ClassifyAndInsertEdge(vertA, vertB, el, auxDataA, auxDataB, tolerance);
  tjuncLineCount++;

  if ( hashAxis >= 0 )
  {
    float **bucket = TJunc_HashLookup(hashAxis, vertA);
    el->hashNext2 = (TjuncEdgeLine_t *)*bucket;
    *bucket = (float *)el;
  }
  #undef MAX_EDGE_LINES
  #undef TJUNC_HASH_DIM
}

/*
================
TJunc_DistToEdgeLine

Max squared perpendicular distance of two vertices to edge line.
================
*/
double TJunc_DistToEdgeLine(TjuncEdgeLine_t *el, float *vertB, float *vertA, float epsilon)
{
  double d;
  float sq, maxSq;

  /* test each vertex against both perpendicular planes — early out if too far */
  d = DotProduct120(vertA, el->normalA) - (double)el->distA;
  maxSq = d * d;
  if ( maxSq > (double)epsilon ) return maxSq;

  d = DotProduct210(vertA, el->normalB) - (double)el->distB;
  sq = d * d;
  if ( sq > (double)epsilon ) return sq;
  if ( sq > maxSq ) maxSq = sq;

  d = DotProduct120(vertB, el->normalA) - (double)el->distA;
  sq = d * d;
  if ( sq > (double)epsilon ) return sq;
  if ( sq > maxSq ) maxSq = sq;

  d = DotProduct210(vertB, el->normalB) - (double)el->distB;
  sq = d * d;
  if ( sq > (double)epsilon ) return sq;
  if ( sq > maxSq ) maxSq = sq;

  return maxSq;
}

/*
================
TJunc_FindEdgeLineByHash

Searches 3x3 hash neighborhood for closest edge line
================
*/
TjuncEdgeLine_t *TJunc_FindEdgeLineByHash(float *direction, float *lastVertex, float *curVertex, float tolerance)
{
  int hashAxis, hashS, hashT;
  int sCenter, tCenter;
  int dr, dc;
  TjuncEdgeLine_t *best = NULL;
  double bestDist = tolerance;

  TJunc_DirectionToHashKey(&hashS, direction, &hashAxis, &hashT);
  sCenter = (hashS + 1) >> 1;
  tCenter = (hashT + 1) >> 1;

  /* search 3x3 neighborhood of hash cells */
  for ( dr = -1; dr <= 0; dr++ )
  {
    for ( dc = -1; dc <= 0; dc++ )
    {
      TjuncEdgeLine_t *node;
      for ( node = TjuncClampNode(tCenter + dr, hashAxis, sCenter + dc); node; node = node->hashNext )
      {
        double dist = TJunc_DistToEdgeLine(node, lastVertex, curVertex, bestDist);
        if ( dist < bestDist )
        {
          bestDist = dist;
          best = node;
        }
      }
    }
  }
  return best;
}

/*
================
TJunc_FindEdgeLineBrute

Brute-force searches all edge lines for closest match
================
*/
TjuncEdgeLine_t *TJunc_FindEdgeLineBrute( float *lastVertex, float *curVertex, float tolerance )
{
  int i;
  TjuncEdgeLine_t *result = NULL;
  TjuncEdgeLine_t *node = tjuncEdgeLines;

  for ( i = 0; i < tjuncLineCount; i++, node++ )
  {
    double dist = TJunc_DistToEdgeLine(node, lastVertex, curVertex, tolerance);
    if ( tolerance > dist )
    {
      tolerance = dist;
      result = node;
    }
  }
  return result;
}

/*
================
TJunc_FindMatchingEdgeLine

Dispatches to hash or brute-force edge search by direction magnitude
================
*/
TjuncEdgeLine_t *TJunc_FindMatchingEdgeLine(float *direction, float *curVertex, float *lastVertex, float epsilon)
{
  float magSq = DotProduct(direction, direction);

  if ( magSq <= tjuncHashTolSq )
    return TJunc_FindEdgeLineBrute(curVertex, lastVertex, epsilon);
  else
    return TJunc_FindEdgeLineByHash(direction, lastVertex, curVertex, epsilon);
}

/*
================
TJunc_FindEdgeLineByIndex

Searches hash bucket by axis index for closest edge line
================
*/
TjuncEdgeLine_t *TJunc_FindEdgeLineByIndex(int hashAxis, float *curVertex, float *lastVertex, float tolerance)
{
  TjuncEdgeLine_t *result;
  TjuncEdgeLine_t *node;
  double dist;

  result = NULL;
  for ( node = (TjuncEdgeLine_t *)*TJunc_HashLookup(hashAxis, lastVertex); node; node = node->hashNext2 )
  {
    dist = TJunc_DistToEdgeLine(node, curVertex, lastVertex, tolerance);
    if ( tolerance > dist )
    {
      tolerance = dist;
      result = node;
    }
  }
  return result;
}

/*
================
TJunc_FindEdge

Main edge finder: computes epsilon, dispatches to index or matching search
================
*/
TjuncEdgeLine_t *TJunc_FindEdge(float *curVertex, float *outEpsilon, float *lastVertex, int hashAxis)
{
  vec3_t dir;
  double lenSqScaled;
  float epsilon;

  VectorSubtract(curVertex, lastVertex, dir);
  lenSqScaled = DotProduct(dir, dir) * 0.25;
  epsilon = (tjuncSnapTolSq > lenSqScaled) ? (float)lenSqScaled : tjuncSnapTolSq;

  Assert(outEpsilon, s_assertDisable_TJunc_FindEdge);
  *outEpsilon = epsilon;

  if ( hashAxis >= 0 )
    return TJunc_FindEdgeLineByIndex(hashAxis, lastVertex, curVertex, epsilon);
  return TJunc_FindMatchingEdgeLine(dir, lastVertex, curVertex, epsilon);
}

/*
================
TJunc_ClassifyEdgeAxis

Classifies edge as axis-aligned (0=X, 1=Y, 2=Z) or non-axial (-1)
================
*/
int TJunc_ClassifyEdgeAxis(float *vertA, float *vertB)
{

  if ( !tjuncCreateNonAxial ) {
    return -1;
  }

  volatile float threshold = tjuncSnapTolSq * 0.25f;

  /* compute per-axis significance flags */
  double dx = (double)vertB[0] - vertA[0];
  volatile float dy = vertB[1] - vertA[1];
  volatile float dz = vertB[2] - vertA[2];
  int flags = 0;

  if (dx * dx > threshold) flags |= 1;
  if ((double)dy * dy > threshold) flags |= 2;
  if ((double)dz * dz > threshold) flags |= 4;

  /* return axis only if exactly one axis is significant */
  if (flags == 1) return 0;
  if (flags == 2) return 1;
  if (flags == 4) return 2;
  return -1;
}

/*
================
TJunc_ProcessWinding

Processes winding edges: finds/creates edge lines, inserts endpoints
================
*/
int TJunc_ProcessWinding( WindingAuxPair_t *verts )
{
  Winding_t *w;
  char *auxBase;
  int numPts, i, prev;
  float epsilonOut;
  TjuncEdgeLine_t *edgeLine;
  int edgeAxis;

  Assert(verts, s_assertDisable_TJunc_ProcessWinding);
  Assert(verts->winding, s_assertDisable_TJunc_ProcessWinding);
  Assert(verts->auxData || !tjuncAuxElemSize, s_assertDisable_TJunc_ProcessWinding);

  w = verts->winding;
  numPts = w->numpoints;
  auxBase = (char *)verts->auxData;

  /* process each edge: (prev vertex → current vertex) */
  for ( i = 0; i < numPts; i++ )
  {
    prev = (i + numPts - 1) % numPts;
    edgeAxis = TJunc_ClassifyEdgeAxis(w->points[i], w->points[prev]);
    edgeLine = TJunc_FindEdge(w->points[prev], &epsilonOut, w->points[i], edgeAxis);

    if ( edgeLine )
      TJunc_ClassifyAndInsertEdge(w->points[prev], w->points[i], edgeLine, auxBase + tjuncAuxElemSize * prev, auxBase + tjuncAuxElemSize * i, epsilonOut);
    else
      TJunc_AddEdgeLine(w->points[prev], w->points[i], auxBase + tjuncAuxElemSize * prev, auxBase + tjuncAuxElemSize * i, edgeAxis, epsilonOut);
  }

  return 0;
}

/*
================
TJunc_ProcessSurface

Processes surface: main winding + all hole windings
================
*/
int TJunc_ProcessSurface( TriSurf_t *ts )
{
  int i;

  Assert(ts->auxElemSize == tjuncAuxElemSize || !tjuncAuxElemSize, s_assertDisable_TJunc_ProcessSurface);

  /* process main winding */
  TJunc_ProcessWinding((WindingAuxPair_t *)&ts->winding);

  /* process hole windings */
  for ( i = 0; i < ts->holeCount; i++ )
    TJunc_ProcessWinding(&ts->holes[i]);

  return ts->holeCount;
}

/*
================
FixSurfaceJunctions

Inserts T-junction points into winding, expands vertex list
================
*/
void FixSurfaceJunctions(WindingAuxPair_t *verts, int auxElemSize, float *surfPlane)
{
  Winding_t *winding, *newWinding;
  TjuncEdgeLine_t *edge;
  TjuncPoint_t *chainNode, *sentinel;
  float *curVert, *nextVert, *outVert;
  vec3_t outVertBuf[MAX_CONCAVE_WINDING_POINTS];
  void *auxBlock;
  int srcIdx, dstIdx, nextIdx, edgeAxis;
  float epsilonOut, snapDist;
  float curIntercept, nextIntercept;
  double planeDist;
  int forward;

  auxBlock = auxElemSize ? malloc(auxElemSize * MAX_CONCAVE_WINDING_POINTS) : NULL;
  winding = verts->winding;
  dstIdx = 0;

  for ( srcIdx = 0; srcIdx < winding->numpoints; srcIdx++ )
  {
    curVert = winding->points[srcIdx];

    /* copy current vertex to output */
    if ( dstIdx == MAX_CONCAVE_WINDING_POINTS )
      Com_Error("MAX_POINTS_ON_CONCAVE_WINDING");
    outVert = outVertBuf[dstIdx];
    VectorCopy(curVert, outVert);
    if ( auxElemSize )
      AuxDataCopy(auxElemSize, (char *)verts->auxData, srcIdx, 1, (char *)auxBlock, dstIdx);
    dstIdx++;

    /* find edge line between this vertex and the next */
    nextIdx = (srcIdx + 1) % winding->numpoints;
    nextVert = winding->points[nextIdx];
    edgeAxis = TJunc_ClassifyEdgeAxis(nextVert, curVert);
    edge = TJunc_FindEdge(curVert, &epsilonOut, nextVert, edgeAxis);
    if ( !edge )
      continue;

    /* tighten tolerance and compute snap distance */
    if ( edge->tolerance > (double)epsilonOut )
      edge->tolerance = epsilonOut;
    snapDist = (float)sqrt(edge->tolerance);

    /* project both endpoints onto the edge line */
    curIntercept = DotProduct210(edge->edgeDir, curVert);
    nextIntercept = DotProduct210(edge->edgeDir, nextVert);

    /* walk the chain of junction points along this edge */
    forward = curIntercept >= (double)nextIntercept;
    chainNode = forward ? edge->sentinel.prev : edge->sentinel.next;
    sentinel = &edge->sentinel;

    while ( chainNode != sentinel )
    {
      float chainIntercept = chainNode->intercept;

      /* check if we've passed the endpoint
         On x86, x87 keeps float+float at 80-bit extended for comparison — matches original.
         On x64, SSE2 does float+float at 32-bit, losing the extra precision.
         Use double on x64 to match x86's 80-bit behavior. */
#ifdef _WIN64
      if ( !forward && (double)nextIntercept - (double)snapDist < chainIntercept )
        break;
      if ( forward && (double)nextIntercept + (double)snapDist > chainIntercept )
        break;

      /* check if junction point is between the two vertices */
      if ( (!forward && (double)curIntercept + (double)snapDist < chainIntercept)
        || (forward && (double)curIntercept - (double)snapDist > chainIntercept) )
#else
      if ( !forward && nextIntercept - snapDist < chainIntercept )
        break;
      if ( forward && nextIntercept + snapDist > chainIntercept )
        break;

      /* check if junction point is between the two vertices */
      if ( (!forward && curIntercept + snapDist < chainIntercept)
        || (forward && curIntercept - snapDist > chainIntercept) )
#endif
      {
        /* check if junction point lies on the surface plane */
        planeDist = DotProduct210(chainNode->pos, surfPlane) - surfPlane[3];
        if ( planeDist > -snapDist && planeDist < snapDist )
        {
          if ( dstIdx == MAX_CONCAVE_WINDING_POINTS )
            Com_Error("MAX_POINTS_ON_CONCAVE_WINDING");
          outVert = outVertBuf[dstIdx];
          VectorCopy(chainNode->pos, outVert);
          if ( auxElemSize )
          {
            double frac = (chainNode->intercept - curIntercept) / (nextIntercept - curIntercept);
            g_lerpAuxDataCallback(
                (float *)((char *)verts->auxData + srcIdx * auxElemSize),
                (float *)((char *)verts->auxData + nextIdx * auxElemSize),
                frac,
                (float *)((char *)auxBlock + dstIdx * auxElemSize)
            );
          }
          dstIdx++;
        }
      }
      chainNode = forward ? chainNode->prev : chainNode->next;
    }
  }

  /* replace winding if new vertices were added */
  Assert(dstIdx >= verts->winding->numpoints, s_assertDisable_FixSurfaceJunctions);
  if ( dstIdx > verts->winding->numpoints )
  {
    FreeWinding(verts->winding);
    newWinding = AllocWinding(dstIdx);
    verts->winding = newWinding;
    newWinding->numpoints = dstIdx;
    memcpy(newWinding->points, outVertBuf, sizeof(vec3_t) * dstIdx);
    if ( auxElemSize )
    {
      free(verts->auxData);
      verts->auxData = malloc(auxElemSize * dstIdx);
      memcpy(verts->auxData, auxBlock, auxElemSize * dstIdx);
    }
  }

  free(auxBlock);
}

/*
================
TjuncFixSurfaceEdges

Fixes junctions for surface main winding + holes
================
*/
int TjuncFixSurfaceEdges( TriSurf_t *ts )
{
  int i;

  FixSurfaceJunctions((WindingAuxPair_t *)&ts->winding, ts->auxElemSize, ts->props->plane);
  for ( i = 0; i < ts->holeCount; i++ )
    FixSurfaceJunctions(&ts->holes[i], ts->auxElemSize, ts->props->plane);
  return ts->holeCount;
}

/*
================
TjuncFixFaces

Orchestrates T-junction fixing: collect edges, reset chains, re-insert, fix surfaces
================
*/
int TjuncFixFaces(intptr_t *surfArray, int surfCount, TriSurf_t *extraSurf)
{
  int i;

  Assert(!tjuncLineCount, s_assertDisable_TjuncFixFaces);
  Assert(!tjuncPointCount, s_assertDisable_TjuncFixFaces);

  /* pass 1: collect all edges */
  if ( extraSurf )
    TJunc_ProcessSurface(extraSurf);
  for ( i = 0; i < surfCount; i++ )
    TJunc_ProcessSurface((TriSurf_t *)surfArray[i]);

  /* reset all edge line linked lists to empty (sentinel → self) */
  for ( i = 0; i < tjuncLineCount; i++ )
  {
    tjuncEdgeLines[i].sentinel.prev = &tjuncEdgeLines[i].sentinel;
    tjuncEdgeLines[i].sentinel.next = &tjuncEdgeLines[i].sentinel;
  }

  /* pass 2: re-insert all edges, then fix junctions */
  tjuncPointCount = 0;
  if ( extraSurf )
    TJunc_ProcessSurface(extraSurf);
  for ( i = 0; i < surfCount; i++ )
    TJunc_ProcessSurface((TriSurf_t *)surfArray[i]);
  for ( i = 0; i < surfCount; i++ )
    TjuncFixSurfaceEdges((TriSurf_t *)surfArray[i]);

  TjuncReset();
  return 0;
}

/*
================
RemoveDegenerateEdges

Removes near-zero-length edges from winding
================
*/
void RemoveDegenerateEdges(WindingAuxPair_t *verts, int auxElemSize)
{
  #define DEGENERATE_EPSILON 0.001f
  Winding_t *w;
  char *auxData;
  int i, next;

  Assert(verts, s_assertDisable_RemoveDegenerateEdges);
  Assert(verts->winding, s_assertDisable_RemoveDegenerateEdges);
  Assert(verts->auxData || !auxElemSize, s_assertDisable_RemoveDegenerateEdges);

  w = verts->winding;
  auxData = (char *)verts->auxData;
  Assert(w->numpoints > 0 && w->numpoints < MAX_CONCAVE_WINDING_POINTS, s_assertDisable_RemoveDegenerateEdges);

  i = 0;
  while ( i < w->numpoints )
  {
    next = (i + 1) % w->numpoints;
    if ( !VectorCompareEpsilon(w->points[i], w->points[next], DEGENERATE_EPSILON, 3) )
    {
      i++;
      continue;
    }

    /* degenerate edge found — remove next vertex by shifting tail */
    if ( next )
    {
      int tail = w->numpoints - next;
      memcpy(w->points[i], w->points[next], sizeof(vec3_t) * tail);
      if ( auxElemSize )
        memcpy(&auxData[auxElemSize * i], &auxData[auxElemSize * next], auxElemSize * tail);
    }
    else
    {
      /* next wrapped to 0 — back up one vertex instead */
      i--;
    }
    w->numpoints--;
  }
  #undef DEGENERATE_EPSILON
}

/*
================
RemoveDegenerateEdgesForFace

Removes degenerate edges from surface main winding + holes
================
*/
int RemoveDegenerateEdgesForFace(TriSurf_t *ts)
{
  int i;
  RemoveDegenerateEdges((WindingAuxPair_t *)&ts->winding, ts->auxElemSize);
  for ( i = 0; i < ts->holeCount; i++ )
    RemoveDegenerateEdges(&ts->holes[i], ts->auxElemSize);
  return ts->holeCount;
}

/*
================
TjuncProcessFace

Fixes T-junctions and removes degenerate edges for one face
================
*/
int TjuncProcessFace(TriSurf_t *surf)
{
  TjuncFixSurfaceEdges(surf);
  return RemoveDegenerateEdgesForFace(surf);
}

static void TJunc_ProcessSurfaceGridCallback(TriSurf_t *ts)
{
  (void)TJunc_ProcessSurface(ts);
}

static void TjuncProcessFaceGridCallback(TriSurf_t *surf)
{
  (void)TjuncProcessFace(surf);
}

/*
================
TjuncOctreeProcess

Recursive octree T-junction processor: subdivides or processes directly
================
*/
int TjuncOctreeProcess(float *boundsMin, float *boundsMax, void (*faceCallback)(TriSurf_t *))
{
  #define OCTREE_MIN_EXTENT 8.0
  #define OCTREE_MAX_SURFACES 256
  float extent[3];
  int maxSurfs, count, oct;
  vec3_t mid, childMin, childMax;

  extent[0] = boundsMax[0] - boundsMin[0];
  extent[1] = boundsMax[1] - boundsMin[1];
  extent[2] = boundsMax[2] - boundsMin[2];
  maxSurfs = (extent[0] >= OCTREE_MIN_EXTENT || extent[1] >= OCTREE_MIN_EXTENT || extent[2] >= OCTREE_MIN_EXTENT) ? OCTREE_MAX_SURFACES : INT_MAX;

  count = GridTree_CountIntersecting(boundsMin, boundsMax, maxSurfs);
  if ( count <= 1 )
    return count;

  /* small enough to process directly */
  if ( count <= maxSurfs )
  {
    GridTree_ForEach(boundsMin, boundsMax, TJunc_ProcessSurfaceGridCallback);
    GridTree_ForEach(boundsMin, boundsMax, faceCallback);
    TjuncReset();
    return 0;
  }

  /* subdivide into 8 octants */
  mid[0] = MIDF(boundsMin[0], boundsMax[0]);
  mid[1] = MIDF(boundsMin[1], boundsMax[1]);
  mid[2] = MIDF(boundsMin[2], boundsMax[2]);

  for ( oct = 0; oct < 8; oct++ )
  {
    int ax;
    for ( ax = 0; ax < 3; ax++ )
    {
      if ( oct & (1 << ax) )
      { childMin[ax] = boundsMin[ax]; childMax[ax] = mid[ax]; }
      else
      { childMin[ax] = mid[ax]; childMax[ax] = boundsMax[ax]; }
    }
    count = TjuncOctreeProcess(childMin, childMax, faceCallback);
  }
  return count;
  #undef OCTREE_MIN_EXTENT
  #undef OCTREE_MAX_SURFACES
}

/*
================
TjuncFixAll

Entry point: runs octree T-junction fixing on world bounds
================
*/
int TjuncFixAll(float *boundsMin, float *boundsMax)
{
  return TjuncOctreeProcess(boundsMin, boundsMax, TjuncProcessFaceGridCallback);
}

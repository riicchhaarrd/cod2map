/*
tris_coalesce.c — Winding coalescing

Reconstructed from cod2map.exe by Rose.

Groups triangle surfaces by shared properties (material, plane, lightmap)
using a spatial BSP tree for fast neighbor lookup. Coplanar surfaces with
matching properties are merged into larger windings to reduce triangle count.
Hash buckets track unique property combinations.
*/

#include "cod2map.h"

CoalesceBspNode_t  *coalesceTreeRoot;
TriSurf_t         **coalesceSurfListHead;

char s_assertDisable_ClipSurfToWinding;
char s_assertDisable_ClipSurfToWinding;
char s_assertDisable_ClipSurfToWinding;
char s_assertDisable_CoalesceChainMatchesArray;
char s_assertDisable_CoalesceChainMatchesArray;
char s_assertDisable_CoalesceChainMatchesArray;
char s_assertDisable_CollectCoalesceLeaves;
char s_assertDisable_FindOrCreateCoalescedSurfProps;
char s_assertDisable_FindOrCreateCoalescedSurfProps;
char s_assertDisable_FreeCoalesceTree;
char s_assertDisable_InitBspLeafNode;
char s_assertDisable_MergeCoplanarSurfs;
char s_assertDisable_MergeCoplanarSurfs;
char s_assertDisable_MergeCoplanarSurfs;
char s_assertDisable_MergeCoplanarSurfs;
char s_assertDisable_MergeCoplanarSurfs;
char s_assertDisable_MergeCoplanarSurfs;
char s_assertDisable_MergeCoplanarSurfs;
char s_assertDisable_MergeCoplanarSurfs;
char s_assertDisable_MergeCoplanarSurfs;
char s_assertDisable_ValidateVisGroupList;
char s_assertDisable_ValidateVisGroupList;
char s_assertDisable_ValidateVisGroupList;


/*
================
CoalesceTreeSearch

Recursively searches coalesce BSP tree, invokes callback on overlapping leaves.
================
*/
int CoalesceTreeSearch(CoalesceBspNode_t *node, float *mins, float *maxs, int (*callback)(CoalesceBspNode_t *, TriSurf_t *), TriSurf_t *userData)
{
  CoalesceBspNode_t *curNode;
  int splitAxis;
  double splitDist;

  curNode = node;
  for ( ;; )
  {
    splitAxis = curNode->splitAxis;
    if ( splitAxis == -1 )
      return callback(curNode, userData);

    splitDist = curNode->splitDist;
    if ( splitDist < mins[splitAxis] )
    {
      curNode = curNode->front;
    }
    else if ( splitDist > maxs[splitAxis] )
    {
      curNode = curNode->back;
    }
    else
    {
      if ( CoalesceTreeSearch(curNode->back, mins, maxs, callback, userData) == 1 )
        return 1;
      curNode = curNode->front;
    }
  }
}

/*
================
CoalesceLeafRemove

Removes surface from coalesce leaf linked list.
================
*/
int CoalesceLeafRemove(CoalesceBspNode_t *leafNode, TriSurf_t *surfPtr)
{
  CoalesceSurfListNode_t **listPtr;
  CoalesceSurfListNode_t *freeNode;

  listPtr = (CoalesceSurfListNode_t **)&leafNode->surfList;
  while ( *listPtr )
  {
    if ( (*listPtr)->surf == surfPtr )
    {
      freeNode = *listPtr;
      *listPtr = freeNode->next;
      free(freeNode);
    }
    else
    {
      listPtr = &(*listPtr)->next;
    }
  }
  return 0;
}

/*
================
CoalesceLeafInsert

Inserts surface into coalesce leaf linked list.
================
*/
int CoalesceLeafInsert(CoalesceBspNode_t *leafNode, TriSurf_t *surfPtr)
{
  CoalesceSurfListNode_t *newNode;

  newNode = malloc(sizeof(CoalesceSurfListNode_t));
  newNode->next = (CoalesceSurfListNode_t *)leafNode->surfList;
  newNode->surf = surfPtr;
  leafNode->surfList = newNode;
  return 0;
}

/*
================
CollectCoalesceLeaves

Collects groupable surface props into sorted array.
================
*/
int CollectCoalesceLeaves(uintptr_t surfProps, intptr_t *outArray, int propsCount, int *groupCallback)
{
  CoalesceNode_t *chainPtr;
  int result;
  int insertIdx;
  uintptr_t *sortPtr;
  float errorPos[3];

  chainPtr = ((TriSurfProps_t *)surfProps)->coalesceChain;
  if ( chainPtr )
  {
    result = propsCount;
    do
    {
      result = CollectCoalesceLeaves(chainPtr->value, outArray, result, groupCallback);
      chainPtr = chainPtr->next;
    }
    while ( chainPtr );
    return result;
  }

  insertIdx = 0;
  while ( insertIdx < propsCount )
  {
    if ( TriSurfPropsGroupable((TriSurfProps_t *)outArray[insertIdx], (TriSurfProps_t *)surfProps, groupCallback) )
      return propsCount;
    ++insertIdx;
  }

  #define MAX_COINCIDENT_WINDINGS 8
  if ( propsCount == MAX_COINCIDENT_WINDINGS )
  {
    WindingCenter((Winding_t *)groupCallback, errorPos);
    Com_Error(
      "MAX_COINCIDENT_WINDINGS (%i) exceeded.  Too many polys overlap at %g %g %g.\n",
      MAX_COINCIDENT_WINDINGS,
      errorPos[0],
      errorPos[1],
      errorPos[2]);
  }
  AssertFatal(insertIdx == propsCount, s_assertDisable_CollectCoalesceLeaves);
  if ( insertIdx > 0 )
  {
    sortPtr = (uintptr_t *)&outArray[insertIdx - 1];
	
    /* Insertion sort by pointer value ascending — bump allocator guarantees
       sequential allocs have ascending addresses, matching original behavior. */
    do
    {
#ifndef DETERMINISTIC_SORT
#define DETERMINISTIC_SORT
#endif
#ifdef DETERMINISTIC_SORT

      /* deterministic: sort by plane values as unsigned DWORDs instead of heap pointer.
         Must use DWORD comparison (not memcmp) to match binary-patched original exe
         which compares plane[0..3] as unsigned 32-bit integers. On little-endian,
         memcmp (byte-order) differs from DWORD comparison (integer-order). */
      { unsigned int *a = (unsigned int *)((TriSurfProps_t *)*sortPtr)->plane;
        unsigned int *b = (unsigned int *)((TriSurfProps_t *)surfProps)->plane;
        int cmp = (a[0] > b[0]) - (a[0] < b[0]);
        if ( cmp == 0 ) cmp = (a[1] > b[1]) - (a[1] < b[1]);
        if ( cmp == 0 ) cmp = (a[2] > b[2]) - (a[2] < b[2]);
        if ( cmp == 0 ) cmp = (a[3] > b[3]) - (a[3] < b[3]);
        if ( cmp <= 0 ) break;
      }
#else
      if ( *sortPtr <= (uintptr_t)surfProps )
        break;
#endif
      sortPtr[1] = *sortPtr;
      --insertIdx;
      --sortPtr;
    }
    while ( insertIdx > 0 );
  }
  outArray[insertIdx] = surfProps;
  return propsCount + 1;
}

/*
================
CoalesceChainMatchesArray

Checks if coalesce chain matches sorted array.
================
*/
int CoalesceChainMatchesArray(CoalesceNode_t *chain, int count, intptr_t *array)
{
  CoalesceNode_t *chainPtr;
  int matchIdx;

  chainPtr = chain;
  Assert(chain, s_assertDisable_CoalesceChainMatchesArray);
  Assert(array, s_assertDisable_CoalesceChainMatchesArray);
  Assert(count >= 2, s_assertDisable_CoalesceChainMatchesArray);
  matchIdx = 0;
  if ( count <= 0 )
    return chainPtr == 0;
  while ( chainPtr && array[matchIdx] == chainPtr->value )
  {
    chainPtr = chainPtr->next;
    if ( ++matchIdx >= count )
      return chainPtr == 0;
  }
  return 0;
}

/*
================
BuildCoalesceChain

Builds coalesce chain linked list from sorted array.
================
*/
CoalesceNode_t *BuildCoalesceChain(intptr_t *array, int count)
{
  CoalesceNode_t *newNode;

  if ( !count )
    return 0;
  newNode = malloc(sizeof(*newNode));
  newNode->value = array[0];
  newNode->next = BuildCoalesceChain(array + 1, count - 1);
  return newNode;
}

/*
================
InitBspLeafNode

Initializes BSP node as leaf with surface list.
================
*/
int InitBspLeafNode(CoalesceBspNode_t *bspNode, CoalesceSurfListNode_t *surfList)
{
  Assert(bspNode, s_assertDisable_InitBspLeafNode);
  bspNode->splitAxis = -1;
  bspNode->surfList = surfList;
  return 0;
}

/*
================
FreeCoalesceTree

Recursively frees coalesce BSP tree.
================
*/
void FreeCoalesceTree(CoalesceBspNode_t *node)
{
  DrawSurfRef_t *freeNode;

  Assert(node, s_assertDisable_FreeCoalesceTree);
  if ( node->splitAxis == -1 )
  {
    while ( node->surfList )
    {
      freeNode = (DrawSurfRef_t *)node->surfList;
      node->surfList = freeNode->next;
      free(freeNode);
    }
  }
  else
  {
    FreeCoalesceTree(node->front);
    FreeCoalesceTree(node->back);
  }
  free(node);
}

/*
================
ValidateVisGroupList

Validates vis group doubly-linked list integrity.
================
*/
void ValidateVisGroupList(TriSurf_t **listHead)
{
  TriSurf_t *curSurf;
  TriSurf_t *nextSurf;

  Assert(listHead, s_assertDisable_ValidateVisGroupList);
  curSurf = *listHead;
  if ( curSurf )
  {
    Assert(!curSurf->prev, s_assertDisable_ValidateVisGroupList);
    for ( nextSurf = curSurf->next; nextSurf; nextSurf = curSurf->next )
    {
      Assert(nextSurf->prev == curSurf, s_assertDisable_ValidateVisGroupList);
      curSurf = curSurf->next;
    }
  }
}

/*
================
FindOrCreateCoalescedSurfProps

Finds or creates merged surface properties.
================
*/
TriSurfProps_t *FindOrCreateCoalescedSurfProps(int *groupCallback, TriSurfProps_t *props0, TriSurfProps_t *props1)
{
  TriSurfProps_t *found;
  intptr_t coalesceArray[8];
  int count, i, hash, bucket;
  CoalesceNode_t *chain;

  if ( TriSurfPropsGroupable(props0, props1, groupCallback) )
    return props0;

  /* collect all leaf props from both sides */
  count = CollectCoalesceLeaves((uintptr_t)props0, coalesceArray, 0, groupCallback);
  count = CollectCoalesceLeaves((uintptr_t)props1, coalesceArray, count, groupCallback);
  Assert(count >= 2, s_assertDisable_FindOrCreateCoalescedSurfProps);

  /* hash the coalesce array bytes */
  hash = 0;
  { unsigned char *bytes = (unsigned char *)coalesceArray;
  int numBytes = sizeof(intptr_t) * count;
  for ( i = 0; i < numBytes; i++ )
    hash = hash * (COALESCE_HASH_PRIME + i) + bytes[i];
  }
  bucket = (unsigned char)hash;

  /* search hash bucket for existing match */
  for ( found = g_windingHashBuckets[bucket]; found; found = found->freeListNext )
  {
    Assert(found->coalesceChain, s_assertDisable_FindOrCreateCoalescedSurfProps);
    if ( CoalesceChainMatchesArray(found->coalesceChain, count, coalesceArray) )
      return found;
  }

  /* create new merged props */
  found = malloc(sizeof(TriSurfProps_t));
  memset(found, 0, sizeof(TriSurfProps_t));
  Vector4Copy(props0->plane, found->plane);
  found->contentFlagBit8 = props0->contentFlagBit8 || props1->contentFlagBit8;
  found->surfFlagBit7 = props0->surfFlagBit7 && props1->surfFlagBit7;

  /* subdivisions: if either is 0, sum them; otherwise take the smaller */
  if ( props0->subdivisions == 0.0f || props1->subdivisions == 0.0f )
    found->subdivisions = props0->subdivisions + props1->subdivisions;
  else
    found->subdivisions = props0->subdivisions < props1->subdivisions ? props0->subdivisions : props1->subdivisions;

  /* build coalesce chain */
  if ( count )
  {
    chain = malloc(sizeof(*chain));
    chain->value = coalesceArray[0];
    chain->next = BuildCoalesceChain(&coalesceArray[1], count - 1);
  }
  else
  {
    chain = NULL;
  }
  found->coalesceChain = chain;
  found->freeListNext = g_windingHashBuckets[bucket];
  g_windingHashBuckets[bucket] = found;
  return found;
}

/*
================
BuildBspSplitNode

Attempts BSP split of surface list along chosen axis.
================
*/
char BuildBspSplitNode(CoalesceSurfListNode_t *surfList, int splitAxis, CoalesceBspNode_t *outNode, float *parentMins, float *parentMaxs, int surfCount)
{
  CoalesceSurfListNode_t *node, *next, *leftList, *rightList, *dup;
  volatile float mid = MIDF(parentMins[splitAxis], parentMaxs[splitAxis]);
  vec3_t leftMins, rightMaxs;
  int frontCount = 0, backCount = 0;

  /* count how many surfaces go to each side */
  for ( node = surfList; node; node = node->next )
  {
    TriSurf_t *s = node->surf;
    if ( s->mins[splitAxis] > mid )
      frontCount++;
    else
    {
      backCount++;
      if ( s->maxs[splitAxis] >= mid )
        frontCount++;
    }
  }
  if ( frontCount == surfCount || backCount == surfCount )
    return 0;

  /* partition surfaces into left (back) and right (front) lists */
  leftList = rightList = NULL;
  for ( node = surfList; node; node = next )
  {
    TriSurf_t *s = node->surf;
    next = node->next;

    if ( s->mins[splitAxis] <= mid )
    {
      if ( s->maxs[splitAxis] < mid )
      {
        /* entirely on back side */
        node->next = leftList;
        leftList = node;
        continue;
      }
	  
      /* straddles — duplicate node for back side */
      dup = malloc(sizeof(CoalesceSurfListNode_t));
      dup->surf = s;
      dup->next = leftList;
      leftList = dup;
    }
	
    /* front side (or straddling original) */
    node->next = rightList;
    rightList = node;
  }

  /* split parent bounds at midpoint and recurse */
  VectorCopy(parentMins, leftMins);
  leftMins[splitAxis] = mid;
  VectorCopy(parentMaxs, rightMaxs);
  rightMaxs[splitAxis] = mid;

  outNode->splitDist = mid;
  outNode->splitAxis = splitAxis;
  outNode->back = BuildCoalesceBspNode(parentMins, rightMaxs, leftList, backCount);
  outNode->front = BuildCoalesceBspNode(leftMins, parentMaxs, rightList, frontCount);
  return 1;
}

CoalesceBspNode_t *BuildCoalesceBspNode(float *mins, float *maxs, CoalesceSurfListNode_t *surfList, int surfCount);
int g_buildBspNodeCount = 0;

/*
================
BuildCoalesceBspNode

Recursively builds coalesce BSP node.
================
*/
CoalesceBspNode_t *BuildCoalesceBspNode(float *mins, float *maxs, CoalesceSurfListNode_t *surfList, int surfCount)
{
  CoalesceBspNode_t *newNode;
  int axisIdx;
  int splitAxes[3];

  g_buildBspNodeCount++;
  newNode = malloc(sizeof(*newNode));
  if ( surfCount > COALESCE_MIN_SPLIT_SURFS )
  {
    ChooseSplitAxes(mins, maxs, splitAxes);
    axisIdx = 0;
    while ( !BuildBspSplitNode(surfList, splitAxes[axisIdx], newNode, mins, maxs, surfCount) )
    {
      if ( ++axisIdx >= 3 )
      {
        Assert(newNode, s_assertDisable_InitBspLeafNode);
        newNode->splitAxis = -1;
        newNode->surfList = surfList;
        return newNode;
      }
    }
    return newNode;
  }
  else
  {
    InitBspLeafNode(newNode, surfList);
    return newNode;
  }
}

/*
================
BuildCoalesceBspTree

Builds coalesce BSP tree from vis group list.
================
*/
CoalesceBspNode_t *BuildCoalesceBspTree(TriSurf_t **listHead)
{
  TriSurf_t *curSurf;
  CoalesceSurfListNode_t *listNode;
  CoalesceBspNode_t *result;
  float treeMins[3];
  float treeMaxs[3];
  CoalesceSurfListNode_t *surfList;
  int surfCount;

  ClearBounds(treeMins, treeMaxs);
  surfCount = 0;
  surfList = NULL;
  for ( curSurf = *listHead; curSurf; curSurf = curSurf->next )
  {
    if ( curSurf->props->si->surfaceFlags & SURF_NODRAW )
      continue;
    WindingBounds(curSurf->winding, curSurf->mins, curSurf->maxs);
    AddBoundsToBounds(curSurf->mins, curSurf->maxs, treeMins, treeMaxs);
    surfCount++;
    listNode = malloc(sizeof(CoalesceSurfListNode_t));
    listNode->surf = curSurf;
    listNode->next = surfList;
    surfList = listNode;
  }
  result = BuildCoalesceBspNode(treeMins, treeMaxs, surfList, surfCount);
  coalesceTreeRoot = result;
  coalesceSurfListHead = listHead;
  return result;
}

/*
================
SurfsAreMergeable

Checks if two surfaces are coplanar and geometrically mergeable.
================
*/
char SurfsAreMergeable(TriSurf_t *ts0, TriSurf_t *ts1)
{
  TriSurfProps_t *p0 = ts0->props;
  TriSurfProps_t *p1 = ts1->props;

  if ( DotProduct210(p0->plane, p1->plane) < COPLANAR_DOT_THRESHOLD
    || !BoundsIntersectTolerance(ts0->mins, ts0->maxs, ts1->mins, ts1->maxs, MERGE_EPSILON)
    || !WindingIsOnPlane(
          ts0->winding,
          p1->plane,
          p1->plane[3],
          MERGE_EPSILON)
    || !WindingIsOnPlane(
          ts1->winding,
          p0->plane,
          p0->plane[3],
          MERGE_EPSILON) )
  {
    return 0;
  }
  if ( p0->contentFlagBit8 || p1->contentFlagBit8 )
  {
    if ( CheckWindingSeparation(ts0->winding, ts1->winding, p0->plane, MERGE_EPSILON)
      || CheckWindingSeparation(ts1->winding, ts0->winding, p1->plane, MERGE_EPSILON) )
    {
      return 0;
    }
  }
  else if ( !CheckWindingContainment(ts0->winding, ts1->winding, p0->plane, MERGE_EPSILON)
         || !CheckWindingContainment(ts1->winding, ts0->winding, p1->plane, MERGE_EPSILON) )
  {
    return 0;
  }
  return 1;
}

/*
================
ClipSurfToWinding

Clips surface by winding edges, splits into fragments.
Returns clipped winding in EAX and auxiliary vert data via outAuxVerts (EDX in original).
================
*/
Winding_t *ClipSurfToWinding(TriSurf_t *ts, Winding_t *chopWinding, float *planeNormal, void **outAuxVerts)
{
  Winding_t *result;
  int numEdges, edgeIdx;
  TriSurf_t *newSurf;
  vec3_t edgePlane, edgeDir;
  float edgePlaneDist;
  WindingAuxPair_t remainPair, frontWinding, backWinding;

  Assert(ts, s_assertDisable_ClipSurfToWinding);
  Assert(chopWinding, s_assertDisable_ClipSurfToWinding);
  Assert(planeNormal, s_assertDisable_ClipSurfToWinding);
  remainPair.winding = ts->winding;
  remainPair.auxData = ts->auxData;
  ts->winding = NULL;
  ts->auxData = NULL;
  numEdges = chopWinding->numpoints;

  for ( edgeIdx = 0; edgeIdx < numEdges && remainPair.winding; edgeIdx++ )
  {
    float *curVert = chopWinding->points[edgeIdx];
    float *prevVert = chopWinding->points[(edgeIdx + numEdges - 1) % numEdges];

    VectorSubtract(curVert, prevVert, edgeDir);
    CrossProduct(planeNormal, edgeDir, edgePlane);
    if ( VecNormalize(edgePlane) == 0.0 )
      break;
    edgePlaneDist = (float)DotProduct120(edgePlane, prevVert);

    ClipTriWinding(&remainPair, ts->auxElemSize, edgePlane, edgePlaneDist, MERGE_EPSILON, &frontWinding, &backWinding);
    if ( frontWinding.winding )
    {
      newSurf = InsertTriSurfAfter(frontWinding.winding, frontWinding.auxData, ts->auxElemSize, ts->props, ts);
      WindingBounds(newSurf->winding, newSurf->mins, newSurf->maxs);
      CoalesceTreeSearch(coalesceTreeRoot, newSurf->mins, newSurf->maxs, CoalesceLeafInsert, newSurf);
    }
    FreeVerts(&remainPair);
    remainPair.winding = backWinding.winding;
    remainPair.auxData = backWinding.auxData;
  }

  result = remainPair.winding;
  if ( outAuxVerts )
    *outAuxVerts = remainPair.auxData;
  return result;
}

/*
================
MergeCoplanarSurfs

Merges two coplanar surfaces into one.
================
*/
int MergeCoplanarSurfs(TriSurf_t *ts0, TriSurf_t *ts1)
{
  TriSurfProps_t *mergedProps;
  TriSurf_t *mergedSurf;
  Winding_t *windingCopy;
  
  /* clipResult and clipAux must be contiguous — FreeVerts writes to both fields */
  WindingAuxPair_t clipPair;
  #define clipResult clipPair.winding
  #define clipAux clipPair.auxData

  Assert(ts0, s_assertDisable_MergeCoplanarSurfs);
  Assert(ts1, s_assertDisable_MergeCoplanarSurfs);
  AssertFatal(ts0->winding, s_assertDisable_MergeCoplanarSurfs);
  AssertFatal(ts1->winding, s_assertDisable_MergeCoplanarSurfs);
  AssertFatal(ts0->winding != ts1->winding, s_assertDisable_MergeCoplanarSurfs);
  AssertFatal(ts0->props, s_assertDisable_MergeCoplanarSurfs);
  AssertFatal(ts1->props, s_assertDisable_MergeCoplanarSurfs);
  { TriSurfProps_t *p0 = ts0->props;
  TriSurfProps_t *p1 = ts1->props;
  Assert(!CheckWindingSeparationBoth(ts0->winding, ts1->winding, p0->plane, -MERGE_EPSILON), s_assertDisable_MergeCoplanarSurfs);
  Assert(!CheckWindingSeparationBoth(ts0->winding, ts1->winding, p1->plane, -MERGE_EPSILON), s_assertDisable_MergeCoplanarSurfs);
  CoalesceTreeSearch(coalesceTreeRoot, ts0->mins, ts0->maxs, CoalesceLeafRemove, ts0);
  CoalesceTreeSearch(coalesceTreeRoot, ts1->mins, ts1->maxs, CoalesceLeafRemove, ts1);
  windingCopy = CopyWinding(ts0->winding);
  clipResult = ClipSurfToWinding(ts0, ts1->winding, p1->plane, &clipAux);
  if ( clipResult )
    FreeVerts(&clipPair);
  clipResult = ClipSurfToWinding(ts1, windingCopy, p0->plane, &clipAux);
  if ( clipResult )
  {
    mergedProps = FindOrCreateCoalescedSurfProps((int *)clipResult, ts0->props, ts1->props);
    mergedSurf = InsertTriSurfAfter(clipResult, clipAux, ts0->auxElemSize, mergedProps, ts0);
    WindingBounds(mergedSurf->winding, mergedSurf->mins, mergedSurf->maxs);
    CoalesceTreeSearch(coalesceTreeRoot, mergedSurf->mins, mergedSurf->maxs, CoalesceLeafInsert, mergedSurf);
  }
  FreeWinding(windingCopy);
  ts0->winding = 0;
  ts1->winding = 0;
  UnlinkAndFreeSurf(ts0, coalesceSurfListHead);
  UnlinkAndFreeSurf(ts1, coalesceSurfListHead);
  FreeTriSurf(ts0);
  return FreeTriSurf(ts1); }
}
#undef clipResult
#undef clipAux

/*
================
FindAndMergeSurfInLeaf

BSP callback: finds mergeable surface in leaf and merges it.
================
*/
int FindAndMergeSurfInLeaf(CoalesceBspNode_t *leafNode, TriSurf_t *targetSurf)
{
  CoalesceSurfListNode_t *listPtr;
  TriSurf_t *candidateSurf;
  CoalesceSurfListNode_t *nextNode;

  listPtr = (CoalesceSurfListNode_t *)leafNode->surfList;
  while ( listPtr )
  {
    candidateSurf = listPtr->surf;
    nextNode = listPtr->next;
    if ( targetSurf != candidateSurf && SurfsAreMergeable(targetSurf, candidateSurf) )
    {
      MergeCoplanarSurfs(targetSurf, listPtr->surf);
      return 1;
    }
    listPtr = nextNode;
  }
  return 0;
}

/*
================
CoalesceVisGroup

Merges coplanar surfaces within a vis group.
================
*/
void CoalesceVisGroup( TriSurf_t **listHead )
{
  TriSurf_t **curSurfPtr;

  ValidateVisGroupList(listHead);
  BuildCoalesceBspTree(listHead);

  /* walk the linked list, merging coplanar surfaces via BSP tree search */
  curSurfPtr = listHead;
  while ( *curSurfPtr )
  {
    TriSurf_t *ts = *curSurfPtr;
    if ( CoalesceTreeSearch(coalesceTreeRoot, ts->mins, ts->maxs, FindAndMergeSurfInLeaf, ts) != 1 )
      curSurfPtr = &ts->next;  /* advance to next surface */
  
    /* else: merge happened, curSurfPtr still valid — retry same position */
  }

  FreeCoalesceTree(coalesceTreeRoot);
  coalesceTreeRoot = 0;
  coalesceSurfListHead = NULL;
}

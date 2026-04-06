/*
tris_gridtree.c — Grid tree for spatial lookups

Reconstructed from cod2map.exe by Rose.

Spatial acceleration structure for the tris coalescing system.
Divides world bounds into a grid of cells for fast neighbor queries
during winding merging and overlap detection.
*/

#include "cod2map.h"

GridTreeNode_t *gridTreeRoot;

char      g_fmtTwoInts[8] = { '%', 'i', ',', ' ', '%', 'i', '\0', '\0' };
const int g_gridTreeSplitAxis[GRID_TREE_AXIS_COUNT] = { 0, 1, 0, 1, 0, 1, 2, 0, 1, 0, 1, 0, 1, 2, 0, 1, 0, 1, 0, 1, 2 };

char s_assertDisable_GridTree_ClassifyNode;
char s_assertDisable_GridTree_FindNode;
char s_assertDisable_GridTree_FindNode;
char s_assertDisable_GridTree_FindNode;
char s_assertDisable_GridTree_FindNode;
char s_assertDisable_GridTree_Init;
char s_assertDisable_GridTree_Insert;
char s_assertDisable_GridTree_Remove;
char s_assertDisable_GridTree_Shutdown;
char s_assertDisable_SetGridDivisionPoints;


/*
================
GridTree_ClassifyNode

Classifies a surface AABB against a grid tree split plane.
Returns 1 (front/left), 0 (back/right), or 3 (both/straddling).
================
*/
int GridTree_ClassifyNode(int nodeIdx, unsigned int depth, float *mins, float *maxs)
{
  int splitAxis;

  Assert(depth < GRID_TREE_MAX_DEPTH, s_assertDisable_GridTree_ClassifyNode);
  splitAxis = g_gridTreeSplitAxis[depth];
  
  /* volatile forces float-to-float FPU comparison, not double */
  volatile float maxsVal = maxs[splitAxis];
  volatile float minsVal = mins[splitAxis];
  volatile float gridVal = gridTreeRoot[nodeIdx].splitValue;

  if ( maxsVal <= gridVal )
    return 1;
  if ( minsVal < gridVal )
    return 3;
  return 0;
}

/*
================
GridTree_FindNode

Finds the grid tree leaf node for a given AABB by walking
the binary tree using split plane classification at each depth.
================
*/
GridTreeNode_t *GridTree_FindNode(float *mins, float *maxs)
{
  int nodeIdx;
  unsigned int depth;
  unsigned int side;

  Assert(mins, s_assertDisable_GridTree_FindNode);
  Assert(maxs, s_assertDisable_GridTree_FindNode);
  Assert(gridTreeRoot, s_assertDisable_GridTree_FindNode);
  nodeIdx = 0;
  depth = 0;
  while ( 1 )
  {
    side = GridTree_ClassifyNode(nodeIdx, depth, mins, maxs);
    if ( side == 3 )
      break;
    Assert(side < 2, s_assertDisable_GridTree_FindNode);
    ++depth;
    nodeIdx = side + 2 * nodeIdx + 1;
    if ( depth >= GRID_TREE_MAX_DEPTH )
      return &gridTreeRoot[nodeIdx];
  }
  return &gridTreeRoot[nodeIdx];
}

/*
================
GridTree_Insert

Inserts a surface into the grid tree by finding its leaf node
and prepending it to the node's linked list.
================
*/
TriSurf_t *GridTree_Insert(TriSurf_t *ts)
{
  GridTreeNode_t *leafNode;
  TriSurf_t *oldHead;

  leafNode = GridTree_FindNode(ts->mins, ts->maxs);

  Assert(leafNode, s_assertDisable_GridTree_Insert);
  ts->gridPrev = 0;
  oldHead = leafNode->surfListHead;
  ts->gridNext = oldHead;
  if ( oldHead )
    oldHead->gridPrev = ts;
  leafNode->surfListHead = ts;
  return oldHead;
}

int g_mergeCallNum = 0;  /* set in MergeVisGroupList, read in GridTree_Remove */

/*
================
GridTree_Remove

Removes a surface from the grid tree by unlinking it
from its leaf node's doubly-linked list.
================
*/
void GridTree_Remove(TriSurf_t *ts)
{
  GridTreeNode_t *leafNode;

  if ( gridTreeRoot )
  {
    if ( ts->gridNext )
      ts->gridNext->gridPrev = ts->gridPrev;
    if ( ts->gridPrev )
    {
      ts->gridPrev->gridNext = ts->gridNext;
      ts->gridPrev = 0;
      ts->gridNext = 0;
    }
    else
    {
      leafNode = GridTree_FindNode(ts->mins, ts->maxs);
      Assert(leafNode, s_assertDisable_GridTree_Remove);
      if ( leafNode->surfListHead == ts )
        leafNode->surfListHead = ts->gridNext;
      ts->gridPrev = 0;
      ts->gridNext = 0;
    }
  }
}

/*
================
GridTree_ForEach_r

Recursively traverses the grid tree, calling the callback
on every surface whose AABB intersects the query bounds.
================
*/
void GridTree_ForEach_r( float *queryMins, float *queryMaxs, int nodeIdx, int depth, void (*callback)(TriSurf_t *) )
{
  TriSurf_t *ts;
  int splitAxis;

  while ( 1 )
  {
    GridTreeNode_t *node = &gridTreeRoot[nodeIdx];

    /* call back for each surface in this node that intersects the query
       (save next before callback — callback may unlink the surface) */
    { TriSurf_t *next;
    for ( ts = node->surfListHead; ts; ts = next )
    {
      next = ts->gridNext;
      if ( BoundsIntersect(ts->mins, ts->maxs, queryMins, queryMaxs) )
        callback(ts);
    }
    }

    if ( depth == GRID_TREE_MAX_DEPTH )
      break;

    /* recurse into children that overlap the query */
    splitAxis = g_gridTreeSplitAxis[depth];
    if ( queryMins[splitAxis] <= node->splitValue )
      GridTree_ForEach_r(queryMins, queryMaxs, 2 * nodeIdx + 2, depth + 1, callback);
    if ( queryMaxs[splitAxis] < node->splitValue )
      break;

    /* tail-recurse into the other child */
    ++depth;
    nodeIdx = 2 * nodeIdx + 1;
  }
}

/*
================
GridTree_ForEach

Iterates all grid tree surfaces whose AABB intersects
the query bounds, calling the callback on each.
================
*/
void GridTree_ForEach(float *queryMins, float *queryMaxs, void (*callback)(TriSurf_t *))
{
  GridTree_ForEach_r(queryMins, queryMaxs, 0, 0, callback);
}

/*
================
GridTree_CountIntersecting_r

Recursively counts surfaces intersecting the query bounds,
with early-out when count exceeds maxCount.
================
*/
int GridTree_CountIntersecting_r(float *queryMins, float *queryMaxs, int nodeIdx, int depth, int maxCount)
{
  GridTreeNode_t *node = &gridTreeRoot[nodeIdx];
  TriSurf_t *ts;
  int count = 0, splitAxis;

  for ( ts = node->surfListHead; ts; ts = ts->gridNext )
  {
    if ( BoundsIntersect(ts->mins, ts->maxs, queryMins, queryMaxs) )
      if ( ++count > maxCount )
        return count;
  }

  if ( depth < GRID_TREE_MAX_DEPTH )
  {
    splitAxis = g_gridTreeSplitAxis[depth];
    if ( queryMins[splitAxis] <= node->splitValue )
    {
      count += GridTree_CountIntersecting_r(queryMins, queryMaxs, 2 * nodeIdx + 2, depth + 1, maxCount - count);
      if ( count > maxCount )
        return count;
    }
    if ( queryMaxs[splitAxis] >= node->splitValue )
      count += GridTree_CountIntersecting_r(queryMins, queryMaxs, 2 * nodeIdx + 1, depth + 1, maxCount - count);
  }
  return count;
}

/*
================
GridTree_CountIntersecting

Counts grid tree surfaces whose AABB intersects the query
bounds, with early-out when count exceeds maxCount.
================
*/
int GridTree_CountIntersecting(float *queryMins, float *queryMaxs, int maxCount)
{
  return GridTree_CountIntersecting_r(queryMins, queryMaxs, 0, 0, maxCount);
}

/*
================
GridTree_Init

Allocates the grid tree root array. The array holds
2^21 - 1 nodes (full binary tree of depth 20), each
node being 8 bytes (float splitValue + int surfListHead).
================
*/
GridTreeNode_t *GridTree_Init(void)
{
  Assert(!gridTreeRoot, s_assertDisable_GridTree_Init);
  /* full binary tree of depth 20: 2^21 - 1 = 2097151 nodes, 8 bytes each */
  gridTreeRoot = malloc(sizeof(GridTreeNode_t) * GRID_TREE_MAX_NODES);
  return gridTreeRoot;
}

/*
================
GridTree_Shutdown

Frees the grid tree root array and resets the pointer.
================
*/
void GridTree_Shutdown(void)
{
  Assert(gridTreeRoot, s_assertDisable_GridTree_Shutdown);
  free(gridTreeRoot);
  gridTreeRoot = 0;
}

/*
================
SetGridDivisionPoints_r

Recursively computes and stores split plane distances
for each grid tree node by bisecting the bounds along
the node's split axis.
================
*/
float *SetGridDivisionPoints_r(int nodeIdx, float *mins, float *maxs, int depth)
{
  int splitAxis;
  float childMins[3], childMaxs[3];

  splitAxis = g_gridTreeSplitAxis[depth];
  gridTreeRoot[nodeIdx].surfListHead = NULL;
  gridTreeRoot[nodeIdx].splitValue = MIDF(mins[splitAxis], maxs[splitAxis]);

  if ( depth != GRID_TREE_MAX_DEPTH )
  {
    VectorCopy(mins, childMins);
    VectorCopy(maxs, childMaxs);
    childMins[splitAxis] = gridTreeRoot[nodeIdx].splitValue;
    childMaxs[splitAxis] = gridTreeRoot[nodeIdx].splitValue;
    SetGridDivisionPoints_r(2 * nodeIdx + 2, mins, childMaxs, depth + 1);
    return SetGridDivisionPoints_r(2 * nodeIdx + 1, childMins, maxs, depth + 1);
  }
  return mins;
}

/*
================
SetGridDivisionPoints

Sets grid tree split distances from the given world bounds
by recursively bisecting along alternating axes.
================
*/
float *SetGridDivisionPoints(float *mins, float *maxs)
{
  Assert(gridTreeRoot, s_assertDisable_SetGridDivisionPoints);
  return SetGridDivisionPoints_r(0, mins, maxs, 0);
}

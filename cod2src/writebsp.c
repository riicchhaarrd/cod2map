/*
writebsp.c — BSP file writing and emission

Reconstructed from cod2map.exe by Rose.
*/

#include "cod2map.h"

char s_assertDisable_EmitBrushes;
char s_assertDisable_EmitBrushes;
char s_assertDisable_EndBSPFile;
char s_assertDisable_RemapBrushSidePlanes;
char s_assertDisable_RemapBrushSidePlanes;
char s_assertDisable_RemapBrushSidePlanes;
char s_assertDisable_RemapNodePlanes;
char s_assertDisable_RemapNodePlanes;


/*
================
EmitMaterial

Emits a BSP material/shader entry. Searches existing materials for a match
by name and surface flags; if not found, adds a new entry to the material table.
================
*/
int EmitMaterial(const char *materialName, int surfaceFlags)
{
  int i;

  if ( !materialName )
    materialName = "noshader";

  /* search for existing material with matching name and flags */
  for ( i = 0; i < numBSPMaterials; i++ )
  {
    if ( !Q_stricmp(materialName, bspMaterials[i].material) && surfaceFlags == bspMaterials[i].contentFlags )
      return i;
  }

  /* not found - add new material entry */
  if ( i == MAX_MAP_MATERIALS )
    Com_Error("MAX_MAP_MATERIALS");
  ++numBSPMaterials;
  strcpy(bspMaterials[i].material, materialName);
  _strlwr(bspMaterials[i].material);
  bspMaterials[i].surfaceFlags = LoadMaterial(materialName)->surfaceFlags;
  bspMaterials[i].contentFlags = surfaceFlags;
  return i;
}

/*
================
RemapNodePlanes

Remaps BSP node plane indices through the planeMap lookup table.
After unused planes are stripped, node plane numbers must be updated
to reference the new compacted plane array.
================
*/
int RemapNodePlanes(int *planeMap)
{
  int mappedPlane;
  int i;

  for ( i = 0; i < numBSPNodes; i++ )
  {
    if ( bspNodes[i].planeNum < 0 )
      continue;

    Assert(bspNodes[i].planeNum >= 0 && bspNodes[i].planeNum < g_numMapPlanes, s_assertDisable_RemapNodePlanes);
    mappedPlane = planeMap[bspNodes[i].planeNum];
    Assert(mappedPlane >= 0 && mappedPlane < numBSPPlanes, s_assertDisable_RemapNodePlanes);
    bspNodes[i].planeNum = mappedPlane;
  }
  return numBSPNodes;
}

/*
================
RemapBrushSidePlanes

Remaps brush side plane indices through the planeMap lookup table.
After unused planes are stripped, brush side plane numbers must be
updated to reference the new compacted plane array.
================
*/
void RemapBrushSidePlanes( int *planeMap )
{
  BspBrushSide_t *side;
  int i, j, mapped;

  Assert( planeMap, s_assertDisable_RemapBrushSidePlanes );

  side = bspBrushSidesData;

  for ( i = 0; i < numBSPBrushes; i++ )
  {
    /* skip first 6 axial sides (store float dist, not plane index) */
    side += BRUSH_AXIAL_SIDES;

    /* remap plane indices for non-axial sides */
    for ( j = BRUSH_AXIAL_SIDES; j < bspBrushes[i].numSides; j++, side++ )
    {
      Assert( side->planeNum >= 0 && side->planeNum < g_numMapPlanes, s_assertDisable_RemapBrushSidePlanes );
      mapped = planeMap[side->planeNum];
      Assert( mapped >= 0 && mapped < numBSPPlanes, s_assertDisable_RemapBrushSidePlanes );
      side->planeNum = mapped;
    }
  }
}

/*
================
EmitBrushes

Writes the brush list to BSP brush and brushside arrays.
For each brush, emits its material, side count, and per-side plane
distances with material indices. Collision sides (first 6) store
axial plane distances; additional sides store plane indices.
================
*/
void EmitBrushes( double a1, Brush_t *brushes )
{
  Brush_t *b;
  BrushSide_t *side;
  BspBrush_t *db;
  int firstSide, sideIdx, j;
  float *planeNormal;

  /* walk the linked list of brushes */
  for ( b = brushes; b; b = b->next )
  {
    /* skip detail brushes and brushes whose original is detail */
    if ( b->detail || b->original->detail )
      continue;

    Assert( b->numCollisionSides >= BRUSH_AXIAL_SIDES, s_assertDisable_EmitBrushes );

    if ( numBSPBrushes == MAX_MAP_BRUSHES )
      Com_Error( "MAX_MAP_BRUSHES" );

    /* record BSP brush index and emit brush header */
    b->bspBrushIdx = numBSPBrushes;
    db = &bspBrushes[numBSPBrushes++];
    db->numSides = 0;
    db->shaderNum = EmitMaterial( b->contentShader, b->contentFlags );

    firstSide = numBSPBrushSides;

    /* emit each collision side */
    for ( j = 0; j < b->numCollisionSides; j++ )
    {
      sideIdx = numBSPBrushSides;
      if ( sideIdx == MAX_MAP_BRUSHSIDES )
        Com_Error( "MAX_MAP_BRUSHSIDES " );

      side = &b->collisionSides[j];
      planeNormal = MAP_PLANE(side->planenum)->normal;
      db->numSides++;
      numBSPBrushSides = sideIdx + 1;

      if ( sideIdx - firstSide >= BRUSH_AXIAL_SIDES )
      {
        /* non-axial side: store plane index */
        MAP_PLANE(side->planenum)->flags = 1;
        bspBrushSidesData[sideIdx].planeNum = side->planenum;
      }
      else if ( ((sideIdx - firstSide) & 1) == 0 )
      {
        /* even axial side: store negated plane distance */
        bspBrushSidesData[sideIdx].dist = -planeNormal[3];
      }
      else
      {
        /* odd axial side: store plane distance */
        bspBrushSidesData[sideIdx].dist = planeNormal[3];
      }

      bspBrushSidesData[sideIdx].shaderNum = EmitMaterial( side->shaderInfo->name, b->contentFlags );
    }

    Assert( db->numSides >= BRUSH_AXIAL_SIDES, s_assertDisable_EmitBrushes );
  }
}

/*
================
BeginModel

Sets up a new BSP brush model. Computes bounding box from entity
brushes and patches, then records the model's first triSoup,
first collision AABB, and first brush indices.
================
*/
int BeginModel(void)
{
  BspModel_t *mod;
  Entity_t *ent;
  Brush_t *br;
  Patch_t *p;
  int i;
  vec3_t mins, maxs;

  if ( numBSPModels == MAX_MAP_MODELS )
    Com_Error("MAX_MAP_MODELS");

  mod = &bspModels[numBSPModels];
  ent = &g_entities[g_currentEntityIndex];
  ClearBounds(mins, maxs);

  /* bound the brushes */
  for ( br = ent->brushes; br; br = br->next )
  {
    if ( br->numSides )
    {
      AddPointToBounds(br->eMins, mins, maxs);
      AddPointToBounds(br->eMaxs, mins, maxs);
    }
  }

  /* bound the patches */
  for ( p = ent->patches; p; p = p->next )
  {
    for ( i = 0; i < p->width * p->height; i++ )
      AddPointToBounds(p->vertexData[i].pos, mins, maxs);
  }

  /* store bounds and offsets into model record */
  VectorCopy(mins, mod->mins);
  VectorCopy(maxs, mod->maxs);
  mod->firstTriSoup = numBSPTriSoups;
  mod->firstAABB = numBSPCollisionAABBs;
  mod->firstBrush = numBSPBrushes;
  return numBSPBrushes;
}

/*
================
EmitPlanes

Emits used map planes to the BSP plane array, skipping unused planes.
Builds a planeMap lookup table to remap old plane indices to new
compacted indices, then remaps all node, brush side, leaf, and
collision node plane references.
================
*/
void EmitPlanes(void)
{
  int *planeMap;
  int i;

  planeMap = malloc(sizeof(int) * g_numMapPlanes);

  for ( i = 0; i < g_numMapPlanes; i++ )
  {
    if ( mapplanes[i].flags )
    {
      planeMap[i] = numBSPPlanes;
      VectorCopy(mapplanes[i].normal, bspPlanes[numBSPPlanes].normal);
      bspPlanes[numBSPPlanes].dist = mapplanes[i].dist;
      numBSPPlanes++;
    }
    else
    {
      planeMap[i] = -1;
    }
  }

  RemapNodePlanes(planeMap);
  RemapBrushSidePlanes(planeMap);
  CreateLeafNode(planeMap);
  CreateNode(planeMap);
  free(planeMap);
}

/*
================
EmitCollisionAABBs_r

Recursively emits collision AABB records from the collision tree.
Each AABB stores center and half-extents computed from node bounds.
Child links reference either sub-AABBs (internal nodes) or brush
indices (leaf nodes) with material indices.
================
*/
int EmitCollisionAABBs_r( CmCollideBox_t *nodeList )
{
  CmCollideBox_t *node;
  CollisionAabbTree_t *aabb;
  int startIdx, aabbIdx, count;

  if ( !nodeList )
    return 0;

  startIdx = numBSPCollisionAABBs;
  aabbIdx = startIdx;
  count = 0;

  /* first pass: emit AABB records for all nodes in the linked list */
  for ( node = nodeList; node; node = node->next )
  {
    if ( aabbIdx == MAX_MAP_COLLISION_AABBS )
      Com_Error( "MAX_MAP_COLLISIONAABBS (%i) exceeded", MAX_MAP_COLLISION_AABBS );

    aabb = &bspCollisionAABBs[aabbIdx];

    /* center = (mins + maxs) * 0.5, halfSize = maxs - center */
    aabb->origin[0] = MIDF(node->mins[0], node->maxs[0]);
    aabb->origin[1] = MIDF(node->mins[1], node->maxs[1]);
    aabb->origin[2] = MIDF(node->mins[2], node->maxs[2]);
    aabb->halfSize[0] = node->maxs[0] - aabb->origin[0];
    aabb->halfSize[1] = node->maxs[1] - aabb->origin[1];
    aabb->halfSize[2] = node->maxs[2] - aabb->origin[2];

    aabbIdx++;
    numBSPCollisionAABBs = aabbIdx;
    count++;
  }

  /* second pass: fill in child links for each AABB */
  aabb = &bspCollisionAABBs[startIdx];
  for ( node = nodeList; node; node = node->next, aabb++ )
  {
    if ( node->child )
    {
      /* internal node: recurse into children */
      aabb->u.firstChildIndex = numBSPCollisionAABBs;
      aabb->childCount = EmitCollisionAABBs_r( node->child );
      aabb->materialIndex = bspCollisionAABBs[aabb->u.firstChildIndex].materialIndex;
    }
    else
    {
      /* leaf node: reference brush partition and material */
      CmPartition_t *part = node->partition;
      DrawSurf_t *ds = part->triArray->drawSurf;
      aabb->u.partitionIndex = part->emitIndex;
      aabb->childCount = 0;
      aabb->materialIndex = EmitMaterial( ds->shaderInfo->name, ds->surfaceFlags );
    }
  }

  return count;
}

/*
================
EmitLeaf

Emits a leaf node to the BSP file. Stores the leaf's cluster, area,
terrain type, and brush references. For non-opaque leaves, also emits
collision AABBs from the leaf's collision tree.
================
*/
int EmitLeaf( Node_t *node )
{
  BspLeaf_disk_t *leaf_p;
  Brush_t *b;
  int leafIdx;

  if ( numBSPLeafs >= MAX_MAP_LEAFS )
    Com_Error( "MAX_MAP_LEAFS" );

  leafIdx = numBSPLeafs++;
  leaf_p = &bspLeafs[leafIdx];
  leaf_p->cluster = node->cluster;
  leaf_p->area = node->area;
  leaf_p->cellnum = node->cellnum;
  leaf_p->firstLeafBrush = numBSPLeafBrushes;

  /* emit leaf brushes — skip detail brushes */
  for ( b = node->leafBrushes; b; b = b->next )
  {
    if ( b->detail || b->original->detail )
      continue;
    if ( numBSPLeafBrushes >= MAX_MAP_LEAFBRUSHES )
      Com_Error( "MAX_MAP_LEAFBRUSHES" );
    bspLeafBrushes[numBSPLeafBrushes++] = b->original->bspBrushIdx;
  }
  leaf_p->numLeafBrushes = numBSPLeafBrushes - leaf_p->firstLeafBrush;

  /* emit collision AABBs for non-opaque leaves */
  if ( node->opaque )
  {
    leaf_p->firstCollisionAABB = 0;
    leaf_p->numCollisionAABBs = 0;
    return 0;
  }

  leaf_p->firstCollisionAABB = numBSPCollisionAABBs;
  leaf_p->numCollisionAABBs = EmitCollisionAABBs_r( (CmCollideBox_t *)node->collisionAABBs );
  return leaf_p->numCollisionAABBs;
}

/*
================
EmitDrawNode_r

Recursively emits BSP nodes. For leaf nodes, calls EmitLeaf and returns
the negative leaf index. For internal nodes, stores the splitting plane,
bounding box, and recursively emits both children.
================
*/
int EmitDrawNode_r( Node_t *node )
{
  BspNode_disk_t *n;
  int nodeIdx, i;

  /* leaf node */
  if ( node->planenum == PLANENUM_LEAF )
  {
    EmitLeaf( node );
    return -numBSPLeafs;
  }

  /* emit internal node */
  if ( numBSPNodes == MAX_MAP_NODES )
    Com_Error( "MAX_MAP_NODES" );

  nodeIdx = numBSPNodes++;
  n = &bspNodes[nodeIdx];

  /* bounding box — float to int truncation */
  n->mins[0] = (int)node->mins[0];
  n->mins[1] = (int)node->mins[1];
  n->mins[2] = (int)node->mins[2];
  n->maxs[0] = (int)node->maxs[0];
  n->maxs[1] = (int)node->maxs[1];
  n->maxs[2] = (int)node->maxs[2];

  if ( node->planenum & 1 )
    Com_Error( "WriteDrawNodes_r: odd planenum" );
  n->planeNum = node->planenum;
  mapplanes[node->planenum].flags = 1;

  /* recursively output children */
  for ( i = 0; i < 2; i++ )
  {
    if ( node->children[i]->planenum == PLANENUM_LEAF )
    {
      n->children[i] = -1 - numBSPLeafs;
      EmitLeaf( node->children[i] );
    }
    else
    {
      n->children[i] = numBSPNodes;
      EmitDrawNode_r( node->children[i] );
    }
  }

  return nodeIdx;
}

/*
================
EndBSPFile

Finishes the BSP file and writes it to disk. Emits planes, unparses
entities, reorders draw verts, then writes the final BSP file.
================
*/
int EndBSPFile(void)
{
  char *ext;
  char path[MAX_OS_PATH];

  EmitPlanes();
  UnparseEntities();
  Tris_ReorderDrawVerts();
  ext = GetBSPFileExtension();
  sprintf(path, "%s%s", g_outputBasePath, ext);
  Com_Printf("Writing %s\n", path);
  Assert(g_targetPlatform, s_assertDisable_EndBSPFile);
  return WriteBSPFile(path, g_targetPlatform->bigEndian);
}

/*
================
EndModel

Finishes processing a BSP model. Emits brushes and the BSP node tree
for the current entity, then updates the model record with surface
counts, leaf cluster reference, and brush counts.
================
*/
int EndModel(Node_t *headnode)
{
  BspModel_t *mod;
  int firstLeaf;

  Com_DPrintf("--- EndModel ---\n");

  EmitBrushes(0.0, g_entities[g_currentEntityIndex].brushes);

  firstLeaf = numBSPLeafs;
  mod = &bspModels[numBSPModels];

  EmitDrawNode_r(headnode);

  mod->numTriSoups = numBSPTriSoups - mod->firstTriSoup;
  mod->numAABBs = bspLeafs[firstLeaf].numCollisionAABBs;
  mod->numBrushes = numBSPBrushes - mod->firstBrush;
  numBSPModels++;

  return numBSPModels;
}

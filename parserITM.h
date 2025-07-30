#ifndef PARSER_ITM_H
#define PARSER_ITM_H

enum ENTITY_TYPE
{
  ENTITY,          //= 0x01 << 0,
  ENTITY_GROUP,    //= 0x01 << 1,
  ENTITY_3D,       //= 0x01 << 2,
  ENTITY_CAMERA,   //= 0x01 << 3,
  ENTITY_GEOMETRY, //= 0x01 << 4,
  ENTITY_RIVER,    //= 0x01 << 5,
  ENTITY_TERRAIN,  //= 0x01 << 6,
  ENTITY_SKYBOX,   //= 0x01 << 7,
  ENTITY_EFFECT,   //= 0x01 << 8,

  ENTITY_CREATURE,     //= 0x01 << 9,
  ENTITY_TRIGGER,      //= 0x01 << 10,
  ENTITY_QUESTZONE,    //= 0x01 << 11,
  ENTITY_STATICOBJECT, //= 0x01 << 12,
  ENTITY_WAYPOINT,     //= 0x01 << 13,
  ENTITY_GRAVEYARD,    //= 0x01 << 14,

  ENTITY_HOUSE, //= 0x01 << 15
  ENTITY_HOUSE_INSIDE,

  ENTITY_ALL = 0xFFFFFFFF
};

// 24 bytes
struct ITMFileHeader
{
  char signature[4];    // 4 bytes  file signature – 'AITM' for .itm file
  unsigned int version; // 4 bytes  format version
  int gridX;            // 4 bytes  X-axis grid position
  int gridZ;            // 4 bytes  Z-axis grid position
  int entityCount;      // 4 bytes  number of entities on a tile (entities may be repeated and have the same model file)
  int fileCount;        // 4 bytes  number of .bdae file names stored in the string data section
};

// 64 bytes, repeated entityCount times
struct EntityInfo
{
  unsigned int type;     // 4 bytes  entity type
  unsigned int ID;       // 4 bytes  number in the entity data section
  unsigned int parentID; // 4 bytes  parent model reference (if true, used for memory optimization (?))
  int fileNameIdx;       // 4 bytes  index of .bdae file name in the string data section
  VEC3 relativePos;      // 12 bytes position relative to the tile origin
  Quaternion rotation;   // 16 bytes orientation in 3D space; quaternion avoids gimbal lock and enables smooth interpolation
  VEC3 scale;            // 12 bytes scaling factor along the X, Y, and Z axes
  int unknown1;          // 4 bytes  (?)
  int unknown2;          // 4 bytes  (?)
};

class TileTerrain; // forward declaration

inline void loadEntity(CZipResReader *physicsArchive, const char *fname, const EntityInfo &entityInfo, TileTerrain *tile, const VEC3 &tileOff);

//! Processes a single .itm file, retrieving the tile's game object resource names, and loading all physics for base entities for each of them.
inline void loadTileEntities(CZipResReader *itemsArchive, CZipResReader *physicsArchive, int gridX, int gridZ, TileTerrain *tile)
{
  // 2.7. Load .itm file into memory.
  char tmpName[256];
  sprintf(tmpName, "items/%04d_%04d.itm", gridX, gridZ); // path inside the .itm archive
  IReadResFile *itmFile = itemsArchive->openFile(tmpName);

  if (!itmFile)
    return;

  itmFile->seek(0);
  int fileSize = (int)itmFile->getSize();
  unsigned char *buffer = new unsigned char[fileSize];
  itmFile->read(buffer, fileSize);
  itmFile->drop();

  /* 2.8. Parse .itm file header, entity info section and namelist section. Retrieve:
        From header:
          – number of game objects in the tile (each object may consist of several entities)
          – total number of entities
        From entity info section:
          – info about every presented entity, such as.. (used in LoadEntity())
        From namelist section:
          – names of game object resource files */

  ITMFileHeader *header = (ITMFileHeader *)buffer;
  EntityInfo *entityData = (EntityInfo *)(buffer + sizeof(ITMFileHeader));
  int *stringData = (int *)(buffer + sizeof(ITMFileHeader) + sizeof(EntityInfo) * header->entityCount + 4);

  /* 2.9. Load physics for every game object in the tile.
          For every base entity within a game object, execution sequence:
          loadEntity() → LoadModelPhysics() → .. */

  // loop through each game object
  for (int i = 0; i < header->fileCount; i++)
  {
    const char *fileName = (const char *)(stringData + *(stringData - 1)) + ((i > 0) ? stringData[i - 1] : 0); // retrieve the corresponding resource file name

    // loop through each entity, filter those that are part of the i-th game object by index, and load them
    for (int j = 0; j < header->entityCount; j++)
    {
      if (entityData[j].fileNameIdx == i)
        loadEntity(physicsArchive, fileName, entityData[j], tile, VEC3(64.0f * header->gridX, 0, 64.0f * header->gridZ)); // load physics for an entity
    }
  }
}

//! Loads physics geometry for a single base entity.
inline void loadEntity(CZipResReader *physicsArchive, const char *fname, const EntityInfo &entityInfo, TileTerrain *tile, const VEC3 &tileOff)
{
  switch (entityInfo.type)
  {
  case ENTITY_3D:
  case ENTITY_HOUSE:
  case ENTITY_EFFECT:
  {
    // load house entity from the physics archive into the PhysicsGeom object (?)
    Physics *physicsGeom = Physics::load(physicsArchive, fname, entityInfo.type == ENTITY_HOUSE); // save into physicsGeom

    if (physicsGeom)
    {
      // build a model matrix that transforms the entity from local to world space coordinates
      MTX4 absTransform;
      entityInfo.rotation.getMatrix(absTransform);

      if (entityInfo.scale != VEC3(1.f, 1.f, 1.f))
        absTransform.postScale(entityInfo.scale);

      absTransform.setTranslation(entityInfo.relativePos + tileOff);

      physicsGeom->SetSerilParentTransform(absTransform); // apply local2world transformation to entity's geometry and recursively to all connected parts of the parent game object, also updating its bounding box

      tile->BBox.addInternalBox(physicsGeom->m_groupAABB); // adjust tile's bounding box to include entity's geometry
      tile->physicsGeometry.push_back(physicsGeom);        // add a new physics geometry to TileTerrain object

      // [TODO] implement geometry
      // CPhysics::instance()->RegisterGeom(physicsGeom, &(tile->m_pGeomList));
    }

    // [TODO load .bdae models]
    // Camera camera;
    // Model model(camera);
    // model.loadTerrainEntity(fname);

    // tile->models.push_back(&model);

    break;
  }

  default:
    std::cout << "[Warning] Unhandled entity type: " << entityInfo.type << std::endl;
    break;
  }
}

#endif

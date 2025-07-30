#ifndef PARSER_ITM_H
#define PARSER_ITM_H

class TileTerrain; // forward declaration to use this class in function signatures

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

struct ItemFileHeader
{
  char Signature[4];
  unsigned int Version;
  int BaseX;
  int BaseZ;
  int EntityCount;
  int FileCount;
};

struct EntityInfo
{
  unsigned int Type;
  unsigned int ID;
  unsigned int ParentID;

  int FileNameIdx;

  VEC3 RelativePos;
  Quaternion Rotation;
  VEC3 Scale;

  int unknown1;
  int unknown2;
};

//! Loads physics geometry for a single base entity.
inline void loadEntity(const char *fileName, const EntityInfo &entityInfo, unsigned int id, TileTerrain *tile, const VEC3 &tileOff)
{
  switch (entityInfo.Type)
  {
  case ENTITY_3D:
  case ENTITY_HOUSE:
  case ENTITY_EFFECT:
  {
    // CPhysicsGeom *physicsGeom = NULL;

    // // load house entity from the physics archive into the PhysicsGeom object (?)
    // CPhysics::LoadModelPhysics(fileName, physicsGeom, entityInfo.Type == ENTITY_HOUSE);

    // if (physicsGeom)
    // {
    //   // build a model matrix that transforms the entity from local to world space coordinates
    //   MTX4 absTransform;
    //   entityInfo.Rotation.getMatrix(absTransform);

    //   if (entityInfo.Scale != VEC3(1.f, 1.f, 1.f))
    //     absTransform.postScale(entityInfo.Scale);

    //   absTransform.setTranslation(entityInfo.RelativePos + tileOff);

    //   physicsGeom->SetSerilParentTransform(absTransform); // apply local2world transformation to entity's geometry and recursively to all connected parts of the parent game object, also updating its bounding box

    //   tile->m_BBox.addInternalBox(physicsGeom->m_groupAABB); // adjust tile's bounding box to include entity's geometry
    //   tile->m_pGeomList.push_back(physicsGeom);              // add new physics geometry to TileTerrain object

    //   // CPhysics::instance()->RegisterGeom(physicsGeom, &(tile->m_pGeomList));
    // }

    // Bdae::LoadModelTexture(fileName);

    break;
  }

  default:
    break;
  }
}

//! Processes a single .itm file, retrieving the tile's game object resource names, and loading all physics for base entities for each of them
inline void loadTileEntities(CZipResReader *archive, int gridX, int gridZ, TileTerrain *tile)
{
  // 2.7. Load .itm file into memory.
  char tmpName[256];
  sprintf(tmpName, "items/%04d_%04d.itm", gridX, gridZ); // path inside the .itm archive
  IReadResFile *itmFile = archive->openFile(tmpName);

  if (!itmFile)
    return;

  std::cout << "\nTile (" << gridX << ", " << gridZ << "): " << tmpName << std::endl;

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

  ItemFileHeader *header = (ItemFileHeader *)buffer;
  unsigned int loadingOffset = sizeof(ItemFileHeader);
  EntityInfo *pInfo = (EntityInfo *)(buffer + loadingOffset);
  int *pNamelist = (int *)(buffer + loadingOffset + sizeof(EntityInfo) * header->EntityCount + 4);

  unsigned int loadingId = 0;

  /* 2.9. Load physics for every game object in the tile.
          For every base entity within a game object, execution sequence:
          loadEntity() → LoadModelPhysics() → .. */

  // loop through each game object
  for (int i = 0; i < header->FileCount; i++)
  {
    const char *fileName = (const char *)(pNamelist + *(pNamelist - 1)) + ((i > 0) ? pNamelist[i - 1] : 0); // retrieve the corresponding resource file name
    std::cout << "Object name: " << fileName << std::endl;

    // loop through each entity, filter those that are part of the i - th game object by index, and load them for (int e = 0; e < header->EntityCount; ++e)
    for (int e = 0; e < header->EntityCount; ++e)
    {
      if (pInfo[e].FileNameIdx == i)
        loadEntity(fileName, pInfo[e], loadingId++, tile, VEC3(64.0f * header->BaseX, 0, 64.0f * header->BaseZ)); // load physics for an entity
    }
  }
}

#endif

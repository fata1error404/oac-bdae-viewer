#ifndef PARSER_ITM_H
#define PARSER_ITM_H

enum ENTITY_TYPE
{
	ENTITY,			 //= 0x01 << 0,
	ENTITY_GROUP,	 //= 0x01 << 1,
	ENTITY_3D,		 //= 0x01 << 2,
	ENTITY_CAMERA,	 //= 0x01 << 3,
	ENTITY_GEOMETRY, //= 0x01 << 4,
	ENTITY_RIVER,	 //= 0x01 << 5,
	ENTITY_TERRAIN,	 //= 0x01 << 6,
	ENTITY_SKYBOX,	 //= 0x01 << 7,
	ENTITY_EFFECT,	 //= 0x01 << 8,

	ENTITY_CREATURE,	 //= 0x01 << 9,
	ENTITY_TRIGGER,		 //= 0x01 << 10,
	ENTITY_QUESTZONE,	 //= 0x01 << 11,
	ENTITY_STATICOBJECT, //= 0x01 << 12,
	ENTITY_WAYPOINT,	 //= 0x01 << 13,
	ENTITY_GRAVEYARD,	 //= 0x01 << 14,

	ENTITY_HOUSE, //= 0x01 << 15
	ENTITY_HOUSE_INSIDE,

	ENTITY_ALL = 0xFFFFFFFF
};

// 24 bytes
struct ITMFileHeader
{
	char signature[4];	  // 4 bytes  file signature – 'AITM' for .itm file
	unsigned int version; // 4 bytes  format version
	int gridX;			  // 4 bytes  X-axis grid position
	int gridZ;			  // 4 bytes  Z-axis grid position
	int entityCount;	  // 4 bytes  number of entities on a tile (entities may be repeated and have the same model file)
	int fileCount;		  // 4 bytes  number of .bdae file names stored in the string data section
};

// 64 bytes, repeated entityCount times
struct EntityInfo
{
	unsigned int type;	   // 4 bytes  entity type
	unsigned int ID;	   // 4 bytes  number in the entity data section
	unsigned int parentID; // 4 bytes  parent model reference (if true, used for memory optimization (?))
	int fileNameIdx;	   // 4 bytes  index of .bdae file name in the string data section
	VEC3 relativePos;	   // 12 bytes position relative to the tile origin
	Quaternion rotation;   // 16 bytes orientation in 3D space; quaternion avoids gimbal lock and enables smooth interpolation
	VEC3 scale;			   // 12 bytes scaling factor along the X, Y, and Z axes

#ifndef BETA_GAME_VERSION
	int unknown1; // 4 bytes  (?)
	int unknown2; // 4 bytes  (?)
#endif
};

class TileTerrain; // forward declaration

inline void loadEntity(CZipResReader *physicsArchive, const char *fname, const EntityInfo &entityInfo, TileTerrain *tile, const VEC3 &tileOff, Terrain &terrain);

//! Processes a single .itm file, retrieving the tile's game object resource names, and loading all physics for base entities for each of them.
inline void loadTileEntities(CZipResReader *itemsArchive, CZipResReader *physicsArchive, int gridX, int gridZ, TileTerrain *tile, Terrain &terrain)
{
	// 1. load .itm file into memory
	// ____________________

	char tmpName[256];

#ifdef BETA_GAME_VERSION
	std::string terrainName = std::filesystem::path(itemsArchive->getZipFileName()).stem().string();
	sprintf(tmpName, "world/%s/items/%04d_%04d.itm", terrainName.c_str(), gridX, gridZ); // path inside the .itm archive
#else
	sprintf(tmpName, "items/%04d_%04d.itm", gridX, gridZ);
#endif

	IReadResFile *itmFile = itemsArchive->openFile(tmpName);

	if (!itmFile)
		return;

	itmFile->seek(0);
	int fileSize = (int)itmFile->getSize();
	unsigned char *buffer = new unsigned char[fileSize];
	itmFile->read(buffer, fileSize);
	itmFile->drop();

	/* 2. parse .itm file header, entity info section and namelist section, retrieve:
		  From header:
			– number of game objects in the tile (each object may consist of several entities)
			– total number of entities
		  From entity info section:
			– info about every presented entity
		  From namelist section:
			– names of game object resource files (.bdae models)
			____________________ */

	ITMFileHeader *header = (ITMFileHeader *)buffer;
	EntityInfo *entityData = (EntityInfo *)(buffer + sizeof(ITMFileHeader));
	int *stringData = (int *)(buffer + sizeof(ITMFileHeader) + sizeof(EntityInfo) * header->entityCount + 4);

	/* 3. load physics and 3D model for every game object in the tile
			For every base entity within a game object, execution sequence:
			loadEntity() → Physics::load() → Model::load()..
			____________________ */

	// loop through each game object
	for (int i = 0; i < header->fileCount; i++)
	{
		const char *fileName = (const char *)(stringData + *(stringData - 1)) + ((i > 0) ? stringData[i - 1] : 0); // retrieve the corresponding resource file name

		// convert to lowercase
		std::string tmpName = std::string(fileName);
		for (char &c : tmpName)
			c = std::tolower(c);

		// std::cout << tmpName << std::endl;

		// loop through each entity, filter those that are part of the i-th game object by index, and load them
		for (int j = 0; j < header->entityCount; j++)
		{
			if (entityData[j].fileNameIdx == i)
				loadEntity(physicsArchive, fileName, entityData[j], tile, VEC3(64.0f * header->gridX, 0, 64.0f * header->gridZ), terrain);
		}
	}

	delete[] buffer;
}

//! Loads physics geometry and 3D model for a single base entity.
inline void loadEntity(CZipResReader *physicsArchive, const char *fname, const EntityInfo &entityInfo, TileTerrain *tile, const VEC3 &tileOff, Terrain &terrain)
{
	switch (entityInfo.type)
	{
	case ENTITY_3D:
	case ENTITY_HOUSE:
	case ENTITY_EFFECT:
	{
		// load .phy model
		Physics *physicsGeom = Physics::load(physicsArchive, fname, entityInfo.type == ENTITY_HOUSE);

		if (physicsGeom)
		{
			// build a model matrix that transforms the entity from local to world space coordinates
			MTX4 absTransform;
			entityInfo.rotation.getMatrix(absTransform);

			if (entityInfo.scale != VEC3(1.0f, 1.0f, 1.0f))
				absTransform.postScale(entityInfo.scale);

			absTransform.setTranslation(entityInfo.relativePos + tileOff);

			physicsGeom->SetSerilParentTransform(absTransform); // apply local2world transformation to entity's geometry and recursively to all connected parts of the parent game object, also updating its bounding box

			tile->BBox.addInternalBox(physicsGeom->m_groupAABB); // adjust tile's bounding box to include entity's geometry
			tile->physicsGeometry.push_back(physicsGeom);		 // add a new physics geometry to TileTerrain object

			// [TODO] implement geometry
			// CPhysics::instance()->RegisterGeom(physicsGeom, &(tile->m_pGeomList));

			// build OpenGL style model matrix: translate -> rotate -> scale
			glm::mat4 model = glm::mat4(1.0f);
			model = glm::translate(glm::mat4(1.0f), glm::vec3(entityInfo.relativePos.X + tileOff.X, entityInfo.relativePos.Y + tileOff.Y, entityInfo.relativePos.Z + tileOff.Z));
			model *= glm::mat4_cast(glm::quat(-entityInfo.rotation.W, entityInfo.rotation.X, entityInfo.rotation.Y, entityInfo.rotation.Z));
			model *= glm::scale(glm::mat4(1.0f), glm::vec3(entityInfo.scale.X, entityInfo.scale.Y, entityInfo.scale.Z));

			// load .bdae model
			Model *bdaeModel = new Model("shaders/model.vs", "shaders/model.fs");
			Sound unused(true);
			bdaeModel->load(fname, model, unused, true);

			if (bdaeModel->modelLoaded)
				tile->models.push_back(bdaeModel);

			terrain.modelCount++;
		}

		break;
	}

	default:
		std::cout << "[Warning] Unhandled entity type: " << entityInfo.type << std::endl;
		break;
	}
}

#endif

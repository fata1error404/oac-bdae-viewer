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

//! Processes a single .itm file, retrieving for each tile's game object its resource file name, object type and world space info, and then calling the loader.
inline void loadTileEntities(CZipResReader *itemsArchive, CZipResReader *physicsArchive, int gridX, int gridZ, TileTerrain *tile, Terrain &terrain)
{
	// 1. load .itm file into memory
	// ____________________

	char tmpName[256];
	sprintf(tmpName, "%04d_%04d.itm", gridX, gridZ);

	IReadResFile *itmFile = itemsArchive->openFile(tmpName);

	if (!itmFile)
		return;

	itmFile->seek(0);
	int fileSize = (int)itmFile->getSize();
	unsigned char *buffer = new unsigned char[fileSize];
	itmFile->read(buffer, fileSize);
	itmFile->drop();

	/* 2. parse file header, entity info section and namelist section, retrieve:
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

	// loop through each game object
	for (int i = 0; i < header->fileCount; i++)
	{
		const char *fileName = (const char *)(stringData + *(stringData - 1)) + ((i > 0) ? stringData[i - 1] : 0); // retrieve the corresponding resource file name

		// convert to lowercase
		std::string tmpName = std::string(fileName);
		for (char &c : tmpName)
			c = std::tolower(c);

		// skip '.beff' effect files ([TODO] load them as well)
		if (tmpName.size() >= 5 && tmpName.compare(tmpName.size() - 5, 5, ".beff") == 0)
			continue;

		// std::cout << tmpName << std::endl;

		// loop through each entity, filter those that are part of the i-th game object by index, and load them
		for (int j = 0; j < header->entityCount; j++)
		{
			if (entityData[j].fileNameIdx == i)
				loadEntity(physicsArchive, tmpName.c_str(), entityData[j], tile, VEC3(64.0f * header->gridX, 0, 64.0f * header->gridZ), terrain);
		}
	}

	delete[] buffer;
}

//! Loads physics geometry model and 3D model for a single base entity.
inline void loadEntity(CZipResReader *physicsArchive, const char *fname, const EntityInfo &entityInfo, TileTerrain *tile, const VEC3 &tileOff, Terrain &terrain)
{
	switch (entityInfo.type)
	{
	case ENTITY_3D:
	case ENTITY_HOUSE:
	case ENTITY_EFFECT:
	{
		// 3. load .phy model
		// ____________________

		/*
			Physics *physicsGeom = Physics::load(physicsArchive, fname); // returned pointer is the first submesh (head node) in linked list

			if (physicsGeom)
			{
				// build a model matrix that transforms the entity from local to world space coordinates
				MTX4 model;
				entityInfo.rotation.getMatrix(model);

				if (entityInfo.scale != VEC3(1.0f, 1.0f, 1.0f))
					model.postScale(entityInfo.scale);

				model.setTranslation(entityInfo.relativePos + tileOff);

				physicsGeom->buildModelMatrix(model); // apply local2world transformation to entity's geometry

				tile->physicsGeometry.push_back(physicsGeom); // add a new physics geometry to TileTerrain object
			}
		*/

		break;
	}

	default:
		std::cout << "[Warning] Unhandled entity type: " << entityInfo.type << std::endl;
		break;
	}

	/* 4. load .bdae model
	   Models are stored in global terrain's cache for memory optimization.
	   This is handled via smart shared pointer (for each model) that does automatic reference counting and deletion. When model is found in cache, a copy of the shared pointer is created.
	 ____________________ */

	std::shared_ptr<Model> bdaeModel;

	auto it = bdaeModelCache.find(fname);

	if (it != bdaeModelCache.end()) // reuse cached model
		bdaeModel = it->second;
	else // not cached — create, load and insert to cache on success
	{
		std::shared_ptr<Model> newModel = std::make_shared<Model>("shaders/model.vs", "shaders/model.fs"); // create new model object and init shared pointer to handle its ownership
		Sound empty(true);
		newModel->load(fname, empty, true);

		if (newModel->modelLoaded)
		{
			bdaeModel = newModel;
			bdaeModelCache[fname] = bdaeModel; // add to global cache with filename as a key for quick lookup
		}
		else
			std::cout << "[Warning] Failed to load 3D model: " << fname << std::endl;
	}

	if (bdaeModel)
	{
		// build OpenGL style model matrix: translate -> rotate -> scale
		glm::mat4 model = glm::mat4(1.0f);
		model = glm::translate(glm::mat4(1.0f), glm::vec3(entityInfo.relativePos.X + tileOff.X, entityInfo.relativePos.Y + tileOff.Y, entityInfo.relativePos.Z + tileOff.Z));
		model *= glm::mat4_cast(glm::quat(-entityInfo.rotation.W, entityInfo.rotation.X, entityInfo.rotation.Y, entityInfo.rotation.Z));
		model *= glm::scale(glm::mat4(1.0f), glm::vec3(entityInfo.scale.X, entityInfo.scale.Y, entityInfo.scale.Z));

		tile->models.emplace_back(bdaeModel, model); // add to tile's data (entities may use the same .bdae model, but located / scaled differently in world space, so we store pairs shared pointer + model matrix)
		terrain.modelCount++;
	}
}

#endif

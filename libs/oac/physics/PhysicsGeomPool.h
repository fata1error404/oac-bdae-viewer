#ifndef PHYSICS_GEOM_POOL_H
#define PHYSICS_GEOM_POOL_H
#pragma once
//#include "PhysicsGeom.h"
class CPhysicsGeom;

struct PhysicsGeom_Item
{
	PhysicsGeom_Item(CPhysicsGeom* geom, const char * geomFileName)
		:  pGeom(geom)
		, life(0)
		, filename(geomFileName)
		, hashValue(0)
	{
	}
	bool operator < (const PhysicsGeom_Item& other) const {
		return hashValue < other.hashValue;
	};
	bool operator == (const PhysicsGeom_Item& other) const {
		return hashValue == other.hashValue;
	};
	CPhysicsGeom* pGeom;
	int			life;
	U32			hashValue;
	std::string	filename;
};

class PhysicsGeomPool
{
public:
	PhysicsGeomPool(void);
	~PhysicsGeomPool(void);
	CPhysicsGeom*  ClonePhysicsGeom(const char * physicsFileName);
	void CachePhysicsGeom(CPhysicsGeom *geom, const char * physicsFileName);
	
	int CleanAll(int minLife = 0);
	void IncreaseAllLife();
	


protected:
	std::vector<PhysicsGeom_Item>  m_geomVec;
	PhysicsGeom_Item			   m_testItem;
};

#endif
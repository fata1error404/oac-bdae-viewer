//#include "preHeader.h"
#include "PhysicsGeomPool.h"
#include "PhysicsGeom.h"
#include "Physics.h"
//#include "engine/3d/Model.h"

#ifdef _MMO_SERVER_
u32 hashString(const char* ptr)
{
	u32 hash = 0;
	for (; *ptr; ++ptr)
	{
		hash = (hash * 13) + (*ptr);
	}
	return hash;
}
#endif

PhysicsGeomPool::PhysicsGeomPool(void) 
: m_testItem(NULL, "")
{
	//PhysicsGeom* geom = gll_new PhysicsGeom();
	//m_testItem.entity = geom;
}

PhysicsGeomPool::~PhysicsGeomPool(void)
{
	CleanAll();
}

void PhysicsGeomPool::CachePhysicsGeom(CPhysicsGeom *geom, const char * modelName) 
{
#ifdef _DEBUG
	CPhysicsGeom*  testGeom =	ClonePhysicsGeom(modelName);
	GLL_ASSERT(testGeom==NULL);
#endif
	if (geom == NULL)
		return;

	PhysicsGeom_Item  curItem(geom->Clone(), modelName);
#ifdef _MMO_SERVER_
	curItem.hashValue = hashString(modelName);
#else
	curItem.hashValue = core::hashString(modelName);
#endif

	std::vector<PhysicsGeom_Item>::iterator it = std::upper_bound(m_geomVec.begin(), m_geomVec.end(), curItem);
#ifdef _DEBUG
	int nSize = m_geomVec.size();
	int index = it - m_geomVec.begin();
#endif

	if(it == m_geomVec.end())
		m_geomVec.push_back(curItem);
	else
	{

		m_geomVec.insert(it, curItem);
	}
}

CPhysicsGeom*  PhysicsGeomPool::ClonePhysicsGeom(const char * modelName)
{
#ifdef _MMO_SERVER_
	m_testItem.hashValue = hashString(modelName);
#else
	m_testItem.hashValue = core::hashString(modelName);
#endif

	std::vector<PhysicsGeom_Item>::iterator it = std::lower_bound(m_geomVec.begin(), m_geomVec.end(), m_testItem);
#ifdef _DEBUG
	int nSize = m_geomVec.size();
	int index = it - m_geomVec.begin();
#endif
	for( ;it != m_geomVec.end() ; ++it)
	{
		
		PhysicsGeom_Item& item = *it;
		if(m_testItem.hashValue < item.hashValue)
			break;

#ifdef _MMO_SERVER_
		if(STRICMP(item.filename.c_str(), modelName) == 0)
#else
		if(core::stricmp(item.filename.c_str(), modelName) == 0)
#endif
		{
			return item.pGeom->Clone();
		}
	}
	return NULL;
}

void PhysicsGeomPool::IncreaseAllLife()
{
#ifdef _DEBUG
	int nSize = m_geomVec.size();
#endif
	std::vector<PhysicsGeom_Item>::iterator it = m_geomVec.begin();
	
	for( ;it != m_geomVec.end() ; ++it)
	{
		PhysicsGeom_Item& item = *it;
		item.life++;
	}
}


int PhysicsGeomPool::CleanAll(int minLife/* = 0*/)
{
#ifdef _DEBUG
	int nSize = m_geomVec.size();
#endif
	int cleanNb = 0;
	std::vector<PhysicsGeom_Item>::iterator it = m_geomVec.begin();

	
	for( ;it != m_geomVec.end() ; )
	{

		PhysicsGeom_Item& item = *it;
		if(item.life >= minLife)
		{
			CPhysics::DeleteGeom(item.pGeom);
			it = m_geomVec.erase(it);
			++cleanNb;
		}
		else
			++it;

	}
	return cleanNb;
}

#ifndef __WDECAL_H_
#define __WDECAL_H_
//
#include "wInterfaceVisitors.h"
#include "Sync.h"
#include "GDecalInfo.h"
#include "..\Misc\EventsBase.h"

/*namespace NDb
{
	class CMaterial;
}*/
namespace NWorld
{
class CWorld;
#ifdef A5_VS2022_PORT
// VS2022 port: the event type is never defined anywhere (VC7 allowed typeid of an incomplete class);
// typeid(CShowBloodUpdated) in CEventRegister<>/ThrowEvent<> needs a complete type now
#ifndef A5_CSHOWBLOODUPDATED_DEFINED
#define A5_CSHOWBLOODUPDATED_DEFINED
class CShowBloodUpdated {};
#endif
#else
class CShowBloodUpdated;
#endif
class CDecal : public IVisObj
{
	OBJECT_NOCOPY_METHODS(CDecal);
	ZDATA
	CPtr<CWorld> pWorld;
	CSyncSrcBind<IVisObj> bindGlobal;
	CObj<NGScene::CDecalTarget> pDecalTarget;
	NGScene::SDecalMappingInfo info;
	CDBPtr<NDb::CMaterial> pMaterial;
	vector<CPtr<CObjectBase> > targets;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pWorld); f.Add(3,&bindGlobal); f.Add(4,&pDecalTarget); f.Add(5,&info); f.Add(6,&pMaterial); f.Add(7,&targets); return 0; }
	NGlobal::CEventRegister< CDecal, NWorld::CShowBloodUpdated > r1;
public:
	CDecal() : r1(this,&CDecal::OnShowBloodUpdated) {}
	CDecal( CWorld *pWorld, const CVec3 &vCenter, const CVec3 &vNormal, float fSize, NDb::CMaterial *pMaterial, CObjectBase *pTarget );
	CDecal( CWorld *pWorld, const CVec3 &vCenter, float fSize, NDb::CMaterial *pMaterial, vector<CObjectBase*> &_targets );
	void Visit( IRenderVisitor *p );
	void OnShowBloodUpdated( const CShowBloodUpdated &event );
};
}
//
#endif
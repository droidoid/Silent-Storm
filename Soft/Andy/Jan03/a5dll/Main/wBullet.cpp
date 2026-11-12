#include "StdAfx.h"
#include "wDynObject.h"
#include "wInterfaceVisitors.h"
#include "Sync.h"
#include "wBullet.h"
#include "wMain.h"
#include "GAnimation.h"
#include "GAnimParticles.h"
#include "..\Misc\StrProc.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataGeometry.h"
#include "..\DBFormat\DataRPG.h"
#include "aiMap.h"
#include "wUnitServer.h"
#include "RPGUnitMission.h"
#include "wAckBase.h"
#include "aiSignal.h"
#include "wDecal.h"

namespace NWorld
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CBulletServer
////////////////////////////////////////////////////////////////////////////////////////////////////
class CBulletServer: public IDynamicObject, public IVisObj
{
	OBJECT_NOCOPY_METHODS(CBulletServer);
private:
	ZDATA
	int nTrailCount;
	float fTrailSpeed;
	STime sCast, sLastTrailTime;
	CPtr<CWorld> pWorld;
	vector<STime> sPassTime;
	CPtr<NDb::CModel> pModel;
	CObj<CActionCounter> pAction;
	CSyncSrcBind<IVisObj> bindGlobal;
	vector<NRPG::STrailPoint> trailpointsSet;
	CDGPtr<NAnimation::CSkeletonAnimator> pAnimator;
	CPtr<CUnitServer> pShooter;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&nTrailCount); f.Add(3,&fTrailSpeed); f.Add(4,&sCast); f.Add(5,&sLastTrailTime); f.Add(6,&pWorld); f.Add(7,&sPassTime); f.Add(8,&pModel); f.Add(9,&pAction); f.Add(10,&bindGlobal); f.Add(11,&trailpointsSet); f.Add(12,&pAnimator); f.Add(13,&pShooter); return 0; }

protected:
	const CVec3 GetPosition();

public:
	CBulletServer() {}
	CBulletServer( CWorld *pWorld, const vector<NRPG::STrailPoint> &trail, STime sCast, NDb::CModel *pTrailModel, float fTrailSpeed );
	//
	bool Segment();
	void Visit( IRenderVisitor *p );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CBulletServer::CBulletServer( CWorld *_pWorld, const vector<NRPG::STrailPoint> &_trail, STime _sCast, NDb::CModel *_pTrailModel, float _fTrailSpeed )
	:pWorld(_pWorld), trailpointsSet( _trail ), sCast( _sCast ), pModel( _pTrailModel ), fTrailSpeed( _fTrailSpeed ), nTrailCount( 0 )
{
	sPassTime.resize( trailpointsSet.size() );
	vector<NAnimation::CATrailPath::STrailPoint> animTrailPoints( trailpointsSet.size() );

	for ( int nTemp = 0; nTemp < trailpointsSet.size(); nTemp++ )
	{
		if ( pModel && ( nTemp > 0 ) )
			sPassTime[nTemp] = sCast + fabs( trailpointsSet[nTemp].vPosition - trailpointsSet[nTemp - 1].vPosition ) * 1000.0f / fTrailSpeed;
		else
			sPassTime[nTemp] = sCast;

		CVec3 vDir( trailpointsSet[nTemp].vDir );
		Normalize( &vDir );

		animTrailPoints[nTemp].vDir = vDir * fTrailSpeed;
		animTrailPoints[nTemp].vPosition = trailpointsSet[nTemp].vPosition;
		animTrailPoints[nTemp].sPassTime = sPassTime[nTemp];
	}

	if ( pModel )
	{
		NAnimation::CATrailPath *pRay = new NAnimation::CATrailPath;
		pRay->Init( sCast, animTrailPoints );

		pAnimator = new NAnimation::CSkeletonAnimator(0);
		pAnimator->pTime = pWorld->GetTime();
		pAnimator->AddAnimator( pWorld->GetTime()->GetValue(), pRay );
	}

	bindGlobal.Link( pWorld->GetActive(), this );
	pAction = pWorld->GetActiveCounter();
	pShooter = pWorld->GetUnitServer( trailpointsSet[0].sAttack.pAttacker );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBulletServer::Visit( IRenderVisitor *pVisitor )
{
	if ( pModel )
		pVisitor->AddItemMesh( pModel, pAnimator );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CBulletServer::Segment()
{
	SRand rnd;
	STime sSegmentTime = pWorld->GetTime()->GetValue();
	for ( int nTemp = nTrailCount + 1; nTemp < trailpointsSet.size(); nTemp++ )
	{
		if ( sPassTime[nTemp] > sSegmentTime )
			break;

		nTrailCount = nTemp;
		NRPG::STrailPoint &sCurrent = trailpointsSet[nTemp];
		NDb::CRPGArmor *pArmor = sCurrent.pArmor;
		if ( sCurrent.pObject == 0 )
		{
			// terrain
			CDGPtr<CFuncBase<STerrainInfo> > pInfo = pWorld->GetTerrainInfo();
			if ( pInfo )
			{
				pInfo.Refresh();
				int nX = Float2Int( sCurrent.vPosition.x / FP_GRID_STEP );
				int nY = Float2Int( sCurrent.vPosition.y / FP_GRID_STEP );
				SStepSound ss = pInfo->GetValue().GetStepSound( nX, nY );
				if ( ss.pArmor )
					pArmor = ss.pArmor;
			}
		}
		if ( pArmor && sCurrent.nFloor < 100 )
		{
			if ( pArmor->pShotEffect )
			{
				CVec3 dir = -trailpointsSet[nTemp].vNormal;
				CQuat rndX( random.GetFloat(0,10000), CVec3(1,0,0) );
				if ( fabs( dir.x ) < 0.999f )
					rndX = CQuat( acos( dir.x ), CVec3( 0, -dir.z, dir.y ), true ) * rndX;
				else if ( dir.x < 0 )
					rndX = CQuat( FP_PI, CVec3(0,1,0) ) * rndX;
				pWorld->CreateParticle( sCurrent.vPosition, rndX, pArmor->pShotEffect->GetEffect( &rnd ), sCurrent.nFloor );
			}
			if ( pArmor->pSoundShot )
			{
				NDb::CSound *pS = NDb::GetSound( pArmor->pSoundShot );
				pWorld->MakeSound( sCurrent.vPosition, pS );
				pWorld->GetAISignalManager()->Add( NAI::CreateAISoundSignal( sCurrent.vPosition, pShooter, 3 ) );
			}
		}
		CObjectBase *pCatcher = trailpointsSet[nTemp].pAttackTarget;
		CVec3 vPlace = sCurrent.vPosition;
		CVec3 vNormal = sCurrent.vNormal;
		if ( CDynamicCast<NRPG::IAttackable> pAttackCatcher( pCatcher ) )
		{
			if ( IsValid( pCatcher ) )
			{
				pAttackCatcher->ProcessAttack( sCurrent.nUserID, &sCurrent.sAttack, sCurrent.pArmor );
		
				if ( IsValid( pCatcher ) )
				{
					if ( CDynamicCast<NWorld::CUnitServer> pUS( pCatcher ) )
					{
						pUS->GetUnitRPG()->BulletHit();
						CPtr<NWorld::CUnitServer> pTarget = pWorld->GetUnitServer( trailpointsSet[nTemp].sAttack.pTarget );
						if ( IsValid( pShooter ) )
						{
							if ( pTarget.GetPtr() == pUS.GetPtr() )
								pWorld->GetGlobalAck()->OnDoDamage( pShooter, pUS );
							else
								pWorld->GetGlobalAck()->OnDoAccidentalDamage( pShooter, pUS );
						}
					}
					if ( pArmor->pShotMaterial )
					{
						if ( CDynamicCast<NWorld::IBuilding> pBuilding( pCatcher ) )
							new CDecal( pWorld, vPlace, vNormal, pArmor->fShotRadius, pArmor->pShotMaterial->GetMaterial(&rnd), pBuilding->GetSceneHandle() );
						else
							new CDecal( pWorld, vPlace, vNormal, pArmor->fShotRadius, pArmor->pShotMaterial->GetMaterial(&rnd), pCatcher );
					}
				}
			}
		}
		else
		{
			// must be a terrain?
			if ( sCurrent.pObject == 0 && sCurrent.nFloor < 100 && pArmor->pShotMaterial )
				new CDecal( pWorld, vPlace, vNormal, pArmor->fShotRadius, pArmor->pShotMaterial->GetMaterial(&rnd), pWorld->GetTerrainInfo() );
		}
	}

	if ( nTrailCount >= trailpointsSet.size() - 1 )
	{
		if ( IsValid( pShooter ) )
		{
			CRay ray;
			int nLast = trailpointsSet.size() - 1;
			ray.ptOrigin = trailpointsSet[0].vPosition;
			ray.ptDir = trailpointsSet[nLast].vPosition - ray.ptOrigin;
			Normalize( &( ray.ptDir ) );
			pWorld->GetAISignalManager()->Add( NAI::CreateAIShootSignal( pShooter, ray ) );
		}
		return true; // = erase
	}

	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
IDynamicObject *CreateBulletServer( CWorld *pWorld, const vector<NRPG::STrailPoint> &trail,
		STime sCast, NDb::CModel *pTrailModel, float fTrailSpeed )
{
	return new CBulletServer( pWorld, trail, sCast, pTrailModel, fTrailSpeed );
}
}
using namespace NWorld;
REGISTER_SAVELOAD_CLASS( 0xB18B1160, CBulletServer )

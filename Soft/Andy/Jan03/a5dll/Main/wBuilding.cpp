#include "StdAfx.h"
#include "wBuilding.h"
#include "BuildingInfo.h"
#include "..\Misc\BasicShare.h"
#include "RPGGame.h"
#include "MakeBuilding.h"
#include "BuildingGrid.h"
#include "..\DBFormat\DataGeometry.h"
#include "Grid.h"
#include "Transform.h"
#include "wMain.h"
#include "..\DBFormat\DataMap.h"
#include "aiObjectLoader.h"
namespace NGScene
{
	extern CBasicShare<int, NBuilding::CBuildInfoLoader> shareBuildings;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NAI
{
	extern CBasicShare<int, NAI::CLoadGeometryInfo> shareAIModel;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NBuilding
{
	extern FixSmallPieceID( const NAI::CGeometryInfo::CPieceMap &pieces, int nPieceID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NWorld
{
////////////////////////////////////////////////////////////////////////////////////////////////////
class CBuildingPart: public IBuilding, public NRPG::IAttackable, public IVisObj
{
	OBJECT_NOCOPY_METHODS(CBuildingPart);
	ZDATA
	NBuilding::SPart part;
	CPtr<CBuilding> pParent;
	CSyncSrcBind<IVisObj> bindGlobal;
public:
	int nWallFrags;
	int nSolidFrags;
	bool bNoAI;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&part); f.Add(3,&pParent); f.Add(4,&bindGlobal); f.Add(5,&nWallFrags); f.Add(6,&nSolidFrags); f.Add(7,&bNoAI); return 0; }

	CBuildingPart() {}
	CBuildingPart( const NBuilding::SPart &_part, CBuilding *p, bool _bNoAI ) : part(_part), pParent(p), bNoAI(_bNoAI)
	{
		SetFragmentCount();
		bindGlobal.Link( p->pShow, this );
	}
	NBuilding::SPart GetPartID() const { return part; }
	virtual const SMapBuilding& GetInfo() const { return pParent->GetInfo(); }
	virtual void Update() { pParent->pBInfo->UpdateInfo(); FastUpdate(); }
	virtual CObjectBase* GetSceneHandle() const { return pParent->pBInfo; }
	void FastUpdate() { SetFragmentCount(); bindGlobal.Update(); }
	virtual void Visit( IRenderVisitor *p )
	{
//		if( part.nFloor <= pParent->info.pGrid->GetCutFloor() )
			p->AddBuildingPart( part.nID, pParent->info, pParent->pBInfo );
	}
	virtual void Visit( IAIVisitor* );
	virtual int ProcessAttack( int nUserID, NRPG::CAttackPortion *pAttack, NDb::CRPGArmor *pArmor )
	{
		return pParent->ProcessAttack( nUserID, pAttack, pArmor );
	}
	void SetFragmentCount()
	{
		const NBuilding::SBuildingInfo& info = pParent->pBInfo->GetInfo();
		hash_map<NBuilding::SPart, NBuilding::SStoreyInfo, NBuilding::SPart>::const_iterator i = info.info.find( part );
		if ( i != info.info.end() )
		{
			nWallFrags = i->second.walls.size();
			nSolidFrags = i->second.fragments.size();
		}
	}
	virtual void UpdateAllParts() {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CBuilding
////////////////////////////////////////////////////////////////////////////////////////////////////
CBuilding::CBuilding( CSyncSrc<IVisObj> *_pShow, const SMapBuilding &_info, IWorld *_pWorld, bool _bNoAI )
	: info(_info), pRPG( NRPG::CreateBuilding(_info.pGrid) ), nSegemntCnt(-1), nActionCnt(-1), bNoAI(_bNoAI), pSWMap(_info.pSWMap)
{
	bindGlobal.Link( _pShow, this );
	originalpos = info.pos;
	pWorld = _pWorld;
	ASSERT( IsValid( pWorld ) );
	pShow = _pShow;
	pBInfo = new NBuilding::CBuildingInfoHold( info.pGrid, pSWMap, info.pVariant->GetRecordID() );
	pSplitBInfo = new NBuilding::CBuildingInfoHold( info.pGrid, pSWMap, info.pVariant->GetRecordID(), true );
	if ( info.pGrid->NeedComputeStability() )
		if ( NBuilding::UpdateBuildingStability( info.pVariant->GetRecordID(), info.pGrid, pSWMap, 100 ) )
			ToggleUpdateFlag();

	pBInfo->UpdateInfo();
	const NBuilding::SBuildingInfo& info = pBInfo->GetInfo();

	hash_map<NBuilding::SPart, NBuilding::SStoreyInfo, NBuilding::SPart>::const_iterator i;
	for ( i = info.info.begin(); i != info.info.end(); ++i )
	{
		parts[i->first.nID] = new CBuildingPart( i->first, this, bNoAI );
	}
	bUpdatingStability = false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
IWorld* CBuilding::GetWorld() const 
{ 
	return pWorld;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuilding::Visit( IRenderVisitor *p )
{
	p->AddBuilding( info );
//	ASSERT(0);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuilding::Visit( IAIVisitor *p )
{
	//p->AddBuilding( info );
	//ASSERT(0);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// обновляем только те куски здания, где изменилась сетка
void CBuilding::UpdateBuildingParts()
{
	vector<NBuilding::SPart> toupdate;
	info.pGrid->GetUpdatedParts( &toupdate );
	pBInfo->UpdateInfo( toupdate );
	const NBuilding::SBuildingInfo &info = pBInfo->GetInfo();
	for ( int i = 0; i < toupdate.size(); ++i )
	{
		const NBuilding::SPart &part = toupdate[i];
		if ( info.info.find( part ) == info.info.end() )
		{
			parts.erase( part.nID );
			continue;
		}
		if ( IsValid( parts[part.nID] ) )
			parts[part.nID]->FastUpdate();
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// обновляем все куски здания
void CBuilding::UpdateAllParts()
{
	pBInfo->UpdateInfo();
	const NBuilding::SBuildingInfo &info = pBInfo->GetInfo();
	for ( CPartsHash::iterator i = parts.begin(); i != parts.end(); )
	{
		NBuilding::SPart part;
		part.nID = i->first;
		if ( info.info.find( part ) == info.info.end() )
		{
			CPartsHash::iterator itmp = i;
			++itmp;
			parts.erase( i ); // не хочет возвращать итератор на след элемент почему то :(
			i = itmp;
			continue;
		}
		else
			++i;
		if ( IsValid( parts[part.nID] ) )
			parts[part.nID]->Update();
	}
	//
	hash_map<NBuilding::SPart, NBuilding::SStoreyInfo, NBuilding::SPart>::const_iterator i;
	for ( i = info.info.begin(); i != info.info.end(); ++i )
	{
		if ( parts.find( i->first.nID ) == parts.end() )
			parts[i->first.nID] = new CBuildingPart( i->first, this, bNoAI );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuilding::Update()
{ 
	pBInfo->UpdateInfo();
	const NBuilding::SBuildingInfo& info = pBInfo->GetInfo();

	for ( CPartsHash::iterator i = parts.begin(); i != parts.end(); )
	{
		NBuilding::SPart pid;
		pid.nID = i->first;
		hash_map<NBuilding::SPart, NBuilding::SStoreyInfo, NBuilding::SPart>::const_iterator it = info.info.find( pid );

		if ( it == info.info.end() )
		{
			CPartsHash::iterator itmp = i;
			++itmp;
			parts.erase( i ); // не хочет возвращать итератор на след элемент почему то :(
			i = itmp;
		}
		else
		{
			CBuildingPart *p = i->second;
			int nWallFrags = it->second.walls.size();
			int nSolidFrags = it->second.fragments.size();
			if ( p->nWallFrags != nWallFrags || p->nSolidFrags != nSolidFrags )
			{
				p->Update();
			}
			++i;
		}
	}
	//
	hash_map<NBuilding::SPart, NBuilding::SStoreyInfo, NBuilding::SPart>::const_iterator i;
	for ( i = info.info.begin(); i != info.info.end(); ++i )
	{
		if ( parts.find( i->first.nID ) == parts.end() )
			parts[i->first.nID] = new CBuildingPart( i->first, this, bNoAI );
	}
	//
	ToggleUpdateFlag();
	UpdateBuildingParts();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuilding::Explode( const CVec3 &ptEpic, int nPower )
{
	info.pGrid->Explode( ptEpic, nPower, sqrt( nPower ) );
	UpdateBuildingParts();
	ToggleUpdateFlag();
		//NBuilding::UpdateBuildingStability( (*it)->info.pVariant->GetRecordID(), (*it)->info.pGrid );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CBuilding::ProcessAttack( int nUserID, NRPG::CAttackPortion *pAttack, NDb::CRPGArmor *pArmor )
{
	CDynamicCast<NRPG::IAttackable> pAtk( pRPG );
	ASSERT( pAtk );
	int nRes = pAtk->ProcessAttack( nUserID, pAttack, pArmor );
	if ( nRes )
	{
//		UpdateBuildingParts(); вынесено в Segment
		ToggleUpdateFlag();
	}
	return nRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuilding::ToggleUpdateFlag()
{
	nSegemntCnt = 0;
	if ( CDynamicCast<CWorld> p( pWorld ) )
		pAction = p->GetActiveCounter( 10 );
	//OutputDebugString( "Building action acquired\n" );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CBuilding::Segment() 
{
	if ( nSegemntCnt == 0 )
		UpdateBuildingParts();
	if ( nSegemntCnt >= 0 && ++nSegemntCnt == 5 )
	{
		nSegemntCnt = -1;
		bUpdatingStability = false;
		//pAction = 0;
		nActionCnt = 0;
		if ( info.pGrid->NeedComputeStability() )
		{
			if ( NBuilding::UpdateBuildingStability( info.pVariant->GetRecordID(), info.pGrid, pSWMap ) )
			{
				nSegemntCnt = 0;
				bUpdatingStability = true;
				nActionCnt = -1;
//				if ( CDynamicCast<CWorld> p( pWorld ) )
//					pAction = p->GetActiveCounter();
			}
			else
				UpdateBuildingParts();
		}
	}
	else if ( nSegemntCnt >= 0 && bUpdatingStability )
	{
		if ( !NBuilding::UpdateBuildingStability( info.pVariant->GetRecordID(), info.pGrid, pSWMap ) )
		{
			nSegemntCnt = -1;
			bUpdatingStability = false;
			//pAction = 0;
			nActionCnt = 0;
			UpdateBuildingParts();
		}
	}
	if ( nActionCnt >=0 && ++nActionCnt == 30 )
	{
		//OutputDebugString( "Building Action release\n" );
		pAction = 0;
		nActionCnt = -1;
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuilding::SetPosition( const SFBTransform &pos )
{
	info.pos = pos * originalpos;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CBuildingPart
////////////////////////////////////////////////////////////////////////////////////////////////////
inline int MakePieceUserID( const SDiscretePos &dpos, int nHashID )
{
	int x, y, z;
	NBuilding::GetPieceCoords( nHashID, &x, &y, &z );
	CVec3 tmp( x - 1, y - 1, z - 1 );
	dpos.MoveAndRotate( &tmp );
	return NBuilding::GetPieceHash( tmp );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void AddPieces( IAIVisitor *p,
	NDb::CAIGeometry *pAIGeom, 
	NAI::CGeometryInfo *pGI, 
	const vector<int> &parts, 
	const SFBTransform &pos, const SDiscretePos &dpos, 
	NDb::CRPGArmor *pArmor, int nFloor, int nMask,
	int nFragmentID, bool bUseFragmentID )
{
	vector<IAIVisitor::SPieceMap> rparts;
	for ( vector<int>::const_iterator i = parts.begin(); i != parts.end(); ++i )
	{
		int nPart = *i;
		const int nPieceID = NBuilding::FixSmallPieceID( pGI->pieces, nPart );
		int nUserID = bUseFragmentID ? nFragmentID : MakePieceUserID( dpos, nPieceID );
		IAIVisitor::SPieceMap m;
		m.nPieceID = nPart;
		m.nUserID = nUserID;
		rparts.push_back( m );
	}
	if ( !rparts.empty() )
		p->AddPieces( pAIGeom, rparts, pos, pArmor, nFloor, nMask );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int GetMask( NDb::CAIGeometry *pAIGeometry, NDb::CRPGArmor *pArmor );
void CBuildingPart::Visit( IAIVisitor* p )
{
	if ( bNoAI || !IsValid( pParent ) || !IsValid( pParent->pBInfo ) )
		return;
	const SMapBuilding &bInfo = pParent->info;
	SFBTransform place = bInfo.pos;
	const NBuilding::SBuildingInfo &info = pParent->pBInfo->GetInfo();

	hash_map<NBuilding::SPart, NBuilding::SStoreyInfo, NBuilding::SPart>::const_iterator i = info.info.find( part );
	if ( i == info.info.end() )
		return;
	const NBuilding::SStoreyInfo &storey = i->second;
	const SMapBuilding::SStorey &storeyInfo = bInfo.GetStorey( storey.nFloor );
	int nAIFloor = storeyInfo.nRealFloor;

	for ( int k = 0; k < storey.walls.size(); ++k )
	{
		const NBuilding::SStoreyInfo::SFragment &wall = storey.walls[k];
		NDb::CAIGeometry *pAIGeom = wall.info.pGeometry->pAIGeometry;
		if ( !IsValid( pAIGeom ) )
			continue;
		const int key = pAIGeom->GetRecordID();
		CDGPtr< CPtrFuncBase<NAI::CGeometryInfo> > pSrc = NAI::shareAIModel.Get( key );
		pSrc.Refresh();
		NAI::CGeometryInfo *pGI = pSrc->GetValue();
		if ( !pGI )
			continue;
		
		CVec3 ptPos( wall.info.ptPos.x * FP_GRID_STEP, wall.info.ptPos.y * FP_GRID_STEP, wall.info.ptPos.z * NBuilding::WALL_HEIGHT );
		SDiscretePos dpos( 0, wall.info.ptPos, wall.info.nRotationID );
		SFBTransform position = place * MakeTransform( ptPos, RotationIDToAngle( wall.info.nRotationID ) );
		vector<int> parts;
		NBuilding::UnpackParts( &parts, wall.info.dwParts, wall.info.nSubBlockID );
		dpos.ptMove.z *= 4;
		if ( NDb::IsNoDamage( pAIGeom->traficability ) )
			AddPieces( p, pAIGeom, pGI, parts, position, dpos, wall.pArmor, nAIFloor, GetMask( pAIGeom, wall.pArmor ), wall.nFragmentID | 0x40000000, true );
		else
		{
			int nFlags = GetMask( pAIGeom, wall.pArmor );
			ASSERT( nFlags & TS_FRAGMENTED );
			AddPieces( p, pAIGeom, pGI, parts, position, dpos, wall.pArmor, nAIFloor, nFlags & (~TS_FRAGMENTED), wall.nFragmentID | 0x40000000, true );
			NBuilding::SplitOptimized( &parts );
			AddPieces( p, pAIGeom, pGI, parts, position, dpos, wall.pArmor, nAIFloor, TS_FRAGMENTED, wall.nFragmentID, false );
		}
	}
	for ( int k = 0; k < storey.fragments.size(); ++k )
	{
		const NBuilding::SStoreyInfo::SFragment &solid = storey.fragments[k];
		NDb::CAIGeometry *pAIGeom = solid.info.pGeometry->pAIGeometry;
		if ( !IsValid( pAIGeom ) )
			continue;
		const int key = pAIGeom->GetRecordID();
		CDGPtr< CPtrFuncBase<NAI::CGeometryInfo> > pSrc = NAI::shareAIModel.Get( key );
		pSrc.Refresh();
		NAI::CGeometryInfo *pGI = pSrc->GetValue();
		if ( !pGI )
			continue;
		
		CVec3 ptPos( solid.info.ptPos.x * FP_GRID_STEP, solid.info.ptPos.y * FP_GRID_STEP, solid.info.ptPos.z * NBuilding::WALL_HEIGHT );
		SFBTransform position = place * MakeTransform( ptPos, RotationIDToAngle( solid.info.nRotationID ) );
		SDiscretePos dpos( 0, solid.info.ptPos, solid.info.nRotationID );
		
		int nFlags = GetMask( pAIGeom, solid.pArmor );
		vector<int> parts;
		NBuilding::UnpackParts( &parts, solid.info.dwParts, solid.info.nSubBlockID );
		dpos.ptMove.z *= 4;
		if ( NDb::IsNoDamage( pAIGeom->traficability ) )
			AddPieces( p, pAIGeom, pGI, parts, position, dpos, solid.pArmor, nAIFloor, nFlags, solid.nFragmentID | 0x40000000, true );
		//AddPieces( &pDst->hulls, parts, solid.pArmor, key, pPosition, dpos, nHGroup, TS_VIRTUAL, pSrc, solid.nFragmentID, true );
		else
		{
			AddPieces( p, pAIGeom, pGI, parts, position, dpos, solid.pArmor, nAIFloor, nFlags & (~TS_FRAGMENTED), solid.nFragmentID, true );
			//AddPieces( &pDst->hulls, parts, solid.pArmor, key, pPosition, dpos, nHGroup, TS_OBJECTS|TS_VISION|nFlags, pSrc, solid.nFragmentID, true );
			NBuilding::SplitOptimized( &parts );
			AddPieces( p, pAIGeom, pGI, parts, position, dpos, solid.pArmor, nAIFloor, TS_FRAGMENTED, solid.nFragmentID, false );
			//AddPieces( &pDst->hulls, parts, solid.pArmor, key, pPosition, dpos, nHGroup, TS_FRAGMENTED, pSrc, solid.nFragmentID, false );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
using namespace NWorld;
REGISTER_SAVELOAD_CLASS( 0x02741136, CBuilding )
REGISTER_SAVELOAD_CLASS( 0xA0142160, CBuildingPart )

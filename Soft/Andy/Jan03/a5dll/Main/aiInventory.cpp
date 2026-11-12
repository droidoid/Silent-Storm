#include "stdafx.h"
//
#include "RPGUnitMission.h"
#include "RPGItemSet.h"
#include "RPGItem.h"
#include "RPGAttackMech.h"
//
#include "..\DBFormat\DataRPG.h"
//
#include "aiUnit.h"
#include "aiWeapon.h"
#include "aiPosition.h"
#include "aiInventory.h"
//
#include "wUnitServer.h"
#include "..\MiscDll\LogStream.h"
//
namespace NAI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIInventory
////////////////////////////////////////////////////////////////////////////////////////////////////
CAIInventory::CAIInventory( IAIUnit *_pOwner ):	pOwner(_pOwner)
{
	ASSERT( IsValid( pOwner ) );
	if ( IsValid( pOwner ) )
	{
		FetchInventoryItems();
		//
		CPtr<NRPG::IInventoryItem> pItem = pOwner->GetUnitMission()->GetInventory()->GetActive();
		if ( IsValid( pItem ) )
		{
			CPtr<IAIInventoryItem> pInvItem = GetAIInventoryItem( pItem );
			ASSERT( IsValid( pInvItem ) );
			if ( IsValid( pInvItem ) )
				SetCurrentItem( pInvItem );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
template< class T >
static IAIInventoryItem* FindAIInventoryItem( NRPG::IInventoryItem *pItem, const vector< CObj<T> > &items )
{
	ASSERT( IsValid( pItem ) );
	if ( IsValid( pItem ) )
	{
		for ( vector< CObj<T> >::const_iterator i = items.begin(); i != items.end(); ++i )
		{
			if ( CDynamicCast<NRPG::IInventoryItem>( (*i)->GetItem() ).GetPtr() == pItem )
				return *i;
		}
	}
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
IAIInventoryItem* CAIInventory::GetAIInventoryItem( NRPG::IInventoryItem *pItem ) const
{
	IAIInventoryItem *pRes = 0;
	ASSERT( IsValid( pItem ) );
	if ( IsValid( pItem ) )
	{
		pRes = FindAIInventoryItem( pItem, fireArms );
		if ( !pRes )
			pRes = FindAIInventoryItem( pItem, grenades );
		if ( !pRes )
			pRes = FindAIInventoryItem( pItem, rocketLaunchers );
	}
	return pRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIInventory::GetInventoryItems( list< CPtr<NRPG::IInventoryItem> > *pItems ) const
{
	pItems->clear();
	CPtr<NRPG::IInventory> pInventory = pOwner->GetUnitMission()->GetInventory();
	// вещи из рюкзачка
	const vector<NRPG::SBackPackItem> &vBackPackItems = pInventory->GetItems();
	vector<NRPG::SBackPackItem>::const_iterator i;
	for ( i = vBackPackItems.begin(); i != vBackPackItems.end(); ++i )
	{
		CPtr<NRPG::IInventoryItem> pItem = (*i).pItem;
		ASSERT( IsValid( pItem ) );
		if ( IsValid( pItem ) )
			pItems->push_back( pItem );
	}
	// вещи из слотов
	for ( int nSlot = 0; nSlot < NDb::N_SLOTS; ++nSlot )
	{
		CPtr<NRPG::IInventoryItem> pItem = pInventory->Get( (NDb::ESlot)nSlot );
		if ( IsValid( pItem ) )
			pItems->push_back( pItem );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIInventory::FetchInventoryItems()
{
	list< CPtr<NRPG::IInventoryItem> > items;
	GetInventoryItems( &items );
	for ( list< CPtr<NRPG::IInventoryItem> >::iterator i = items.begin(); i != items.end(); ++i  )
	{
		ASSERT( IsValid( *i ) );
		if ( IsValid( *i ) )
		{
			if ( CDynamicCast<NRPG::CWeaponItem> pFireArms( *i ) )
			{
				CPtr<CAIFireArmsWeapon> pWeapon = CreateAIFireArmsWeapon( pOwner, pFireArms );
				if ( pWeapon->IsRocketLauncher() )
					AddRocketLaunchers( pWeapon );
				else
					AddFireArms( pWeapon );
			}
			if ( CDynamicCast<NRPG::CGrenadeItem> pGrenade( *i ) )
				AddGrenade( CreateAIGrenadeWeapon( pGrenade ) );
		}
	}
	//
	for ( list< CPtr<NRPG::IInventoryItem> >::iterator i = items.begin(); i != items.end(); ++i  )
	{
		ASSERT( IsValid( *i ) );
		if ( IsValid( *i ) )
		{
			if ( CDynamicCast<NRPG::CClipItem> pClip( *i ) )
				AddClip( CreateAIFireArmsWeaponClip( pClip ) );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIInventory::AddClip( CAIFireArmsWeaponClip *pClip )
{
	ASSERT( IsValid( pClip ) );
	CPtr<CAIFireArmsWeaponClip> pHolder = pClip;
	CPtr<CAIFireArmsWeapon> pWeapon = GetSuitableWeapon( pClip );
	if ( IsValid(pWeapon) )
		pWeapon->AddClip( pClip );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
template< class T >
static void AddItemToInventory( T *pItem, vector< CObj<T> > *pVector )
{
	ASSERT( IsValid( pItem ) );
	ASSERT( find( pVector->begin(), pVector->end(), pItem ) == pVector->end() );
	if ( IsValid( pItem ) )
		pVector->push_back( pItem );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
template< class T >
void RemoveItemFromInventory( T *pItem, vector< CObj<T> > *pVector )
{
	bool bCurrent = false;
	ASSERT( IsValid( pItem ) );
	ASSERT( find( pVector->begin(), pVector->end(), pItem ) != pVector->end() );
	pVector->erase( remove( pVector->begin(), pVector->end(), pItem ), pVector->end() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIInventory::AddFireArms( CAIFireArmsWeapon *pWeapon )
{
	bool bRocketLauncher = pWeapon->IsRocketLauncher();
	ASSERT( !bRocketLauncher );
	if ( !bRocketLauncher )
		AddItemToInventory( pWeapon, &fireArms );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIInventory::RemoveFireArms( CAIFireArmsWeapon *pWeapon )
{
	RemoveItemFromInventory( pWeapon, &fireArms );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIInventory::AddRocketLaunchers( CAIFireArmsWeapon *pLauncher )
{
	bool bRocketLauncher = pLauncher->IsRocketLauncher();
	ASSERT( bRocketLauncher );
	if ( bRocketLauncher )
		AddItemToInventory( pLauncher, &rocketLaunchers );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIInventory::RemoveRocketLaunchers( CAIFireArmsWeapon *pLauncher )
{
	RemoveItemFromInventory( pLauncher, &rocketLaunchers );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIInventory::AddGrenade( CAIGrenadeWeapon *pGrenade )
{
	AddItemToInventory( pGrenade, &grenades );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIInventory::RemoveGrenade( CAIGrenadeWeapon *pGrenade )
{
	RemoveItemFromInventory( pGrenade, &grenades );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIInventory::SetCurrentItem( IAIInventoryItem *pItem )
{
	if ( IsValid( pItem ) )
		pCurrent = pItem;
	else
		pCurrent = 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
IAIInventoryItem* CAIInventory::GetCurrentItem() const 
{ 
	return pCurrent; 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAIInventory::IsCurrentItem( IAIInventoryItem *pItem ) const
{
	ASSERT( IsValid( pItem ) );
	return pCurrent == pItem;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CAIFireArmsWeapon* CAIInventory::GetCurrentFireArms() const
{
	CPtr<CAIFireArmsWeapon> pRes = CDynamicCast<CAIFireArmsWeapon>( pCurrent ).GetPtr();
	if ( !IsValid( pRes ) || pRes->IsRocketLauncher() )
		return 0;
	else
		return pRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CAIFireArmsWeapon* CAIInventory::GetCurrentRocketLauncher() const
{
	CPtr<CAIFireArmsWeapon> pRes = CDynamicCast<CAIFireArmsWeapon>( pCurrent ).GetPtr();
	if ( !IsValid( pRes ) || !pRes->IsRocketLauncher() )
		return 0;
	else
		return pRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CAIGrenadeWeapon* CAIInventory::GetCurrentGrenade() const
{
	return CDynamicCast<CAIGrenadeWeapon>( pCurrent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static CAIFireArmsWeapon* FindSuitableWeapon( CAIFireArmsWeaponClip *pClip, const vector< CObj<CAIFireArmsWeapon> > &weapons )
{
	for ( vector< CObj<CAIFireArmsWeapon> >::const_iterator i = weapons.begin(); i != weapons.end(); ++i  )
	{
		if ( (*i)->IsSuitableClip( pClip ) )
			return *i;
	}
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CAIFireArmsWeapon* CAIInventory::GetSuitableWeapon( CAIFireArmsWeaponClip *pClip ) const
{
	CAIFireArmsWeapon *pRes = 0;
	ASSERT( IsValid( pClip ) );
	if ( IsValid( pClip ) )
	{
		pRes = FindSuitableWeapon( pClip, fireArms );
		if ( !pRes )
			pRes = FindSuitableWeapon( pClip, rocketLaunchers );
	}
	return pRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CAIFireArmsWeapon* CAIInventory::GetBestFireArms( const NAI::SUnitPosition &pos, IAIUnit *pTarget, 
	int nAP, int *pBestWeaponHitCover, int *pQuality,	NDb::EShootMode *eShootMode, int *nMaxToHit ) const
{
	CAIFireArmsWeapon *pBestWeapon = 0;
	int nShootMode = 0;
	*nMaxToHit = 0;
	*pBestWeaponHitCover = 0;
	*pQuality = -0xFFFF;
	for ( vector< CObj<CAIFireArmsWeapon> >::const_iterator i = fireArms.begin(); i != fireArms.end(); ++i  )
	{
		CPtr<NRPG::CWeaponItem> pWeaponItem( (*i)->GetItem() );
		int nHitCover = pOwner->GetCoverForFixedUnit( pos, pTarget->GetUnitServer(), pWeaponItem, NAI::HL_ANY );
		//
		for ( int nShootMode = ( int )NDb::SM_Snap; nShootMode != ( int )NDb::SM_Snipe; ++nShootMode )
		{
			if ( !pWeaponItem->IsShootModeSupported( ( NDb::EShootMode )nShootMode ) )
				continue;
			//
			int nTmpMaxToHit;
			int nTmpQuality = (*i)->GetDamage( pos, pTarget, nHitCover, NAI::WALK, nAP, ( NDb::EShootMode )nShootMode, &nTmpMaxToHit );
			*nMaxToHit = max( *nMaxToHit, nTmpMaxToHit );
			if ( nTmpQuality > *pQuality )
			{
				pBestWeapon = *i;
				*pQuality = nTmpQuality;
				*eShootMode = (NDb::EShootMode)nShootMode;
				*pBestWeaponHitCover = nHitCover;
			}
		}
	}
	return pBestWeapon;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CAIGrenadeWeapon* CAIInventory::GetBestGrenade( CVec3 ptTarget ) const
{
	if ( grenades.empty() )
		return 0;
	else
	{
		CPtr<CAIGrenadeWeapon> pGrenade = GetCurrentGrenade();
		if ( !IsValid( pGrenade ) )
			pGrenade = grenades.front();
		return pGrenade;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CAIFireArmsWeapon* CAIInventory::GetBestRocketLaunchers() const
{
	if ( rocketLaunchers.empty() )
		return 0;
	else
	{
		CPtr<CAIFireArmsWeapon> pLauncher = GetCurrentRocketLauncher();
		if ( !IsValid( pLauncher ) )
			pLauncher = rocketLaunchers.front();
		return pLauncher;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAIInventory::HasAnyWeapon() const
{
	return !fireArms.empty() || !grenades.empty() || !rocketLaunchers.empty();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIInventory::DebugOutput() const
{
	csSystem << "Unit inventory:\n[\n";
	//
	csSystem << "FireArms : " << ( int )fireArms.size();
	csSystem << "     [  ";
	vector< CObj<CAIFireArmsWeapon> >::const_iterator i;
	for ( i = fireArms.begin(); i != fireArms.end(); ++i )
		csSystem << "  " << (*i)->GetClipCount();
	csSystem << "   ]\n";
	//
	csSystem << "RocketLaunchers : " << ( int )rocketLaunchers.size();
	csSystem << "     [  ";
	for ( i = rocketLaunchers.begin(); i != rocketLaunchers.end(); ++i )
		csSystem << "  " << (*i)->GetClipCount();
	csSystem << "   ]\n";
	//
	csSystem << "Grenades : " << ( int )grenades.size() << "\n";
	csSystem << "]\n";
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CONSTRUCTORS
////////////////////////////////////////////////////////////////////////////////////////////////////
CAIInventory* CreateAIInventory( IAIUnit *pOwner )
{
	ASSERT( pOwner->GetUnitServer()->CanFight() );
	return new CAIInventory( pOwner );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
using namespace NAI;
//
REGISTER_SAVELOAD_CLASS( 0x52642102, CAIInventory );
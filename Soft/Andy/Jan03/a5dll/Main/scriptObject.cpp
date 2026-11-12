#include "stdafx.h"
//
#include "A5Script.h"
#include "scriptCommon.h"
#include "scriptPtr.h"
#include "wMain.h"
#include "wOSBase.h"
#include "wObject.h"
#include "rpgAttackMech.h"
#include "..\DBFormat\DataRPG.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataObject.h"
#include "..\MiscDll\LogStream.h"
#include "aiRoute.h"
#include "wDebris.h"
//
#include "scriptObject.h"
//
namespace NScript
{
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( GetItem, "s" )
	string szName = luaParams[ 0 ].s;
	CPtr<NWorld::CDFrozenItem> pItem = pScript->pWorld->GetItemByName( szName );
	luaPushCPtr( pState, pItem );
	if ( !IsValid( pItem ) )
		csSystem << CC_RED << "Script warning: " << CC_GREY << " item [" << szName << "] not found" << endl;
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( ItemGetName, "u" )
	if ( CDynamicCast<NWorld::CDFrozenItem> pItem( luaParams[ 0 ].p ) )
	{
		string szName;
		if ( pScript->pWorld->GetItemName( pItem, &szName ) )
		{
			pScript->PushString( szName.c_str() );
			return 1;
		}
	}
	//
	pScript->PushNil();
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( ItemRemove, "u" )
	if ( CDynamicCast<NWorld::CDFrozenItem> pItem( luaParams[ 0 ].p ) )
		pScript->pWorld->RemoveFrozenItem( pItem->GetInvItem() );
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( GetObject, "s" )
	string szName = luaParams[ 0 ].s;
	CPtr<NWorld::CObjectServerBase> pObject = pScript->pWorld->GetObjectByName( szName );
	luaPushCPtr( pState, pObject );
	if ( !IsValid( pObject ) )
		csSystem << CC_RED << "Script warning: " << CC_GREY << " object [" << szName << "] not found" << endl;
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
static void OpenCloseObject( CObjectBase *pObject, bool bOpen )
{
	CDynamicCast<NWorld::CWindowDoor> pDoor( pObject );
	if ( IsValid( pDoor ) )
		pDoor->OpenClose( bOpen, false );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( ObjectOpen, "u" )
	OpenCloseObject( luaParams[ 0 ].p, true );
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( ObjectClose, "u" )
	OpenCloseObject( luaParams[ 0 ].p, false );
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( ObjectIsOpened, "u" )
	CDynamicCast<NWorld::CWindowDoor> pObject( luaParams[ 0 ].p );
	if ( IsValid( pObject ) && pObject->IsOpen() )
		pScript->PushNumber( 1 );
	else
		pScript->PushNil();
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( ObjectDestroy, "u" )
	if ( IsValid( luaParams[ 0 ].p ) )
	{
		if ( CDynamicCast<NRPG::IAttackable> pAtt( luaParams[ 0 ].p ) )
		{
			CRay ray;
			NRPG::CAttackPortion atk;
			ray.ptDir = VNULL3;
			ray.ptOrigin = VNULL3;
			atk.MakeClickOfDeath( ray );
			atk.atkType = NRPG::AT_NORMAL;
			pAtt->ProcessAttack( 0, &atk, NDb::GetArmor( NDb::N_DEFAULT_ARMOR ) );
		}
	}
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( ObjectRemove, "u" )
	CDynamicCast<NWorld::CObjectServerBase> pObject( luaParams[ 0 ].p );
	if ( IsValid( pObject ) )
		pScript->pWorld->KillObject( pObject );
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( CreateObject, "nsns" )
	// DBID, WAYPOINT, ANGLE, NAME
	int nID = luaParams[ 0 ].n;
	string szWaypoint = luaParams[ 1 ].s;
	int nAngle = luaParams[ 2 ].n;
	string szName = luaParams[ 3 ].s;
	//
	CPtr<NAI::CAIRouteWaypoint> pWaypoint = pScript->pWorld->GetWaypoint( szWaypoint );
	if ( IsValid( pWaypoint ) )
	{
		CDBPtr<NDb::CRndObject> pDBRndObject = NDb::GetDBRndObject( nID );
		if ( IsValid( pDBRndObject ) )
		{
			SRand rand;
			vector<int> params;
			CPtr<NDb::CObject> pDBObject = pDBRndObject->CreateObject( &rand, params );
			if ( IsValid( pDBObject ) )
			{
				NWorld::SObjectPlace pos = pWaypoint->GetObjectPlace( nAngle );
				CPtr<NWorld::CObjectServerBase> pObject = pScript->pWorld->AddObject( pos, pDBObject, szName );
				if ( IsValid( pObject ) )
				{
					luaPushCPtr( pState, pObject );
					return 1;
				}
			}
		}
	}
	//
	pScript->PushNil();
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( ObjectSetToWaypoint, "usn" )
	string szWaypoint = luaParams[ 1 ].s;
	int nAngle = luaParams[ 2 ].n;
	CDynamicCast<NWorld::CObjectServerBase> pObject( luaParams[ 1 ].p );
	if ( !IsValid( pObject ) )
		return 0;
	//
	CPtr<NAI::CAIRouteWaypoint> pWaypoint = pScript->pWorld->GetWaypoint( szWaypoint );
	if ( !IsValid( pWaypoint ) )
		return 0;
	//
	NWorld::SObjectPlace pos = pWaypoint->GetObjectPlace( nAngle );
	pObject->SetPosition( pos );
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( ObjectPlayAnimation, "un" )
	CDynamicCast<NWorld::CAnimObjectServerBase> pObject( luaParams[ 0 ].p );
	if ( IsValid( pObject ) )
		pObject->PlayDBAnimation( luaParams[ 1 ].n );
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( ObjectIsAction, "u" )
	CDynamicCast<NWorld::CAnimObjectServerBase> pObject( luaParams[ 0 ].p );
	if ( IsValid( pObject ) && pObject->IsPerformingAction() )
		pScript->PushNumber( 1 );
	else
		pScript->PushNil();
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( ObjectSetDestroyStage, "un" )
	CDynamicCast<NWorld::CObjectServerBase> pObject( luaParams[ 0 ].p );
	if ( IsValid( pObject ) )
		pObject->SetDestroyStage( pScript->GetObject( 2 ).GetInteger() );
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( ObjectGetName, "u" )
	if ( CDynamicCast<NWorld::CObjectServerBase> pOS( luaParams[ 0 ].p ) )
	{
		string szName;
		if ( pScript->pWorld->GetObjectName( pOS, &szName ) )
		{
			pScript->PushString( szName.c_str() );
			return 1;
		}
	}
	else
		csSystem << CC_RED << "Invalid object : " << IsValid( luaParams[ 0 ].p ) << endl;

	pScript->PushNil();
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( ObjectCancelAction, "u" )
	CDynamicCast<NWorld::CAnimObjectServerBase> pObject( luaParams[ 0 ].p );
	if ( IsValid( pObject ) && pObject->IsPerformingAction() )
		pObject->CancelAction();
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
}
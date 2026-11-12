#include "stdafx.h"
#include "A5Script.h"
#include "scriptPtr.h"
#include "scriptCommon.h"
#include "wMain.h"
#include "wUnitGroup.h"
#include "wUnitServer.h"
#include "aiRoute.h"
#include "rpgUnitMission.h"
#include "rpgUnit.h"
#include "wUnitCommands.h"
//
#include "scriptUnitGroup.h"
//
namespace NScript
{
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( CreateGroup, "" )
	NWorld::CUnitGroup *pGroup = new NWorld::CUnitGroup();
	if ( IsValid( pGroup ) )
	{
		int nUnits = luaGetParamCount( pState );
		for ( int i = 1; i <= nUnits; ++i )
		{
			string szName = pScript->GetObject( i ).GetString();
			CPtr<NWorld::CUnitServer> pUS = pScript->pWorld->GetUnitServer( szName );
			if ( IsValid( pUS ) )
				pGroup->units.Add( pUS );
		}
		luaPushCObj( pState, pGroup );
	}
	else
		pScript->PushNil();
	//
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( GetGroup, "n" )
	int nGroupID = luaParams[ 0 ].n;
	NWorld::CUnitGroup *pGroup = pScript->pWorld->GetUnitGroup( nGroupID );
	if ( IsValid( pGroup ) )
		luaPushCPtr( pState, pGroup );
	else
	{
		char szGroupID[ 12 ];
		sprintf( szGroupID, "%d", nGroupID );
		ScriptWarning( string( "group number " ) + string( szGroupID ) + string( " not found" )  );
		pScript->PushNil();
	}
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( GroupGetID, "u" )
	CDynamicCast<NWorld::CUnitGroup> pGroup( luaParams[ 0 ].p );
	if ( IsValid( pGroup ) )
		pScript->PushNumber( pGroup->GetID() );
	else
		pScript->PushNumber( 0 );
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( GroupAddUnit, "uu" )
	CDynamicCast<NWorld::CUnitGroup> pGroup( luaParams[ 0 ].p );
	CDynamicCast<NWorld::CUnitServer> pUS( luaParams[ 1 ].p );
	if ( IsValid( pGroup ) && IsValid( pUS ) )
		pGroup->units.Add( pUS );
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( GroupRemoveUnit, "uu" )
	CDynamicCast<NWorld::CUnitGroup> pGroup( luaParams[ 0 ].p );
	CDynamicCast<NWorld::CUnitServer> pUS( luaParams[ 1 ].p );
	if ( IsValid( pGroup ) && IsValid( pUS ) )
		pGroup->units.Remove( pUS );
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( GroupGetSize, "u" )
	if ( CDynamicCast<NWorld::CUnitGroup> pGroup( luaParams[ 0 ].p ) )
		pScript->PushNumber( pGroup->units.GetSize() );
	else
		pScript->PushNil();
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( GroupGetUnit, "un" )
	if ( CDynamicCast<NWorld::CUnitGroup> pGroup( luaParams[ 0 ].p ) )
	{
		int n = luaParams[ 1 ].n;
//		ASSERT( n >= 0 && n < pGroup->units.GetSize() );
		if ( n >= 0 && n < pGroup->units.GetSize() )
		{
			NWorld::CUnitServer *pUS = pGroup->units[ n ];
			if ( IsValid( pUS ) )
			{
				luaPushCPtr( pState, pUS );
				return 1;
			}
		}
		else
		{
			char szIndex[12];
			sprintf( szIndex, "%d", n );
			ScriptWarning( string( "Unit index in group is out of range ( " ) + string( szIndex ) + string( " )" ) );
		}
	}
	//
	pScript->PushNil();
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( GroupIsContainUnit, "uu" )
	CDynamicCast<NWorld::CUnitGroup> pGroup( luaParams[ 0 ].p );
	CDynamicCast<NWorld::CUnitServer> pUS( luaParams[ 1 ].p );
	if ( IsValid( pGroup ) && IsValid( pUS ) )
	{
		if ( pGroup->units.IsContain( pUS ) )
		{
			pScript->PushNumber( 1 );
			return 1;
		}
	}
	pScript->PushNil();
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( GroupMoveToWaypoint, "us" )
	CDynamicCast<NWorld::CUnitGroup> pGroup( luaParams[ 0 ].p );
	if ( !IsValid( pGroup ) )
		return 0;
	//
	CPtr<NAI::CAIRouteWaypoint> pWaypoint = pScript->pWorld->GetWaypoint( luaParams[ 1 ].s );
	if ( !IsValid( pWaypoint ) )
		return 0;
	//
	NAI::SPosition pos = pWaypoint->pos;
	vector< NAI::SPosition > unitPlaces;
	for ( int i = 0; i < pGroup->units.GetSize(); ++i )
		unitPlaces.push_back( pGroup->units[ i ]->GetPosition().pos );
	pScript->pWorld->GetPathNetwork()->FormationMoveTo( &unitPlaces, pos  );
	//
	for ( int i = 0; i < pGroup->units.GetSize(); ++i )
	{
		pGroup->units[ i ]->Do( 
			new NWorld::CCmdSetCommand( pGroup->units[ i ], new NWorld::CCmdPath( unitPlaces[ i ] ) ) );
		pGroup->units[ i ]->Do( 
			new NWorld::CCmdSetCommand( pGroup->units[ i ], new NWorld::CCmdContinue() ) );
	}
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( GroupGetVisible, "u" )
	CDynamicCast<NWorld::CUnitGroup> pGroup( luaParams[ 0 ].p );
	if ( IsValid( pGroup ) )
	{
		CObj<NWorld::CUnitGroup> pVisible = pScript->pWorld->CreateUnitGroup();
		for ( int u = 0; u < pGroup->units.GetSize(); ++u )
		{
			CPtr<NWorld::CUnitServer> pUS = pGroup->units[ u ];
			for ( list< CPtr<NWorld::CUnitServer> >::const_iterator 
				i = pUS->GetTBSVisible().begin(); i != pUS->GetTBSVisible().end(); ++i )
					if ( !pVisible->units.IsContain( *i ) )
						pVisible->units.Add( i->GetPtr() );
		}
		luaPushCObj( pState, pVisible );
		return 1;
	}
	//
	pScript->PushNil();
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( GroupCheat, "unb" )
	CDynamicCast<NWorld::CUnitGroup> pGroup( luaParams[ 0 ].p );
	if ( !IsValid( pGroup ) )
		return 0;
	//
	int nCheat = luaParams[ 1 ].n;
	bool bEnable = luaParams[ 2 ].b;
	for ( int u = 0; u < pGroup->units.GetSize(); ++u )
		pGroup->units[u]->GetUnitRPG()->GetRPGUnit()->SetCheat( nCheat, bEnable );
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( GroupAddGroup, "uu" )
	CDynamicCast<NWorld::CUnitGroup> pLeft( luaParams[ 0 ].p );
	CDynamicCast<NWorld::CUnitGroup> pRight( luaParams[ 1 ].p );
	if ( !IsValid( pLeft ) || !IsValid( pRight ) )
	{
		pScript->PushNil();
		return 1;
	}
	//
	CPtr<NWorld::CUnitGroup> pGroup = new NWorld::CUnitGroup();
	for ( int i = 0; i < pLeft->units.GetSize(); ++i )
		pGroup->units.Add( pLeft->units[ i ] );
	for ( int i = 0; i < pRight->units.GetSize(); ++i )
		pGroup->units.Add( pRight->units[ i ] );
	luaPushCObj( pState, pGroup );
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( GroupGetCross, "uu" )
	CPtr<NWorld::CUnitGroup> pGroup = new NWorld::CUnitGroup();
	CDynamicCast<NWorld::CUnitGroup> pLeft( luaParams[ 0 ].p );
	CDynamicCast<NWorld::CUnitGroup> pRight( luaParams[ 1 ].p );
	if ( IsValid( pLeft ) && IsValid( pRight ) )
	{
		for ( int i = 0; i < pLeft->units.GetSize(); ++i )
		{
			if ( pRight->units.IsContain( pLeft->units[ i ] ) )
				pGroup->units.Add( pLeft->units[ i ] );
		}
	}
	luaPushCObj( pState, pGroup );
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( PlayerGetUnits, "n" )
	CPtr<NWorld::CPlayer> pPlayer = pScript->pWorld->GetPlayerByID( luaParams[ 0 ].n );
	CPtr<NWorld::CUnitGroup> pResult = new NWorld::CUnitGroup();
	if ( IsValid( pPlayer ) )
	{
		vector< CPtr<NWorld::CUnit> > units;
		pPlayer->GetUnits( &units );
		for ( vector< CPtr<NWorld::CUnit> >::iterator i = units.begin(); i != units.end(); ++i )
		{
			if ( CDynamicCast<NWorld::CUnitServer> pUS( i->GetPtr() ) )
				pResult->units.Add( pUS );
		}
	}
	//
	luaPushCObj( pState, pResult );
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
}
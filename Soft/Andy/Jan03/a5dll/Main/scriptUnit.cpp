#include "stdafx.h"
#include "A5Script.h"
#include "scriptCommon.h"
#include "scriptPtr.h"
#include "wMain.h"
#include "wUnitServer.h"
#include "aiPosition.h"
#include "aiCommander.h"
#include "rpgUnit.h"
#include "rpgItemInfo.h"
#include "rpgUnitMission.h"
#include "rpgCritical.h"
#include "rpgGame.h"
#include "..\DBFormat\DataRPG.h"
#include "..\MiscDll\LogStream.h"
#include "wAckBase.h"
#include "wUnitGroup.h"
#include "aiRoute.h"
#include "rpgAttackMech.h"
#include "wUnitCommands.h"
#include "rpgPerk.h"
#include "rpgGlobal.h"
#include "aiUnit.h"
#include "rpgCheatConstants.h"
#include "weActiveItem.h"
#include "scScenarioTracker.h"
#include "scFlowChartItems.h"
#include "wUnitStates.h"
#include "aiTaskCommander.h"
#include "aiInventory.h"
//
#include "scriptUnit.h"
//
namespace NScript
{
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( GetUnit, "s" )
	string szName = luaParams[ 0 ].s;
	CPtr<NWorld::CUnitServer> pUS = pScript->pWorld->GetUnitServer( szName );
	luaPushCPtr( pState, pUS );
	if ( !IsValid( pUS ) )
		csSystem << CC_RED << "Script warning: " << CC_GREY << " unit [" << szName << "] not found" << endl;
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( CreateUnit, "nnssn" )
//PERS_ID, PLAYER_ID, UNIT_NAME, WAYPOINT_NAME, LEVEL
	int nPersID = luaParams[ 0 ].n;
	int nPlayerID = luaParams[ 1 ].n;
	string szName = luaParams[ 2 ].s;
	string szWaypoint = luaParams[ 3 ].s;
	int nLevel = luaParams[ 4 ].n;
	//
	CPtr<NAI::CAIRouteWaypoint> pWaypoint = pScript->pWorld->GetWaypoint( szWaypoint );
	if ( IsValid( pWaypoint ) )
	{
		CPtr<NWorld::CPlayer> pPlayer = pScript->pWorld->GetPlayerByID( nPlayerID );
		if ( IsValid( pPlayer ) )
		{
			CDBPtr<NDb::CRPGPers> pRPGPers = NDb::GetPers( nPersID );
			if ( IsValid( pRPGPers ) )
			{
				CPtr<NRPG::IUnitMission> pRPGUnit = NRPG::CreateUnit( pRPGPers );
				if ( IsValid( pRPGUnit ) )
				{
					pRPGUnit->GetRPGUnit()->SetXPLevel( nLevel );
					CPtr<NWorld::CUnitServer> pUS;
					pUS = pScript->pWorld->AddUnitInGame( pWaypoint->pos.p, pRPGUnit, pPlayer, szName );
					luaPushCPtr( pState, pUS );
					return 1;
				}
			}
		}
	}
	pScript->PushNil();
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( UnitSayAck, "un" )
	CDynamicCast<NWorld::CUnitServer> pUS( luaParams[ 0 ].p );
	if ( IsValid( pUS ) )
		pScript->pWorld->GetGlobalAck()->SayAck( pUS, luaParams[ 1 ].n );
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( HasInventoryItem, "n" )
	NWorld::IPlayer::CUnitSet units;
	int nHasItem = 0;
	CPtr<NWorld::CWorld> pWorld = pScript->pWorld;
	NWorld::CUnitServer *pWhoHas = 0;
	if ( !pWorld )
	{
		pScript->PushNil();
		pScript->PushNil();
		return 2;
	}
	NWorld::IPlayer *pPlayer = pWorld->GetNextPlayerForScript( 0 );
	while ( pPlayer )
	{
		if ( CDynamicCast<NAI::CAICommander>( pPlayer->GetCommander() ) )
		{
			pPlayer = pWorld->GetNextPlayerForScript( pPlayer );
			continue;
		}
		pPlayer->GetUnits( &units );
		for ( int i = 0; i < units.size(); ++i )
		{
			NWorld::CUnit *pWho = units[i];
			CDynamicCast<NWorld::CUnitServer> pServer( pWho );
			NRPG::IInventoryInfo *pInfo = pWho->GetRPG()->GetInventoryInfo();
			for ( int j = 0; j < pInfo->GetItems().size(); ++j )
			{
				const NRPG::SBackPackItem &item = (pInfo->GetItems())[ j ];
				const NDb::CRPGItem *pDBItem = item.pItem->GetDBItem();
				if ( pDBItem->GetRecordID() == luaParams[ 0 ].n )
				{
					++nHasItem;
					pWhoHas = pServer;
				}
			}
			NRPG::IInventoryItem *pItem = pInfo->Get( NDb::SLOT_1 );
			if ( pItem )
			{
				const NDb::CRPGItem *pDBItem = pItem->GetDBItem();
				if ( pDBItem->GetRecordID() == luaParams[ 0 ].n )
				{
					++nHasItem;
					pWhoHas = pServer;
				}
			}
			pItem = pInfo->Get( NDb::SLOT_2 );
			if ( pItem )
			{
				const NDb::CRPGItem *pDBItem = pItem->GetDBItem();
				if ( pDBItem->GetRecordID() == luaParams[ 0 ].n )
				{
					++nHasItem;
					pWhoHas = pServer;
				}
			}
		}
		pPlayer = pWorld->GetNextPlayerForScript( pPlayer );
	}
	if ( nHasItem )
		pScript->PushNumber( nHasItem );
	else
		pScript->PushNil();
	luaPushCPtr( pState, pWhoHas );
	return 2;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( UnitSetXPLevel, "un" )
	if ( CDynamicCast<NWorld::CUnitServer> pUS( luaParams[ 0 ].p ) )
	{
		int nLevel = luaParams[ 1 ].f;
		pUS->GetUnitRPG()->GetRPGUnit()->SetXPLevel( nLevel );
	}
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
int luaUnitApplyCritical( lua_State* pState )
{
	ASSERT( pState != 0 );
	if ( pState == 0 )
		return 0;
	//
	CScript *pScript;
	bool bCriticalFromTable = luaGetParamCount( pState ) == 2;
	if ( !bCriticalFromTable )
	{
		if ( !luaPrepareData( pState, "UnitApplyCritical", "unn", &pScript, &vector<SLuaParams>() ) )
			return 0;
	}
	else
	{
		if ( !luaPrepareData( pState, "UnitApplyCritical", "un", &pScript, &vector<SLuaParams>() ) )
			return 0;
	}
	//
	CDynamicCast<NWorld::CUnitServer> pUS( luaGetPtr( pScript->GetObject( 1 ) ) );
	if ( !IsValid( pUS ) )
		return 0;
	//
	NRPG::SCritical rpgCritical;
	if ( bCriticalFromTable )
	{
		int nID = pScript->GetObject(2).GetInteger();
		CDBPtr<NDb::CRPGCritical> pCritical = NDb::GetDBCritical( nID );
		rpgCritical = NRPG::SCritical( pCritical->hl, 
			pCritical->type, pCritical->nMaxDuration, pCritical->fValue );
	}
	else
	{
		NDb::ECriticalLocation location = (NDb::ECriticalLocation)pScript->GetObject(2).GetInteger();
		NDb::ECritical critical = (NDb::ECritical)pScript->GetObject(3).GetInteger();
		rpgCritical = NRPG::SCritical( location, critical );
	}
	//
	pUS->GetUnitRPG()->ApplyCritical( rpgCritical );
	pUS->ProcessCriticalImmediately( rpgCritical.eCritical );
	csRPG << "<color=red>Critical applied\n";
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( UnitIsAction, "u" )
	CDynamicCast<NWorld::CUnitServer> pUS( luaParams[ 0 ].p );
	if ( IsValid( pUS ) && pUS->IsPerformingAction() )
		pScript->PushNumber( 1 );
	else
		pScript->PushNil();
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( UnitShoot, "uun" )
	CDynamicCast<NWorld::CUnitServer> pUS( luaParams[ 0 ].p );
	if ( !IsValid( pUS ) )
		return 0;
	//
	CDynamicCast<NWorld::CUnitServer> pTarget( luaParams[ 1 ].p );
	if ( !IsValid( pTarget ) )
		return 0;
	//
	NAI::EHitLocation hl = ( NAI::EHitLocation )( int )luaParams[ 2 ].n;
	pUS->Do( new NWorld::CCmdSetCommand( pUS, new NWorld::CCmdShootObject( pTarget, 0, hl ) ) );
	pUS->Do( new NWorld::CCmdSetCommand( pUS, new NWorld::CCmdContinue() ) );
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( UnitSetShootMode, "un" )
	if ( CDynamicCast<NWorld::CUnitServer> pUS( luaParams[ 0 ].p ) )
	{
		NDb::EShootMode mode = ( NDb::EShootMode )luaParams[ 1 ].n;
		pUS->Do( new NWorld::CCmdSetCommand( pUS, new NWorld::CCmdShootMode( mode ) ) );
		pUS->Do( new NWorld::CCmdSetCommand( pUS, new NWorld::CCmdContinue() ) );
	}
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( UnitSetWishPose, "un" )
	if ( CDynamicCast<NWorld::CUnitServer> pUS( luaParams[ 0 ].p ) )
	{
		NAI::EPose pose = ( NAI::EPose )luaParams[ 1 ].n;
		pUS->Do( new NWorld::CCmdSetCommand( pUS, new NWorld::CCmdWishPose( pose ) ) );
		pUS->Do( new NWorld::CCmdSetCommand( pUS, new NWorld::CCmdContinue() ) );
	}
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( UnitSetPose, "un" )
	if ( CDynamicCast<NWorld::CUnitServer> pUS( luaParams[ 0 ].p ) )
	{
		NAI::EPose pose = ( NAI::EPose )luaParams[ 1 ].n;
		NAI::SUnitPosition pos = pUS->GetPosition();
		pos.SetPose( pose );
		pUS->Do( new NWorld::CCmdSetCommand( pUS, new NWorld::CCmdPath( pos.pos, NAI::PF_USE_POSEDIR ) ) );
		pUS->Do( new NWorld::CCmdSetCommand( pUS, new NWorld::CCmdContinue() ) );
	}
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( UnitSetDirection, "un" )
	if ( CDynamicCast<NWorld::CUnitServer> pUS( luaParams[ 0 ].p ) )
	{
		NAI::EDirection dir = ( NAI::EDirection )luaParams[ 1 ].n;
		NAI::SPosition pos = pUS->GetPosition().pos;
		pos.p.SetDirection( dir );
		pUS->Do( new NWorld::CCmdSetCommand( pUS, new NWorld::CCmdPath( pos, NAI::PF_USE_DIR ) ) );
		pUS->Do( new NWorld::CCmdSetCommand( pUS, new NWorld::CCmdContinue() ) );
	}
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( UnitIsDead, "u" )
	CDynamicCast<NWorld::CUnitServer> pUS( luaParams[ 0 ].p );
	if ( !IsValid( pUS ) )
		return 0;
	//
	if ( pUS->IsDead() )
		pScript->PushNumber( 1 );
	else
		pScript->PushNil();
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( UnitIsUnconscious, "u" )
	if ( CDynamicCast<NWorld::CUnitServer> pUS( luaParams[ 0 ].p ) )
	{
		if ( pUS->IsUnconscious() )
		{
			pScript->PushNumber( 1 );
			return 1;
		}
	}
	//
	pScript->PushNil();
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( UnitGetVisible, "u" )
	CObj<NWorld::CUnitGroup> pGroup = pScript->pWorld->CreateUnitGroup();
	if ( CDynamicCast<NWorld::CUnitServer> pUS( luaParams[ 0 ].p ) )
	{
		for ( list< CPtr<NWorld::CUnitServer> >::const_iterator 
			i = pUS->GetTBSVisible().begin(); i != pUS->GetTBSVisible().end(); ++i )
				pGroup->units.Add( i->GetPtr() );
	}
	luaPushCObj( pState, pGroup );
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( UnitIsSeeUnit, "uu" )
	CDynamicCast<NWorld::CUnitServer> pWatcher( luaParams[ 0 ].p );
	CDynamicCast<NWorld::CUnitServer> pTarget( luaParams[ 1 ].p );
	if ( IsValid( pWatcher ) && IsValid( pTarget ) )
	{
		if ( pScript->pWorld->GetGame()->CheckVisibility( pWatcher, pTarget ) )
		{
			pScript->PushNumber( 1 );
			return 1;
		}
	}
	//
	pScript->PushNil();
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( UnitReload, "u" )
	if ( CDynamicCast<NWorld::CUnitServer> pUS( luaParams[ 0 ].p ) )
	{
		pUS->Do( new NWorld::CCmdSetCommand( pUS, new NWorld::CCmdReload() ) );
		pUS->Do( new NWorld::CCmdSetCommand( pUS, new NWorld::CCmdContinue() ) );
	}
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( UnitCheat, "unb" )
	if ( CDynamicCast<NWorld::CUnitServer> pUS( luaParams[ 0 ].p ) )
	{
		int nCheat = luaParams[ 1 ].n;
		bool bEnable = luaParams[ 2 ].b;
		pUS->GetUnitRPG()->GetRPGUnit()->SetCheat( nCheat, bEnable );
		//
		if ( nCheat == NRPG::CHEAT_NOAI )
		{
			if ( CDynamicCast<NAI::CAICommander> pAICommander( pUS->GetPlayer()->GetCommander() ) )
			{
				CPtr<NAI::IAIUnit> pAIUnit = pAICommander->GetAIUnit( pUS );
				ASSERT( IsValid( pAIUnit ) );
				if ( IsValid( pAIUnit ) )
				{
					if ( bEnable ) 
						pAIUnit->DeactivateCurrentControl();
					else
						pAIUnit->ActivateCurrentControl();
				}
			}
		}
	}
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( UnitKill, "u" )
	if ( CDynamicCast<NWorld::CUnitServer> pUS( luaParams[ 0 ].p ) )
	{
		NRPG::SCritical rpgCritical( NDb::CL_HEAD, NDb::C_DEATH );
		pUS->GetUnitRPG()->ApplyCritical( rpgCritical );
		pUS->ProcessCriticalImmediately( rpgCritical.eCritical );
	}
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( UnitPlayAnimation, "unb" )
	if ( CDynamicCast<NWorld::CUnitServer> pUS( luaParams[ 0 ].p ) )
	{
		int nDBAnimationID = luaParams[ 1 ].n;
		bool bCircled = luaParams[ 2 ].b;
		pUS->Do( new NWorld::CCmdSetCommand( pUS, new NWorld::CCmdPlayAnimation( nDBAnimationID, bCircled ) ) );
		pUS->Do( new NWorld::CCmdSetCommand( pUS, new NWorld::CCmdContinue() ) );
	}
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( UnitHide, "u" )
	if ( CDynamicCast<NWorld::CUnitServer> pUS( luaParams[ 0 ].p ) )
	{
		pUS->Do( new NWorld::CCmdSetCommand( pUS, new NWorld::CCmdHide() ) );
		pUS->Do( new NWorld::CCmdSetCommand( pUS, new NWorld::CCmdContinue() ) );
	}
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( UnitDrawPerksTree, "u" )
	CDynamicCast<NWorld::CUnitServer> pUS( luaParams[ 0 ].p );
	if ( IsValid( pUS ) )
		pUS->GetUnitRPG()->GetRPGUnit()->GetPerksTree()->Draw();
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( UnitTakePerk, "un" )
	if ( CDynamicCast<NWorld::CUnitServer> pUS( luaParams[ 0 ].p ) )
		pUS->GetUnitRPG()->GetRPGUnit()->GetPerksTree()->TakePerk( luaParams[ 1 ].n );
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( UnitCancelAction, "u" )
	if ( CDynamicCast<NWorld::CUnitServer> pUS( luaParams[ 0 ].p ) )
		pUS->Do( new NWorld::CCmdCancel() );
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( UnitRemove, "u" )
	if ( CDynamicCast<NWorld::CUnitServer> pUS( luaParams[ 0 ].p ) )
	{
		// если это clue, то он считается уничтоженным
		CPtr<NScenario::CScenarioTracker> pTracker = pScript->pWorld->GetGlobalGame()->pScenarioTracker;
		if ( IsValid( pTracker ) )
		{
			int nPersID = pUS->GetUnitRPG()->GetRPGPersID();
			CPtr<NScenario::CScenarioClue> pClue = pTracker->GetClueByPersID( nPersID );
			if ( !pTracker->IsClueFound( pClue ) )
				pTracker->OnScenarioClueDestroyed( nPersID, true );
		}
		//
		pScript->pWorld->RemoveUnit( pUS );
	}
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( UnitGetName, "u" )
	if ( CDynamicCast<NWorld::CUnitServer> pUS( luaParams[ 0 ].p ) )
	{
		string szName;
		if ( pScript->pWorld->GetUnitName( pUS, &szName ) )
		{
			pScript->PushString( szName.c_str() );
			return 1;
		}
	}
	pScript->PushNil();
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( UnitSetPlayer, "un" )
	if ( CDynamicCast<NWorld::CUnitServer> pUS( luaParams[ 0 ].p ) )
	{
		CPtr<NWorld::CPlayer> pPlayer = pScript->pWorld->GetPlayerByID( luaParams[ 1 ].n );
		if ( IsValid( pPlayer ) )
		{
			pScript->pWorld->ChangeUnitPlayer( pUS, pPlayer );
		}
	}
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( UnitSetDialog, "us" )
	if ( CDynamicCast<NWorld::CUnitServer> pUS( luaParams[ 0 ].p ) )
	{
		pUS->SetDialog( luaParams[ 1 ].s );
		pUS->SetCanTalk( true );
	}
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( UnitSetCanTalk, "ub[true]" )
	if ( CDynamicCast<NWorld::CUnitServer> pUS( luaParams[ 0 ].p ) )
		pUS->SetCanTalk( luaParams[ 1 ].b );
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( GetHero, "" )
	CPtr<NWorld::CWorld> pWorld = pScript->pWorld;
	CPtr<NRPG::CUnit> pHero = pWorld->GetGlobalGame()->GetHero();
	if ( IsValid( pHero ) )
		luaPushCPtr( pState, pWorld->GetUnitServerByPersID( pHero->GetPers()->GetRecordID() ) );
	else
		lua_pushnil( pState );
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( UnitTakeCorpse, "uu" )
	CDynamicCast<NWorld::CUnitServer> pUS( luaParams[ 0 ].p );
	CDynamicCast<NWorld::CUnitServer> pCorpse( luaParams[ 1 ].p );
	if ( IsValid( pUS ) && IsValid( pCorpse ) && pUS->CanFight() && !pCorpse->CanFight() )
	{
		pUS->Do( new NWorld::CCmdSetCommand( pUS, new NWorld::CCmdTakeCorpse( ( NWorld::CUnit * )pCorpse ) ) );
		pUS->Do( new NWorld::CCmdSetCommand( pUS, new NWorld::CCmdContinue() ) );
	}
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( UnitDropCorpse, "u" )
	if ( CDynamicCast<NWorld::CUnitServer> pUS( luaParams[ 0 ].p ) )
	{
		pUS->Do( new NWorld::CCmdSetCommand( pUS, new NWorld::CCmdDropCorpse() ) );
		pUS->Do( new NWorld::CCmdSetCommand( pUS, new NWorld::CCmdContinue() ) );
	}
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( UnitMakeUnconscious, "u" )
	if ( CDynamicCast<NWorld::CUnitServer> pUS( luaParams[ 0 ].p ) )
		pUS->MakeUnconscious( VNULL3, true );
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( UnitActivateWeapon, "ub[true]" )
	if ( CDynamicCast<NWorld::CUnitServer> pUS( luaParams[ 0 ].p ) )
	{
		NWorld::ENeedActiveItem needActiveItem = luaParams[ 1 ].b ? NWorld::ITEM_ACTIVE : NWorld::ITEM_INACTIVE;
		pUS->Do( new NWorld::CCmdSetCommand( pUS, 
			new NWorld::CCmdPath( pUS->GetPosition().pos, NAI::PF_DEFAULT, needActiveItem ) ) );
		pUS->Do( new NWorld::CCmdSetCommand( pUS, new NWorld::CCmdContinue() ) );
	}
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( UnitPlaceInPocket, "u" )
	if ( CDynamicCast<NWorld::CUnitServer> pUS( luaParams[ 0 ].p ) )
	{
		if ( !pScript->pWorld->IsUnitInPocket( pUS ) )
		{
			pUS->SetState( new NWorld::CUnitStateInPocket( pUS ) );
			pUS->Update();
		}
	}
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( UnitRestoreFromPocket, "u" )
	if ( CDynamicCast<NWorld::CUnitServer> pUS( luaParams[ 0 ].p ) )
	{
		if ( pScript->pWorld->IsUnitInPocket( pUS ) )
		{
			pUS->SetState( new NWorld::CUnitStateNormal( pUS ) );
			pUS->Update();
		}
	}
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( UnitGetRoute, "u" )
	if ( CDynamicCast<NWorld::CUnitServer> pUS( luaParams[ 0 ].p ) )
	{
		if ( CDynamicCast<NAI::CAICommander> pCommander( pUS->GetPlayer()->GetCommander() ) )
		{
			CPtr<NAI::IAIUnit> pUnit( pCommander->GetAIUnit( pUS ) );
			if ( IsValid( pUnit ) )
			{
				luaPushCPtr( pState, pUnit->GetRoute() );
				return 1;
			}
		}
	}
	pScript->PushNil();
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( RouteIsFinished, "u" )
	bool bFinished = true;
	if ( CDynamicCast<NAI::CTask> pTask( luaParams[ 0 ].p ) )
		bFinished = pTask->IsEndOfTask();
	luaPushBool( pState, bFinished );
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( UnitAI, "u" )
	if ( CDynamicCast<NWorld::CUnitServer> pUS( luaParams[ 0 ].p ) )
	{
		if ( CDynamicCast<NAI::CAICommander> pCommander( pUS->GetPlayer()->GetCommander() ) )
		{
			CPtr<NAI::IAIUnit> pUnit( pCommander->GetAIUnit( pUS ) );
			if ( IsValid( pUnit ) )
				pUnit->GetAIInventory()->DebugOutput();
		}
	}
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
}
#include "stdafx.h"
#include "A5Script.h"
#include "scriptCommon.h"
#include "wMain.h"
#include "wInterface.h"
#include "wUICommands.h"
#include "wUnitServer.h"
#include "..\DBFormat\DataCamera.h"
#include "scriptPtr.h"
#include "rpgCheatConstants.h"
#include "rpgUnitMission.h"
#include "rpgUnit.h"
#include "wAnimation.h"
#include "..\MiscDll\LogStream.h"
#include "aiCommander.h"
#include "aiTacticalCommander.h"
//
#include "scriptSequence.h"
//
namespace NScript
{
////////////////////////////////////////////////////////////////////////////////////////////////////
static void SetSequenceCheat( NWorld::CWorld *pWorld, bool bOn )
{
	vector< CPtr<NWorld::CUnit> > units;
	pWorld->GetAllUnits( &units );
	for ( vector< CPtr<NWorld::CUnit> >::iterator i = units.begin(); i != units.end(); ++i )
	{
		if ( CDynamicCast<NWorld::CUnitServer> pUS( *i ) )
		{
			pUS->GetUnitRPG()->GetRPGUnit()->SetCheat( NRPG::CHEAT_SCRIPTSEQUENCE, bOn );
			pUS->animator.SetBreathOnlyIdle( bOn );
			if ( bOn )
				pUS->OnTBSEvent( NWorld::TBS_CANCEL_ACTION );
		}
	}
	if ( bOn )
	{
		// release all tactical commander units
		vector<CPtr<NWorld::CPlayer> > players;
		pWorld->GetPlayersList( &players );
		for ( vector< CPtr<NWorld::CPlayer> >::const_iterator i = players.begin(); i != players.end(); ++i )
		{
			if ( CDynamicCast<NAI::CAICommander> pCommander( (*i)->GetCommander() ) )
				pCommander->GetAITacticalCommander()->DismissAllUnits();
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( c_BeginSequence, "b[true]" );
	pScript->AddUICommand( new NWorld::CUICmdBeginSequence() );
	SetSequenceCheat( pScript->pWorld, true );
	pScript->pWorld->ForceRealTime( true );
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( EndSequencePart, "" );
	pScript->AddUICommand( new NWorld::CUICmdPartFinished() );
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( EndSequence, "" );
	pScript->AddUICommand( new NWorld::CUICmdPartFinished() );
	pScript->AddUICommand( new NWorld::CUICmdEndSequence() );
	SetSequenceCheat( pScript->pWorld, false );
	pScript->pWorld->ForceRealTime( false );
	pScript->pWorld->UpdateVisible();
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( IsInterfaceAction, "n[0]" );
	if ( pScript->IsInterfaceAction( ( NWorld::EInterfaceActionType )luaParams[ 0 ].n ) )
		pScript->PushNumber( 1 );
	else
		pScript->PushNil();
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( GetCamera, "n" );
	luaPushCDBPtr<NDb::CDBCamera>( pState, NDb::GetDBCamera( luaParams[ 0 ].n ) );
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( CameraMove, "un" );
	CDBPtr<NDb::CDBCamera> pDBCamera = luaGetDBPtr<NDb::CDBCamera>( pScript->GetObject( 1 ) );
	if ( !IsValid( pDBCamera ) )
		return 0;
	//
	STime transitionTime = ( STime )luaParams[ 1 ].n;
	ICamera::SCameraPos pos( pDBCamera->vAnchor,
		pDBCamera->fDistance, pDBCamera->fPitch, pDBCamera->fYaw, pDBCamera->fRoll, pDBCamera->fFOV );
	pScript->OnInterfaceActionStarted( NWorld::IAT_CAMERA );
	pScript->AddUICommand( new NWorld::CUICmdMoveCamera( pos, transitionTime ) );
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( uiShowStore, "" );
	pScript->AddUICommand( new NWorld::CUICmdShowStore() );
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( uiShowTeamMngMenu, "" );
	pScript->AddUICommand( new NWorld::CUICmdShowTeamMng() );
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( Floor, "n" );
	pScript->AddUICommand( new NWorld::CUICmdSetFloor( luaParams[ 0 ].n ) );
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
}
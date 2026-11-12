#include "StdAfx.h"
#include "wInterface.h"
#include "wMainTrace.h"
#include "wUICommands.h"
#include "Transform.h"
#include "DiscretePos.h"
#include "GView.h"
#include "G2DView.h"
#include "GSceneUtils.h"
#include "Sound.h"
#include "RWGame.h"
#include "RWSound.h"
#include "RPGGame.h"
#include "RPGGlobal.h"
#include "RPGUnit.h"
#include "RPGMerc.h"
#include "RPGUnitInfo.h"
#include "RPGItemInfo.h"
#include "..\MiscDll\Commands.h"
#include "..\MiscDll\LogStream.h"
#include "..\Misc\StrProc.h"
#include "..\Misc\BasicShare.h"
#include "..\Input\Bind.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataMap.h"
#include "..\DBFormat\DataRPG.h"
#include "..\DBFormat\DataSound.h"
#include "..\DBFormat\DataLight.h"
#include "iMain.h"
#include "iSaveManager.h"
#include "Interface.h"
#include "iCommonUI.h"
#include "iMission.h"
#include "iInterMission.h"
#include "iGlobalMap.h"
#include "iChapterMap.h"
#include "iSaveLoad.h"
#include "iInGameMenu.h"
#include "iLoseFake.h"
#include "iCluesMenu.h"
#include "iCharGen.h"
#include "iTeamMngMenu.h"
#include "iAIViewer.h"
#include "iRadTest.h"
#include "iGameStates.h"
#include "aiInterval.h"
#include "aiMap.h"
#include "iMissionUI.h"
#include "iMissionDlgUI.h"
#include "iMissionMovieUI.h"
#include "iMissionTrailerUI.h"
#include "iMissionInternal.h"
#include "iMissionExec.h"
#include "UnitTracker.h"
#include "PlayerTracker.h"
#include "MemObject.h"
#include "BSchemaViewer.h"
#include "MakeBuilding.h"
#include "MapBuildingInfo.h"
#include "scScenarioTracker.h"
#include "scFlowChartItems.h"
#include "scFlowChart.h"
#include "RPGDiplomacy.h"
#include "..\DBFormat\DataScenario.h"
#include "rpgCheatConstants.h"
#include "..\DBFormat\DataDifficulty.h"
#include "iShowClue.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGame
{
////////////////////////////////////////////////////////////////////////////////////////////////////
const int
	N_SCROLL_STEP				= 4,
	N_SCROLL_GUARDBAND	= 4,
	N_MAX_SKIPPED_STEPS = 1200;
////////////////////////////////////////////////////////////////////////////////////////////////////
static bool bShowAllCheat = false;
static float fDefaultCameraFOV = N_FOV;
static ICamera::SCameraLimits defaultCameraLimits( 10, 50, -1.5f, -0.1f, CTRect<float>( -1000, -1000, 1000, 1000 ) );
////////////////////////////////////////////////////////////////////////////////////////////////////
static void CommandSurrender( const string &szID, const vector<wstring> &paramsSet, void *pContext );
static void CommandTypeCameraParams( const string &szID, const vector<wstring> &paramsSet, void *pContext );
static void CommandSetXPLevel( const string &szID, const vector<wstring> &paramsSet, void *pContext );
static void CommandSummonUnit( const string &szID, const vector<wstring> &paramsSet, void *pContext );
static void CommandUnsummonUnit( const string &szID, const vector<wstring> &paramsSet, void *pContext );
static void CommandGetItem( const string &szID, const vector<wstring> &paramsSet, void *pContext );
////
static void VarCheatSeeAll( const string &szID, const NGlobal::CValue &sValue, void *pContext );
static void VarCheatGodMode( const string &szID, const NGlobal::CValue &sValue, void *pContext );
static void VarCheatTeleport( const string &szID, const NGlobal::CValue &sValue, void *pContext );
static void VarCheatAP( const string &szID, const NGlobal::CValue &sValue, void *pContext );
////////////////////////////////////////////////////////////////////////////////////////////////////
// CMission
////////////////////////////////////////////////////////////////////////////////////////////////////
CMission::CMission():
	bHideInterface( false ), bSpecialHideInterface( false ), bPause( false ), actionsInfoSet( UA_MAXVALUE ), nPanelsState( 0 ), nFramesSameCameraPosition(0), vPrevCameraPosition(0,0,0),
	fFOV( fDefaultCameraFOV ), eCameraType( CAMERA_PC ), nLightMode( 0 ), bFreezeCamera( false ), bTraceOk( false ),
	bCheatVisibility(false),
	bindCancel( "cancel" ),	bindCancelAction( "cancelaction" ),	bindContinue( "continue" ),	bindPause( "pause" ), bindMainMenu( "mainmenu" ), bindEndMission( "endmission" ), bindHideInterface( "hideinterface" ), bindSpecialHideInterface( "hideinterface_special" ), bindTrailerHideInterface( "hideinterface_trailer" ), 
	bindHero1( "hero_1" ), bindHero2( "hero_2" ), bindHero3( "hero_3" ), bindHero4( "hero_4" ), bindHero5( "hero_5" ), bindHero6( "hero_6" ), bindSelPrev( "hero_prev" ), bindSelNext( "hero_next" ),
	bindSnapShot( "weapon_snapshot" ), bindAimedShot( "weapon_aimedshot" ), bindCarefulShot( "weapon_carefulshot" ), bindShortBurst( "weapon_shortburst" ), bindLongBurst( "weapon_longburst" ), bindSnipeShot( "weapon_snipeshot" ), bindWeaponPrevMode( "weapon_prevmode" ), bindWeaponNextMode( "weapon_nextmode" ),
	bindGrenadeModeThrow( "grenade_throw" ), bindGrenadeModeSetTrap( "grenade_settrap" ),
	bindWeaponReload( "weapon_reload" ), bindPrevSlot( "slot_prev" ), bindNextSlot( "slot_next" ), bindItemUnload( "unload" ), bindItemArrange( "arrange" ),
	bindSwitchLighting("switch_lighting"),
	cmdTypeCamera( "camerapos", CommandTypeCameraParams, this ), 
	cmdSurrender( "surrender", CommandSurrender, this ), 
	cmdSummonUnit( "summonunit", CommandSummonUnit, this ), 
	cmdUnsummonUnit( "unsummonunit", CommandUnsummonUnit, this ), 
	cmdGetItem( "getitem", CommandGetItem, this ), 
	cmdSetXPLevel( "setxplevel", CommandSetXPLevel, this ),	
	varCheatGodMode( "godmode", VarCheatGodMode, this, NGlobal::CValue( L"off" ) ), 
	varCheatSeeAll( "seeall", VarCheatSeeAll, this, NGlobal::CValue( L"off" ) ), 
	varCheatTeleport( "teleport", VarCheatTeleport, this, NGlobal::CValue( L"off" ) ),
	varCheatAP( "cheat_ap", VarCheatAP, this, NGlobal::CValue( L"off" ) ),
	bindCluesMenu( "clues" ),
	bindStartOfTurn( "startofturn" ), bindEndOfTurn( "endofturn" ), 
	bindSaveMenu( "savemenu" ), bindLoadMenu( "loadmenu" ), 
	bindMove( "move" ), bindAttack( "attack" ), bindSetMine( "setmine" ), bindSetTrap( "settrap" ), bindFirstAid( "firstaid" ), bindDropCorpse( "dropcorpse" ), bindExitPK( "exitpk" ), bindRotate( "unit_rotate" ), 
	bindSnipeAttack( "snipe_attack" ), bindCollect1AP( "collectap_1ap" ), bindCollect10AP( "collectap_10ap" ), bindCollectMaxAP( "collectap_max" ), bindCollectAllAP( "collectap_all" ),
	bindNormalPose( "pose_normal" ), bindCrawlPose( "pose_crawl" ), bindCrouchPose( "pose_crouch" ), bindRunPose( "pose_run" ), bindStrafe( "pose_strafe" ), bindHide( "hide" ),
	bindFocusUnit( "focus_unit" ), bindAddFloor("next_floor"), bindSubFloor("prev_floor"),
	bindShadows("toggle_shadows"), bindFog("toggle_fog"), bindHSR("toggle_hsr"), 
	bindNextEnemy( "next_enemy" ), bindCheatTeleport( "cheat_teleport" ),bindExplode( "explode" ), bindShowAI( "showai" ), bindRPGStats( "rpg_stats" ), bindShowSchema( "viewSchema" ), bindShowParticles("switch_particles"), bindCheckBuildings( "checkStability" ), 
	bindShowVision( "rpg_vision" ), bindShowRAD( "showrad" ), bindShowVisibility("rpg_visibility"),
	bindTestIsect("test_intersect"), bindClickOfDeath("click_of_death"), bindExplosion("explosion"), bindErasePart("erase_part"),
	bindToggleTransp("toggle_transparent"), bindShowLightmap("toggle_lightmaps"),
	bindShowRain("test_rain"), bindShowSnow("test_snow")
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMission::Initialize( int _nTemplateID, int _nVariantID, NScenario::CScenarioZone *_pZone, const vector<string> &params, NRPG::CGlobalGame *_pGlobalGame )
{
	pZone = _pZone;
	nTemplateID = _nTemplateID;
	nVariantID = _nVariantID;
	pGlobalGame = _pGlobalGame;

	int nMobsLevel = 0;
	SRandomSeed sSeed;
	list< CPtr<NScenario::CScenarioClue> > clues;
	if ( IsValid( pZone ) && pGlobalGame->pScenarioTracker->IsScenarioAvailable() )
	{
		if ( nTemplateID == -1 )
			nTemplateID = pZone->GetDefaultTemplateID();
		pGlobalGame->pScenarioTracker->GetPlacedClues( pZone, nTemplateID, &clues );
		pZone->SetPassed();
		pGlobalGame->pCurrentZone = pZone;
		pGlobalGame->nCurrentTemplateID = nTemplateID;
		sSeed = pZone->GetRandomSeedForTemplate( nTemplateID );
		nMobsLevel = pZone->GetDifficulty();
	}
	else
	{
		// вычисляем сложность random encounter-а
		int nDelta = 0;
		for ( int i = 0; i < 10; ++i )
			nDelta += random.Get( 0, Max( 1, 2 * pGlobalGame->pDifficulty->nREDifficulty ) );
		nDelta /= 10; nDelta -= pGlobalGame->pDifficulty->nREDifficulty;
		nMobsLevel = pGlobalGame->nCurrentChapterDifficulty + nDelta;
	}

	if ( nVariantID == -1 )
	{
		CPtr<NDb::CTemplate> pTemplate = NDb::GetTemplate( nTemplateID );
		if ( !IsValid( pTemplate ) )
			return false;

		SRand sRand( sSeed );
		vector<int> dummyTemp;
		nVariantID = NDb::GetTemplVariant( pTemplate, dummyTemp, -1, &sRand )->GetRecordID();
	}

	if ( IsValid( pZone ) )
		LoadWorld( NStr::Format( "%d.sav", pZone->GetDBZone()->GetRecordID() ) );

	CObj<NWorld::CPostWorldCreateInfo> pPostInfo;
	if ( !IsValid( pWorld ) )
	{
		pWorld = NWorld::CreateWorld( _pGlobalGame );
		pWorld->CreateRandom( nVariantID, params, true, clues, 
			nMobsLevel, &pPostInfo, sSeed );
	}
	else
		pWorld->CreateRestored();

	NDb::CMusic *pAmbientMelody = NDb::GetMusic( 1 );
	pCombatMelody  = NDb::GetMusic( 3 );
	NDb::CTemplVariant *pVar = NDb::GetTemplVariant( nVariantID );
	if ( pVar )
	{
		if ( IsValid( pVar->pAmbientMusic ) )
			pAmbientMelody = pVar->pAmbientMusic;
		if ( IsValid( pVar->pCombatMusic ) )
			pCombatMelody  = pVar->pCombatMusic;
	}

	pScene = NGScene::CreateNewView();
	pSoundScene = NSound::CreateSoundScene( pAmbientMelody );
	pRender = NRender::CreateRenderGame( pWorld, pScene );
	pRenderSound = NRender::CreateRenderSound( pWorld, pSoundScene );
#ifdef _MAPEDIT
	pCursor = NUI::ICursor::CreateEditorCursor();
#else
	pCursor = NUI::ICursor::Create( true, NGfx::GetScreenRect() / 2 );
#endif
	pInterface = new NUI::CInterface( pCursor, pSoundScene );

	playersSet.resize( pGlobalGame->players.size() );
	for ( int nTemp = 0; nTemp < pGlobalGame->players.size(); nTemp++ )
	{
		WCHAR wsString[1024];
		swprintf( wsString, L"Player %d", nTemp );
		playersSet[nTemp] = new CPlayerTracker( this, pGlobalGame->players[nTemp], wsString );
	}
	pActivePlayer = playersSet.front();
	CommandState( new CStateEmpty );

	TrackChanges();
	bUpdated = false;
	bForceUpdateNextFrame = true;

	NDb::CAmbientLightReal *pLight = GetWorld()->GetDefaultLight();
	if ( pLight )
		GetScene()->SetAmbient( pLight );
	else
		SetLightMode( 0 );

	SetCameraParams( CAMERA_PC, fFOV, defaultCameraLimits );
	pCamera->SetPlacement( GetActivePlayer()->GetCamera() );

	TraceCursor();

	bWaitForPartFinished = false;
	pMissionUI = new NUI::CMissionUI( NUI::SWindowInfo( pInterface, NUI::SPoint( 0, 0 ), NUI::SPoint( 1024, 768 ), "missionUI" ), this );
	NUI::LoadTemplate( pMissionUI, NDb::GetUIContainer( 123 ) );
	pMissionUI->ShowWindow( NUI::SWTYPE_SHOW );

	vector<CObj<IState> > updatedStatesSet;
	updatedStatesSet.push_back( new CStateWait() );
	updatedStatesSet.push_back( new CStateDragItem() );
	updatedStatesSet.push_back( new CStateUntrap() );
	updatedStatesSet.push_back( new CStateUse() );
	updatedStatesSet.push_back( new CStateTeam() );
	updatedStatesSet.push_back( new CStateFriend() );
	updatedStatesSet.push_back( new CStateAttack( false ) );
	updatedStatesSet.push_back( new CStatePickItem() );
	updatedStatesSet.push_back( new CStateMove( false ) );
	updatedStatesSet.push_back( new CStateEmpty() );
	SetUpdatedStates( updatedStatesSet );

	pWorld->RunPostInit( pPostInfo );

	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMission::Terminate()
{
	if ( IsValid( pZone ) )
	{
		for ( int nTemp = 0; nTemp < playersSet.size(); nTemp++ )
			pWorld->RemovePlayer( playersSet[nTemp]->GetPlayer() );

		SaveWorld( NStr::Format( "%d.sav", pZone->GetDBZone()->GetRecordID() ) );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMission::Command( NWorld::CCommand *pCmd )
{
	bForceUpdateNextFrame = true;

	ASSERT( pCmd );
	pActivePlayer->GetCommander()->Do( pCmd );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMission::Command( NWorld::CUnit *pUnit, NWorld::CCmd *pCmd, bool bInstantly )
{
	bForceUpdateNextFrame = true;

	ASSERT( pUnit );
	ASSERT( pCmd );
	Command( new NWorld::CCmdSetCommand( pUnit, pCmd ) );

	if ( bInstantly )
		Command( new NWorld::CCmdSetCommand( pUnit, new NWorld::CCmdContinue ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMission::StopAction()
{
	pActivePlayer->GetCommander()->StopAction();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMission::IsReady() const
{
	if ( !IsRealTime() && ( IsActionExecuted() || ( pWorld->GetCurrentPlayer() != pActivePlayer->GetPlayer() ) ) )
		return false;

	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMission::IsActionExecuted() const
{
	return pWorld->IsExecuting() || pActivePlayer->GetCommander()->HasCommands();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMission::IsRealTime() const
{
	return pWorld->GetCurrentPlayer() == 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMission::PauseGame( bool bState )
{
	bPause = bState;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMission::IsGamePaused() const
{
	return bPause;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
IPlayerTracker* CMission::GetActivePlayer() const
{
	ASSERT( pActivePlayer );
	return pActivePlayer;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMission::SetActivePlayer( IPlayerTracker* pPlayer )
{
	ASSERT( find( playersSet.begin(), playersSet.end(), pPlayer ) != playersSet.end() );
	pActivePlayer = pPlayer;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMission::GetPlayers( vector< CPtr<IPlayerTracker> > *pPlayersSet ) const
{
	ASSERT( pPlayersSet );
	pPlayersSet->clear();
	pPlayersSet->resize( playersSet.size() );
	for ( int nTemp = 0; nTemp < playersSet.size(); nTemp++ )
		(*pPlayersSet)[nTemp] = playersSet[nTemp].GetPtr();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMission::GetUnits( vector< CPtr<IUnitTracker> > *pUnits ) const
{
	pActivePlayer->GetUnits( pUnits );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMission::GetSelectedUnits( vector< CPtr<IUnitTracker> > *pUnits ) const
{
	pActivePlayer->GetSelectedUnits( pUnits );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CMission::CountSelected()
{
	return pActivePlayer->CountSelected();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMission::Select( NWorld::CUnit *pUnit, bool bAdditive )
{
	pActivePlayer->Select( pUnit, bAdditive );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMission::SelectNext()
{
	pActivePlayer->SelectNext();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMission::SelectPrev()
{
	pActivePlayer->SelectPrev();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMission::IsUpdated() const
{
	return bUpdated;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
IState* CMission::GetState() const
{
	return pState;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMission::CommandState( IState *pNewState )
{
	CObj<IState> pHold( pNewState );

	if ( IsValid( pState ) && ( pState->GetType() == IState::FORCED ) )
		return false;

	if ( !pNewState->Initialize( this ) )
		return false;

	if ( IsValid( pState ) )
		pState->Terminate();

	pState = pNewState;
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMission::ResetState()
{
	pState->Terminate();
	pState = 0;
	CommandState( new CStateEmpty );
	UpdateState();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMission::SetUpdatedStates( const vector<CObj<IState> > &_updatedStatesSet )
{
	updatedStatesSet = _updatedStatesSet;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CObjectBase* CMission::GetStateTarget() const
{
	if ( IsValid( pStateTarget ) )
		return pStateTarget;

	return pTraceObject;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMission::SetStateTarget( CObjectBase* pObject )
{
	pStateTarget = pObject;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CMission::GetUnitsState()
{
	vector< CPtr<NGame::IUnitTracker> > unitsSet;
	GetSelectedUnits( &unitsSet );

	if ( unitsSet.size() != 1 )
		return N_UNITSTATE_DEFAULT;

	switch ( unitsSet.front()->GetUnit()->GetState() )
	{
	case NWorld::CUnit::ST_NORMAL_KNIFE:
	case NWorld::CUnit::ST_NORMAL_RIFLE:
	case NWorld::CUnit::ST_NORMAL_MELEE:
	case NWorld::CUnit::ST_NORMAL_PISTOL:
	case NWorld::CUnit::ST_NORMAL_GRENADE:
	case NWorld::CUnit::ST_NORMAL_DEFAULT:
	case NWorld::CUnit::ST_NORMAL_SUB_MACHINE_GUN:
	case NWorld::CUnit::ST_NORMAL_HAND_MACHINE_GUN:
	case NWorld::CUnit::ST_NORMAL_RLAUNCHER:
	case NWorld::CUnit::ST_NORMAL_MEDKIT:
	case NWorld::CUnit::ST_NORMAL_MINE:
	case NWorld::CUnit::ST_HEALER:
	case NWorld::CUnit::ST_CARRY_CORPSE:
		return N_UNITSTATE_DEFAULT;
	case NWorld::CUnit::ST_MACHINE_GUN:
		return N_UNITSTATE_CANNON;
	case NWorld::CUnit::ST_SNIPE:
		return N_UNITSTATE_SNIPE;
	default:
		ASSERT( 0 );
	}

	return N_UNITSTATE_DEFAULT;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NWorld::CUnit::EState CMission::GetUnitsWorldState()
{
	vector< CPtr<NGame::IUnitTracker> > unitsSet;
	GetSelectedUnits( &unitsSet );

	if ( unitsSet.size() != 1 )
		return NWorld::CUnit::ST_NORMAL_DEFAULT;

	return unitsSet.front()->GetUnit()->GetState();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMission::GetActionInfo( EUnitAction eAction, SActionInfo *pInfo )
{
	*pInfo = actionsInfoSet[eAction];
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMission::CanDoCommand( NWorld::CCmd *pCmd, bool bNoTarget, SActionInfo *pInfo )
{
	pInfo->eResult = CanDoCommand( pCmd, bNoTarget, &pInfo->nActionAP );

	pInfo->bOk = false;
	pInfo->bEnoughAP = true;
	pInfo->bAvailable = false;
	switch( pInfo->eResult )
	{
	case NWorld::UCR_UNAVAILABLE:
	case NWorld::UCR_INVALID_COMMAND:
		pInfo->bOk = false;
		pInfo->bAvailable = false;
		break;
	case NWorld::UCR_GENERAL_FAILURE:
		pInfo->bOk = false;
		pInfo->bAvailable = true;
		break;
	case NWorld::UCR_OK:
		pInfo->bOk = true;
		pInfo->bEnoughAP = true;
		pInfo->bAvailable = true;
		break;
	case NWorld::UCR_NO_TARGET:
		pInfo->bOk = bNoTarget;
		pInfo->bEnoughAP = true;
		pInfo->bAvailable = true;
		break;
	case NWorld::UCR_NOT_ENOUGH_AP:
		pInfo->bOk = true;
		pInfo->bEnoughAP = false;
		pInfo->bAvailable = true;
		break;
	case NWorld::UCR_PATH_NOT_FOUND:
	case NWorld::UCR_NEED_RELOAD:
	case NWorld::UCR_NO_EQUIPMENT:
	case NWorld::UCR_WEAPON_JAMMED:
	case NWorld::UCR_CRITICALS_BAN:
	case NWorld::UCR_TARGET_OUT_OF_RANGE:
		pInfo->bOk = false;
		pInfo->bEnoughAP = true;
		pInfo->bAvailable = true;
		break;
	default:
		ASSERT( 0 );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NWorld::EUnitCommandResult CMission::CanDoCommand( NWorld::CCmd *pCmd, bool bNoTarget, int *pnAP )
{
	CObj<NWorld::CCmd> pHolder( pCmd );
	vector< CPtr<IUnitTracker> > unitsSet;
	GetSelectedUnits( &unitsSet );

	if ( unitsSet.size() == 0 )
		return NWorld::UCR_GENERAL_FAILURE;

	NWorld::EUnitCommandResult eTotalRes = NWorld::UCR_OK;
	for ( vector< CPtr<IUnitTracker> >::iterator iTemp = unitsSet.begin(); iTemp != unitsSet.end(); iTemp++ )
	{
		CPtr<NWorld::CUnit> pUnit = (*iTemp)->GetUnit();

		if ( bNoTarget )
		{
			if ( CDynamicCast<NWorld::CCmdPath> pMove( pCmd ) )
			{
				if ( pMove->eParams != NAI::PF_USE_DIR )
					pMove->ptDst = (*iTemp)->GetTargetPosition();
				else
					pMove->ptDst = pUnit->GetPosition().pos;
			}
			else if ( CDynamicCast<NWorld::CCmdLook> pLook( pCmd ) )
				pLook->ptDst = pUnit->GetPosition().pos;
			else if ( CDynamicCast<NWorld::CCmdSetMineOnTile> pMine( pCmd ) )
				pMine->ptDst = pUnit->GetPosition().pos;
		}

		int nTemp;
		NWorld::EUnitCommandResult eRes = pUnit->CanDo( pCmd, &nTemp, pnAP );
		if ( eRes == NWorld::UCR_OK )
			continue;

		return eRes;
	}

	if ( pnAP && unitsSet.size() != 1 )
		*pnAP = -1;

	return eTotalRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMission::UpdateActionsInfo()
{
	actionsInfoSet[UA_DEFAULT].eResult = NWorld::UCR_OK;
	actionsInfoSet[UA_DEFAULT].nActionAP = 0;
	actionsInfoSet[UA_DEFAULT].bOk = true;
	actionsInfoSet[UA_DEFAULT].bEnoughAP = true;
	actionsInfoSet[UA_DEFAULT].bAvailable = true;
	
	actionsInfoSet[UA_STOP].eResult = NWorld::UCR_OK;
	actionsInfoSet[UA_STOP].nActionAP = 0;
	actionsInfoSet[UA_STOP].bOk = true;
	actionsInfoSet[UA_STOP].bEnoughAP = true;
	actionsInfoSet[UA_STOP].bAvailable = true;
	
	CanDoCommand( new NWorld::CCmdContinue(), true, &actionsInfoSet[UA_CONTINUE] );
	CanDoCommand( new NWorld::CCmdReload(), true, &actionsInfoSet[UA_WEAPONRELOAD] );

	CanDoCommand( new NWorld::CCmdPath( NAI::SPosition(), NAI::PF_DEFAULT ), true, &actionsInfoSet[UA_MOVE] );
	CanDoCommand( new NWorld::CCmdLook( NAI::SPosition() ), true, &actionsInfoSet[UA_LOOK] );

	actionsInfoSet[UA_USE].eResult = NWorld::UCR_OK;
	actionsInfoSet[UA_USE].nActionAP = 0;
	actionsInfoSet[UA_USE].bOk = true;
	actionsInfoSet[UA_USE].bEnoughAP = true;
	actionsInfoSet[UA_USE].bAvailable = true;

	CanDoCommand( new NWorld::CCmdShootObject( 0, 0 ), true, &actionsInfoSet[UA_ATTACK] );

	CanDoCommand( new NWorld::CCmdHeal( 0 ), true, &actionsInfoSet[UA_HEAL] );
	CanDoCommand( new NWorld::CCmdSetMineOnTile( NAI::SPosition() ), true, &actionsInfoSet[UA_MINE] );
	CanDoCommand( new NWorld::CCmdDropCorpse(), true, &actionsInfoSet[UA_DROPCORPSE] );
	CanDoCommand( new NWorld::CCmdExitPK(), true, &actionsInfoSet[UA_EXITPK] );
	CanDoCommand( new NWorld::CCmdHide(), true, &actionsInfoSet[UA_HIDE] );

	CanDoCommand( new NWorld::CCmdStrafe( true ), true, &actionsInfoSet[UA_STRAFE] );
	CanDoCommand( new NWorld::CCmdWishPose( NAI::RUN ), true, &actionsInfoSet[UA_POSERUN] );
	CanDoCommand( new NWorld::CCmdWishPose( NAI::WALK ), true, &actionsInfoSet[UA_POSEWALK] );
	CanDoCommand( new NWorld::CCmdWishPose( NAI::CROUCH ), true, &actionsInfoSet[UA_POSECROUCH] );
	CanDoCommand( new NWorld::CCmdWishPose( NAI::CRAWL ), true, &actionsInfoSet[UA_POSECRAWL] );

	CanDoCommand( new NWorld::CCmdSnipeAttack(), true, &actionsInfoSet[UA_SNIPE_ATTACK] );
	CanDoCommand( new NWorld::CCmdCollectSnipeAP( NWorld::CSAP_1AP ), true, &actionsInfoSet[UA_COLLECTAP_1AP] );
	CanDoCommand( new NWorld::CCmdCollectSnipeAP( NWorld::CSAP_10AP ), true, &actionsInfoSet[UA_COLLECTAP_10AP] );
	CanDoCommand( new NWorld::CCmdCollectSnipeAP( NWorld::CSAP_MAX ), true, &actionsInfoSet[UA_COLLECTAP_MAX] );
	CanDoCommand( new NWorld::CCmdCollectSnipeAP( NWorld::CSAP_ALL ), true, &actionsInfoSet[UA_COLLECTAP_ALL] );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
ICamera* CMission::GetCamera() const
{
	return pCamera;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
float CMission::GetCameraFOV() const
{
	return fFOV;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMission::GetCameraParams( ECameraType *pType, float *pFOV, ICamera::SCameraLimits *pLimits )
{
	*pFOV = fFOV;
	*pType = eCameraType;
	*pLimits = cameraLimits;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMission::SetCameraParams( ECameraType eType, float _fFOV, const ICamera::SCameraLimits &limits )
{
	CPtr<ICamera> pNewCamera;

	fFOV = _fFOV;
	eCameraType = eType;
	cameraLimits = limits;
	cameraLimits.sZoneLimit = GetWorld()->GetMapSafeZone();

	pNewCamera = CreateCamera( eType );
	if ( !IsValid( pNewCamera ) )
	{
		ASSERT( 0 );
		return;
	}
	ICamera::SCameraPos sPos;
	if ( pCamera )
	{
		pCamera->GetPlacement( &sPos );
		pNewCamera->SetPlacement( sPos );
	}
	pNewCamera->SetLimits( cameraLimits );
	pCamera = pNewCamera;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMission::FreezeCamera( bool bState )
{
	bFreezeCamera = bState;
	if ( bState )
		pCamera->GetPlacement( &sFreezePose );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMission::FocusCameraOnUnit( NWorld::CUnit *pUnit )
{
	ICamera::SCameraPos sPos;
	pCamera->GetPlacement( &sPos );
	sPos.ptAnchor = pUnit->GetPosition().pos.GetCP();
	pCamera->SetPlacement( sPos );
	GetScene()->SetCutFloor( pUnit->GetPosition().pos.GetFloor() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CMission::GetCutFloor()
{
	return GetScene()->GetCutFloor();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMission::SetCutFloor( int nFloor )
{
	GetScene()->SetCutFloor( nFloor );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const CVec3& CMission::GetCameraCP() const
{
	return vCameraCP;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const SHMatrix& CMission::GetCameraPos() const
{
	return sCameraPos;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const CTransformStack& CMission::GetCameraTransform() const
{
	return sTransform;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CObjectBase* CMission::GetTraceObject() const
{
	return pTraceObject;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMission::GetTracePosition( CVec3 *pPos ) const
{
	int nMaxFloor = GetScene()->GetCutFloor();
	vector<NAI::SInterval> intervals;
	pWorld->GetAIMap()->Trace( rTraceRay, &intervals, NWorld::TS_PASS_BLOCKER, NAI::CFloorsSet() );
	for ( int k = 0; k < intervals.size(); ++k )
	{
		NAI::SInterval &interv = intervals[k];
		if ( interv.enter.fT < 0 )
			continue;
		if ( interv.pSrc->nFloor > nMaxFloor )
			continue;
		*pPos = rTraceRay.Get( intervals[k].enter.fT );
		return true;
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMission::GetTracePosition( NAI::SPosition *pPos ) const
{
	*pPos = sTraceTile;
	return bTraceOk;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CRay CMission::GetTraceRay() const
{
	CRay res;
	CTransformStack sTS = GetCameraTransform();
	MakeProjectiveRay( &res.ptDir, &res.ptOrigin, &sTS, GetScene()->GetScreenRect(), GetCursor()->GetPos() );
	return res;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NUI::ICursor* CMission::GetCursor() const
{
	return pCursor;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NUI::CInterface* CMission::GetInterface() const
{
	return pInterface;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMission::IsInterfaceHidden() const
{
	return bHideInterface;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMission::SetWaitForPartFinished( bool bState )
{
	bWaitForPartFinished = bState;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMission::IsWaitForPartFinished() const
{
	return bWaitForPartFinished;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMission::PopDesktop( NUI::CDesktopWindow *pDesktop )
{
	for ( int nTemp = 0; nTemp < desktopWindowsList.size(); nTemp++ )
	{
		NUI::CDesktopWindow *pTempWnd = desktopWindowsList.back();
		desktopWindowsList.pop_back();

		if ( pTempWnd == pDesktop )
			break;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMission::PushDesktop( NUI::CDesktopWindow *pDesktop )
{
	desktopWindowsList.push_back( pDesktop );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NUI::CDesktopWindow* CMission::GetDesktop() const
{
	if ( !desktopWindowsList.empty() )
		return desktopWindowsList.back();

	return pMissionUI;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
EActionIconsSet CMission::GetActionIconsSet() const
{
	return eActionIconsSet;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMission::SetActionIconsSet( EActionIconsSet eMode )
{
	eActionIconsSet = eMode;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CMission::GetPanelState( int nMask ) const
{
	return nPanelsState & nMask;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMission::SetPanelState( int nMask, bool bState )
{
	if ( bState )
		nPanelsState |= nMask;
	else
		nPanelsState &= ~nMask;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMission::SetCheatVisibility( bool bState )
{
	bCheatVisibility = bState;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMission::GetCheatVisibility() const
{
	return bCheatVisibility;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NRPG::CGlobalGame* CMission::GetRPGGame() const
{
	return pGlobalGame;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NWorld::IWorld* CMission::GetWorld() const
{
	return pWorld;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NGScene::IGameView* CMission::GetScene() const
{
	return pScene;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NSound::ISoundScene* CMission::GetSoundScene() const
{
	return pSoundScene;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NRender::IRenderGame* CMission::GetRenderGame() const
{
	return pRender;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMission::Step()
{
	if ( CanRender() )
	{
		int nStepCount = 0;
		do
		{
			pRender->UpdateViewWorld( !bPause, GetTime(), pActivePlayer->GetPlayer(), bShowAllCheat || bCheatVisibility );

			InternalStep();
			nStepCount++;

			if ( bWaitForPartFinished )
			{
				MarkNewDGFrame();
				SetDeltaTime( GetDeltaTime() + 50 );
			}
			if ( nStepCount > N_MAX_SKIPPED_STEPS )
			{
				bWaitForPartFinished = false;
				csGame << CC_RED << "ERROR: Script skip-part execute more than" << N_MAX_SKIPPED_STEPS << " steps!" << endl;
			}
		} while ( bWaitForPartFinished );

		NUI::CWindow *pClientWindow = GetDesktop()->GetClientWindow();

		NUI::SRect sScrWindow;
		NUI::SPoint sScrPosition;
		const NUI::SPoint &sScrSize = pClientWindow->GetSize();
		pClientWindow->ClientToScreen( &sScrPosition, &sScrWindow );

		NUI::SRect sScrClientRect( sScrPosition.x, sScrPosition.y, sScrPosition.x + sScrSize.x, sScrPosition.y + sScrSize.y );
		GetCamera()->SetScreenRect( CTRect<float>( float( sScrClientRect.x1 ) / 1024.0f, float( sScrClientRect.y1 ) / 768.0f, float( sScrClientRect.x2 ) / 1024.0f, float( sScrClientRect.y2 ) / 768.0f ) );

		int nFlags = N_RENDERMODE_3D;
		if ( !bHideInterface )
			nFlags |= N_RENDERMODE_2D;

		RenderFrame( nFlags, GetTime(), GetCamera() );
	}
	else
	{
		pRender->ResetTiming();
		pRenderSound->ResetTiming();
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMission::InternalStep()
{
	if ( ( playersSet.size() > 1 ) && !IsRealTime() && ( pActivePlayer->GetPlayer() != pWorld->GetCurrentPlayer() ) )
	{
		ICamera::SCameraPos sPosition;
		pCamera->GetPlacement( &sPosition );
		pActivePlayer->SetCamera( sPosition );
		pActivePlayer->Deactivate();

		for ( vector< CObj<IPlayerTracker> >::iterator iPlayer = playersSet.begin(); iPlayer != playersSet.end(); iPlayer++ )
		{
			if ( (*iPlayer)->GetPlayer() == pWorld->GetCurrentPlayer() )
			{
				pActivePlayer = (*iPlayer);
				pCamera->SetPlacement( pActivePlayer->GetCamera() );
			}
		}
		pActivePlayer->Activate();
	}

	ExecWorldCommands();
	if ( IsValid( pCmdExec ) )
	{
		if ( pCmdExec->Update( GetTime() ) )
		{
			pCmdExec->Finished();
			pCmdExec = 0;
		}
	}

	pCamera->Update( GetTime() );

	if ( bFreezeCamera )
		pCamera->SetPlacement( sFreezePose );

	vCameraCP = pCamera->GetCP();
	sCameraPos = pCamera->GetPos();
	pCamera->GetTransform( &sTransform, GetScene()->GetScreenRect() );
	if ( vPrevCameraPosition != vCameraCP )
	{
		vPrevCameraPosition = vCameraCP;
		nFramesSameCameraPosition = 0;
	}
	else
		++nFramesSameCameraPosition;

	for ( vector< CObj<IPlayerTracker> >::iterator iPlayer = playersSet.begin(); iPlayer != playersSet.end(); iPlayer++ )
		(*iPlayer)->Update( (*iPlayer)->GetPlayer() == pWorld->GetCurrentPlayer() );

	if ( pActivePlayer->IsPlayerLoser() )
		NMainLoop::Command( new CICLoseMenu );

	pInterface->UpdateCursor();

	bTraceOk = false;
	if ( IsReady() )
	{
		TraceCursor();

		bUpdated = TrackChanges() || bForceUpdateNextFrame;
		bForceUpdateNextFrame = false;

		if ( bUpdated )
			UpdateActionsInfo();
	}
	else
	{
		bUpdated = TrackChanges();
		bForceUpdateNextFrame = true;

		for( int nTemp = 0; nTemp < actionsInfoSet.size(); nTemp++ )
			actionsInfoSet[nTemp].bOk = false;

		actionsInfoSet[UA_STOP].eResult = NWorld::UCR_OK;
		actionsInfoSet[UA_STOP].nActionAP = 0;
		actionsInfoSet[UA_STOP].bOk = true;
		actionsInfoSet[UA_STOP].bEnoughAP = true;
		actionsInfoSet[UA_STOP].bAvailable = true;
	}

	if ( ( pState->GetType() != IState::TEMPORARY ) && ( IsUpdated() || ( pState->GetType() == IState::INSTANT ) ) )
		UpdateState();

	pState->Step();

	UpdateSound();

	pStateTarget = 0;
	pInterface->Step( GetTime() );
	GetDesktop()->UpdateDesktop( GetTime() );

	bHasCommands |= pActivePlayer->GetCommander()->HasCommands();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMission::OnGetFocus()
{
	pRender->ResetTiming();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMission::ProcessEvent( const NInput::SEvent &sEvent )
{
	NInput::SetSection( "game" );

	pCursor->ProcessEvent( sEvent );

	if ( bindHideInterface.ProcessEvent( sEvent ) )
	{
		bHideInterface = !bHideInterface;
		return true;
	}
	else if ( bindSpecialHideInterface.ProcessEvent( sEvent ) )
	{
		bSpecialHideInterface = !bSpecialHideInterface;
		return true;
	}
	else if ( bindPause.ProcessEvent( sEvent ) )
	{
		bPause = !bPause;
		return true;
	}

	if ( bindMainMenu.ProcessEvent( sEvent ) )
	{
		NMainLoop::Command( new CICInGameMenu( GetActivePlayer()->GetGlobalPlayer() ) );
		return true;
	}
	else if ( bindSaveMenu.ProcessEvent( sEvent ) )
	{
		NMainLoop::Command( new CICSaveLoadMenu( SAVE ) );
		return true;
	}
	else if ( bindLoadMenu.ProcessEvent( sEvent ) )
	{
		NMainLoop::Command( new CICSaveLoadMenu( LOAD ) );
		return true;
	}
	else if ( bindCluesMenu.ProcessEvent( sEvent ) )
	{
		NMainLoop::Command( new CICClues( GetRPGGame() ) );
//		NMainLoop::Command( new CICTeamMngMenu( pActivePlayer->GetGlobalPlayer(), this ) );
//		pMissionDlgUI->ShowDialog();
		return true;
	}

	if ( pInterface->ProcessEvent( sEvent ) )
		return true;

/// SYSTEM( No unit or player allowed! )
	if ( bindShadows.ProcessEvent( sEvent ) )
		pScene->SetNextShadowsMode();
	else if (	bindToggleTransp.ProcessEvent( sEvent ) )
		pScene->SetNextTranspRenderMode();
	else if ( bindShowLightmap.ProcessEvent( sEvent ) )
		pScene->SetNextLightmapViewMode();
	else if ( bindHSR.ProcessEvent( sEvent ) )
		pScene->SetNextHSRMode();
	else if ( bindFog.ProcessEvent( sEvent ) )
		pScene->SetNextFogMode();
	else if ( bindAddFloor.ProcessEvent( sEvent ) )
		pScene->SetCutFloor( pScene->GetCutFloor() + 1 );
	else if ( bindSubFloor.ProcessEvent( sEvent ) )
		pScene->SetCutFloor( pScene->GetCutFloor() - 1 );
	else if ( bindShowParticles.ProcessEvent( sEvent ) )
		pScene->SetParticleShow( !pScene->GetParticleShow() );
	else if ( bindShowAI.ProcessEvent( sEvent ) )
	{
		ICamera::SCameraPos cameraPos;
		pCamera->GetPlacement( &cameraPos );
		NMainLoop::Command( new CICAIView( 
			pWorld->GetAIMap(), 
			pWorld->GetPathNetwork(), 
			pWorld->GetGame()->GetVisionTracker(), 
			cameraPos ) );
		return true;
	}
	else if ( bindShowRAD.ProcessEvent( sEvent ) )
	{
		ICamera::SCameraPos cameraPos;
		pCamera->GetPlacement( &cameraPos );
		NMainLoop::Command( new CICRadTest( pScene, cameraPos ) );
		return true;
	}
	else if ( bindShowVisibility.ProcessEvent( sEvent ) )
		ShowVisibleFrom();
	else if ( bindShowVision.ProcessEvent( sEvent ) )
	{
		if ( !pVisibleTracker )
		{
			pVisibleTracker = new CVisibleTracker( CVisibleTracker::VISIBLE );
			pVisibleTracker->Update( this );
		}
		else
			pVisibleTracker = 0;
	}
	else if ( bindShowSchema.ProcessEvent( sEvent ) )
	{
		if ( !buildingSchemas.empty() )
		{
			buildingSchemas.clear();
			return true;
		}

		CViewBuildingScheme b( pWorld->GetActive(), pScene, &buildingSchemas );
		b.Sync();
	}
	else if ( bindCheckBuildings.ProcessEvent( sEvent ) )
	{
		CUpdateBuildingStability b( pWorld->GetActive() );
		b.Sync();
	}
	else if ( bindTestIsect.ProcessEvent( sEvent ) )
		TestIntersection();
	else if ( bindShowRain.ProcessEvent( sEvent ) )
		ShowWeatherEffect( 817 );
	else if ( bindShowSnow.ProcessEvent( sEvent ) )
		ShowWeatherEffect( 732 );
	else if ( bindClickOfDeath.ProcessEvent( sEvent ) )
		ClickOfDeath();
	else if ( bindExplosion.ProcessEvent( sEvent ) )
		Explosion();
	else if ( bindErasePart.ProcessEvent( sEvent ) )
		ErasePartUnderCursor();
	else if ( bindRPGStats.ProcessEvent( sEvent ) )
	{
		vector< CPtr<IPlayerTracker> > playersSet;
		GetPlayers( &playersSet );
		for ( int i = 0; i < playersSet.size(); ++i )
		{
			if ( !IsValid( playersSet[i] ) )
				continue;
			csRPG << "<color=cyan>" << "PLAYER " << i << ":\n";
			vector< CPtr<IUnitTracker> > units;
			playersSet[i]->GetUnits( &units );
			for ( int j = 0; j < units.size(); ++j )
			{
				if ( !IsValid( units[j] ) || !IsValid( units[j]->GetUnit() ) )
					continue;
				csRPG << "<color=green>" << "Unit " << j << ":";
				units[j]->GetUnit()->GetRPG()->DumpStats();
			}
		}
		csRPG << "\n";
		return true;
	}
	else if ( bindExplode.ProcessEvent( sEvent ) )
	{
		vector< CPtr<NGame::IUnitTracker> > unitsSet;
		GetSelectedUnits( &unitsSet );
		for ( vector< CPtr<NGame::IUnitTracker> >::iterator iTemp = unitsSet.begin(); iTemp != unitsSet.end(); iTemp++ )
			Command( (*iTemp)->GetUnit(), new NWorld::CCmdExplode );
		return true;
	}

	if ( bindSwitchLighting.ProcessEvent( sEvent ) )
		SetLightMode( nLightMode + 1 );

	if ( IsReady() )
	{
		if ( pState->ProcessEvent( sEvent ) )
			return true;
	}

	if ( GetDesktop()->ProcessEvent( sEvent ) )
		return true;

	if ( IsRealTime()
		|| GetWorld()->GetCurrentPlayer() == GetActivePlayer()->GetPlayer()
		|| !GetWorld()->CanSeeAction( GetActivePlayer()->GetPlayer() ) )
		pCamera->ProcessEvent( sEvent );

//////////////////////////////////////////
/// Unit control
	if ( !IsRealTime() && ( pWorld->GetCurrentPlayer() != pActivePlayer->GetPlayer() ) )
		return false;

	if ( bindCancel.ProcessEvent( sEvent ) || bindCancelAction.ProcessEvent( sEvent ) )
	{
		if ( pState->GetType() == IState::FORCED )
			ResetState();
		else if ( IsActionExecuted() )
			StopAction();
		else
		{
			vector<CPtr<IUnitTracker> > unitsSet;
			GetSelectedUnits( &unitsSet );

			for ( vector<CPtr<IUnitTracker> >::iterator iTemp = unitsSet.begin(); iTemp != unitsSet.end(); iTemp++ )
			{
				NWorld::CUnit::EState eState = (*iTemp)->GetUnit()->GetState();
				if ( eState == NWorld::CUnit::ST_MACHINE_GUN )
					Command( (*iTemp)->GetUnit(), new NWorld::CCmdExitCannon );
				else if ( ( eState == NWorld::CUnit::ST_SNIPE ) || ( eState == NWorld::CUnit::ST_HEALER ) || !(*iTemp)->IsPathComplete() ) 
					Command( new NWorld::CCmdCancel( (*iTemp)->GetUnit() ) );
			}
		}

		return true;
	}

	if ( !IsRealTime() && IsActionExecuted() )
		return false;

	if ( IsRealTime() )
	{
		if ( bindStartOfTurn.ProcessEvent( sEvent ) )
		{
			vector< CPtr<IUnitTracker> > unitsSet;
			GetUnits( &unitsSet );
			if ( unitsSet.size() > 0 )
				Command( unitsSet.front()->GetUnit(), new NWorld::CCmdStartCombat );
			return true;
		}
	}
	else 
	{
		if ( bindEndOfTurn.ProcessEvent( sEvent ) )
		{
			Command( new NWorld::CCmdEndOfTurn );
			return true;
		}
	}

	if ( bindEndMission.ProcessEvent( sEvent ) )
	{
		vector<CPtr<NMainLoop::CInterfaceCommand> > cmdsSet;
		cmdsSet.push_back( new NMainLoop::CICSave( "Autosave (end mission)", true ) );
		cmdsSet.push_back( new CICEndMission( this ) );
		NMainLoop::Command( new NMainLoop::CICContainer( cmdsSet ) );
		return true;
	}

	if ( bindContinue.ProcessEvent( sEvent ) )
	{
		SActionInfo sInfo;
		GetActionInfo( UA_CONTINUE, &sInfo );
		if ( sInfo.bAvailable && sInfo.bOk )
		{
			vector<CPtr<IUnitTracker> > unitsSet;
			GetSelectedUnits( &unitsSet );

			for ( vector<CPtr<IUnitTracker> >::iterator iTemp = unitsSet.begin(); iTemp != unitsSet.end(); iTemp++ )
				Command( (*iTemp)->GetUnit(), new NWorld::CCmdContinue() );

			return true;
		}
	}

	/// check for selection
	int nSelection = -1;
	if ( bindHero1.ProcessEvent( sEvent ) )
		nSelection = 0;
	else if ( bindHero2.ProcessEvent( sEvent ) )
		nSelection = 1;
	else if ( bindHero3.ProcessEvent( sEvent ) )
		nSelection = 2;
	else if ( bindHero4.ProcessEvent( sEvent ) )
		nSelection = 3;
	else if ( bindHero5.ProcessEvent( sEvent ) )
		nSelection = 4;
	else if ( bindHero6.ProcessEvent( sEvent ) )
		nSelection = 5;

	if ( nSelection != -1 )
	{
		vector< CPtr<IUnitTracker> > unitsSet;
		GetUnits( &unitsSet );
		if ( nSelection < unitsSet.size() )
		{
			if ( !unitsSet[nSelection]->IsSelected() || ( CountSelected() != 1 ) )
				Select( unitsSet[nSelection]->GetUnit(), false );
			else
				FocusCameraOnUnit( unitsSet[nSelection]->GetUnit() );
		}
	}
	else if ( bindSelPrev.ProcessEvent( sEvent ) )
		SelectPrev();
	else if ( bindSelNext.ProcessEvent( sEvent ) )
		SelectNext();

	if ( bindPrevSlot.ProcessEvent( sEvent ) )
		SelectSlot( -1 );
	if ( bindNextSlot.ProcessEvent( sEvent ) )
		SelectSlot( 1 );

	if ( bindWeaponReload.ProcessEvent( sEvent ) )
		WeaponReload();
	if ( bindItemUnload.ProcessEvent( sEvent ) )
	{
		if ( CDynamicCast<CStateUnloadItem> pState( GetState() ) )
		{
			ResetState();
		}
		else
		{
			ResetState();
			CommandState( new CStateUnloadItem );
		}
	}
	if ( bindItemArrange.ProcessEvent( sEvent ) )
	{
		vector<CPtr<IUnitTracker> > unitsSet;
		GetSelectedUnits( &unitsSet );

		for ( vector<CPtr<IUnitTracker> >::iterator iTemp = unitsSet.begin(); iTemp != unitsSet.end(); iTemp++ )
			Command( (*iTemp)->GetUnit(), new NWorld::CCmdArrangeInventory() );
	}

	if ( bindSnapShot.ProcessEvent( sEvent ) )
		NewWeaponMode( NDb::SM_Snap );
	else if ( bindAimedShot.ProcessEvent( sEvent ) )
		NewWeaponMode( NDb::SM_Aimed );
	else if ( bindCarefulShot.ProcessEvent( sEvent ) )
		NewWeaponMode( NDb::SM_Careful );
	else if ( bindShortBurst.ProcessEvent( sEvent ) )
		NewWeaponMode( NDb::SM_ShortBurst );
	else if ( bindLongBurst.ProcessEvent( sEvent ) )
		NewWeaponMode( NDb::SM_LongBurst );
	else if ( bindSnipeShot.ProcessEvent( sEvent ) )
		NewWeaponMode( NDb::SM_Snipe );

	if ( bindGrenadeModeThrow.ProcessEvent( sEvent ) )
		NewGrenadeMode( NRPG::GM_THROW );
	if ( bindGrenadeModeSetTrap.ProcessEvent( sEvent ) )
		NewGrenadeMode( NRPG::GM_SETTRAP );

	if ( bindWeaponPrevMode.ProcessEvent( sEvent ) )
		SelectWeaponMode( -1 );
	else if ( bindWeaponNextMode.ProcessEvent( sEvent ) )
		SelectWeaponMode( 1 );

	/// Forced actions
	if ( bindMove.ProcessEvent( sEvent ) )
		CommandState( new CStateMove( true ) );
	else if ( bindRotate.ProcessEvent( sEvent ) )
		CommandState( new CStateRotate() );
	else if ( bindAttack.ProcessEvent( sEvent ) )
		CommandState( new CStateAttack( true ) );
	else if ( bindSetMine.ProcessEvent( sEvent ) )
		CommandState( new CStateSetMine() );
	else if ( bindSetTrap.ProcessEvent( sEvent ) )
		CommandState( new CStateSetTrap() );
	else if ( bindFirstAid.ProcessEvent( sEvent ) )
		CommandState( new CStateFirstAid() );
	else if ( bindDropCorpse.ProcessEvent( sEvent ) )
		CommandState( new CStateDropCorpse() );
	else if ( bindExitPK.ProcessEvent( sEvent ) ) //// CRAP
	{
		vector< CPtr<NGame::IUnitTracker> > unitsSet;
		GetSelectedUnits( &unitsSet );
		for ( int nTemp = 0; nTemp < unitsSet.size(); nTemp++ )
			Command( unitsSet[nTemp]->GetUnit(), new NWorld::CCmdExitPK() );
	}

	if ( bindNormalPose.ProcessEvent( sEvent ) )
		CommandState( new CStatePose( NAI::WALK ) );
	else if ( bindCrawlPose.ProcessEvent( sEvent ) )
		CommandState( new CStatePose( NAI::CRAWL ) );
	else if ( bindCrouchPose.ProcessEvent( sEvent ) )
		CommandState( new CStatePose( NAI::CROUCH ) );
	else if ( bindRunPose.ProcessEvent( sEvent ) )
		CommandState( new CStatePose( NAI::RUN ) );
	else if ( bindStrafe.ProcessEvent( sEvent ) )
	{
		SActionInfo sAction;
		GetActionInfo( UA_STRAFE, &sAction );
		if ( sAction.eResult == NWorld::UCR_OK )
		{
			vector< CPtr<NGame::IUnitTracker> > unitsSet;
			GetSelectedUnits( &unitsSet );
			if ( unitsSet.size() == 1 )
			{
				NWorld::CUnit *pUnit = unitsSet.front()->GetUnit();
				Command( pUnit, new NWorld::CCmdStrafe( !pUnit->IsStrafing() ) );
			}
		}
		else
			ShowError( this, sAction.eResult );
	}
	else if ( bindHide.ProcessEvent( sEvent ) )
	{
		SActionInfo sAction;
		GetActionInfo( UA_STRAFE, &sAction );
		if ( sAction.eResult == NWorld::UCR_OK )
		{
			vector< CPtr<NGame::IUnitTracker> > unitsSet;
			GetSelectedUnits( &unitsSet );
			if ( unitsSet.size() == 1 )
				Command( unitsSet.front()->GetUnit(), new NWorld::CCmdHide() );
		}
		else
			ShowError( this, sAction.eResult );
	}

	if ( bindSnipeAttack.ProcessEvent( sEvent ) )
	{
		SActionInfo sAction;
		GetActionInfo( UA_SNIPE_ATTACK, &sAction );
		if ( sAction.eResult == NWorld::UCR_OK )
		{
			vector< CPtr<NGame::IUnitTracker> > unitsSet;
			GetSelectedUnits( &unitsSet );
			if ( unitsSet.size() == 1 )
				Command( unitsSet.front()->GetUnit(), new NWorld::CCmdSnipeAttack() );
		}
		else
			ShowError( this, sAction.eResult );
	}
	else if ( bindCollect1AP.ProcessEvent( sEvent ) )
		UnitCollectAP( NWorld::CSAP_1AP );
	else if ( bindCollect10AP.ProcessEvent( sEvent ) )
		UnitCollectAP( NWorld::CSAP_10AP );
	else if ( bindCollectMaxAP.ProcessEvent( sEvent ) )
		UnitCollectAP( NWorld::CSAP_MAX );
	else if ( bindCollectAllAP.ProcessEvent( sEvent ) )
		UnitCollectAP( NWorld::CSAP_ALL );

	if ( bindNextEnemy.ProcessEvent( sEvent ) )
	{
		vector< CPtr<NGame::IUnitTracker> > unitsSet;
		GetSelectedUnits( &unitsSet );
		if ( unitsSet.size() == 1 )
		{
			CPtr<NWorld::CUnit> pEnemy = unitsSet[0]->GetNextVisibleEnemy();
			if ( IsValid( pEnemy ) )
				FocusCameraOnUnit( pEnemy );
		}
	}
	else if ( bindCheatTeleport.ProcessEvent( sEvent ) )
	{
		vector< CPtr<NGame::IUnitTracker> > unitsSet;
		GetSelectedUnits( &unitsSet );
		if ( unitsSet.size() == 1 )
		{
			NAI::SUnitPosition sPosition = unitsSet[0]->GetUnit()->GetPosition();
			if( GetTracePosition( &sPosition.pos ) )
				Command( unitsSet[0]->GetUnit(), new NWorld::CCmdTeleport( sPosition ) );
		}

		return true;
	}
	else if ( bindFocusUnit.ProcessEvent( sEvent ) )
	{
		vector< CPtr<IUnitTracker> > unitsSet;
		GetSelectedUnits( &unitsSet );
		if ( unitsSet.size() == 1 )
		{
			for ( vector< CPtr<IUnitTracker> >::iterator iUnit = unitsSet.begin(); iUnit != unitsSet.end(); iUnit++ )
				FocusCameraOnUnit( (*iUnit)->GetUnit() );
		}
	}


//// Some special binds
	if ( bindTrailerHideInterface.ProcessEvent( sEvent ) )
	{
		CPtr<NUI::CMissionTrailerUI> pMissionTrailerUI = new NUI::CMissionTrailerUI( NUI::SWindowInfo( pInterface, NUI::SPoint( 0, 0 ), NUI::SPoint( 1024, 768 ), "trailerUI", NUI::STYLE_ENABLED ), this, GetDesktop() );
		NUI::LoadTemplate( pMissionTrailerUI, NDb::GetUIContainer( 376 ) );
		pMissionTrailerUI->ShowDesktop();
		return true;
	}

	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMission::RenderFrame( int nMode, const STime &sTime, ICamera *pCamera, bool bShowUnits )
{
	if ( nMode & N_RENDERMODE_3D )
	{
		CTransformStack ts;
		pCamera->GetTransform( &ts, pScene->GetScreenRect() );

		pRenderSound->Update( &ts, sTime );

		const CTRect<float> &rScreen = pCamera->GetScreenRect();
		if ( ( rScreen.Width() != 0 ) && ( rScreen.Height() != 0 ) )
		{
			NGScene::IGameView::SDrawInfo drawInfo;
			drawInfo.pTS = &ts;
			drawInfo.vOrigin = CVec2( rScreen.x1, rScreen.y1 );
			drawInfo.vSize = CVec2( rScreen.x2 - rScreen.x1, rScreen.y2 - rScreen.y1 );
			drawInfo.bUseDefaultClearColor = true;
			drawInfo.vClearColor = CVec3(0.25f,0.25f,0.25f); // not used due to using default clear color
			pScene->Draw( drawInfo );
		}
	}

	if ( !bHideInterface && !bSpecialHideInterface && ( nMode & N_RENDERMODE_2D ) )
		pInterface->Draw( sTime );

	float fFrameTime = NGScene::GetFrameTime();
	static float fMinFrameTime = 1, fMaxFrameTime = 1e-4f, fElapsed = 0;
	static int nFrames;
	fMinFrameTime = Min( fMinFrameTime, fFrameTime );
	fMaxFrameTime = Max( fMaxFrameTime, fFrameTime );
	fElapsed += fFrameTime;
	++nFrames;
	if ( fElapsed > 3 )
	{
		DebugTrace( "min %f max %f average %f\n", 1 / fMaxFrameTime, 1 / fMinFrameTime, nFrames / fElapsed );
		fMinFrameTime = 1;
		fMaxFrameTime = 1e-4f;
		fElapsed = 0;
		nFrames = 0;
	}

	NGScene::Flip();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Internal
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMission::TrackChanges()
{
	bool bUpdated = false;

	CPtr<NWorld::IPlayer> pNewTrackPlayer = pWorld->GetCurrentPlayer();
	if ( pNewTrackPlayer != pTrackPlayer )
	{
		bUpdated = true;
		pTrackPlayer = pNewTrackPlayer;
	}

	vector< CPtr<NGame::IUnitTracker> > unitsSet;
	GetSelectedUnits( &unitsSet );
	if ( unitsSet != selectedUnits )
	{
		bUpdated = true;
		selectedUnits = unitsSet;
	}

	bool bNewRealTime = IsRealTime();
	if ( bNewRealTime != bRealTime )
	{
		bUpdated = true;
		bRealTime = bNewRealTime;
	}

	NWorld::IPlayer::SItemInfo sItem;
	CPtr<CObjectBase> pNewPlayerInHand;
	if ( pActivePlayer->GetPlayer()->GetInHandItem( &sItem ) )
		pNewPlayerInHand = sItem.pItem;
	if ( pNewPlayerInHand != pPlayerInHand )
	{
		bUpdated = true;
		pPlayerInHand = pNewPlayerInHand;
	}

	bool bNewHasCommands = pActivePlayer->GetCommander()->HasCommands();
	if ( bNewHasCommands != bHasCommands )
	{
		bUpdated = true;
		bHasCommands = bNewHasCommands;
	}

	bool bNewActionExecuted = IsActionExecuted();
	if ( bNewActionExecuted != bActionExecuted )
	{
		bUpdated = true;
		bActionExecuted = bNewActionExecuted;
	}

	CPtr<CObjectBase> pNewTrackTarget = GetStateTarget();
	if ( pNewTrackTarget != pTrackTarget )
	{
		bUpdated = true;
		pTrackTarget = pNewTrackTarget;
	}

	return bUpdated;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMission::UpdateState()
{
	if ( ( pState->GetType() == IState::FORCED ) && pState->Initialize( this ) )
		return;
	if ( ( pState->GetType() == IState::TEMPORARY ) && pState->Initialize( this ) )
		return;

	pState->Terminate();
	pState = 0;
	for ( int nTemp = 0; nTemp < updatedStatesSet.size(); nTemp++ )
	{
		if ( CommandState( updatedStatesSet[nTemp] ) )
			return;
	}

	ASSERT( 0 );
	CommandState( new CStateEmpty );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMission::UpdateSound()
{
	int nEWatch = pWorld->GetEnemyWatchers( pActivePlayer->GetPlayer() );
	list< CPtr<NWorld::CUnit> > visible;
	int nVisibleEnemy = 0;
	pActivePlayer->GetPlayer()->GetVisible( &visible );
	for ( list< CPtr<NWorld::CUnit> >::const_iterator it = visible.begin(); it != visible.end(); ++it )
		if ( !(*it)->IsDead() && !(*it)->IsUnconscious() && (*it)->GetPlayer() != pActivePlayer->GetPlayer() )
			++nVisibleEnemy;
	if ( nEWatch > 0 || nVisibleEnemy > 0 )
		pSoundScene->SetMusic( pCombatMelody );
	else
		pSoundScene->FadeOutMusic();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMission::WeaponReload()
{
	SActionInfo sInfo;
	GetActionInfo( UA_WEAPONRELOAD, &sInfo );
	if ( sInfo.eResult != NWorld::UCR_OK )
	{
		ShowError( this, sInfo.eResult );
		return;
	}

	vector< CPtr<IUnitTracker> > unitsSet;
	GetSelectedUnits( &unitsSet );
	for ( int nTemp = 0; nTemp < unitsSet.size(); nTemp++ )
		Command( unitsSet[nTemp]->GetUnit(), new NWorld::CCmdReload );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMission::SelectSlot( int nInc )
{
	vector< CPtr<IUnitTracker> > unitsSet;
	GetSelectedUnits( &unitsSet );

	if ( unitsSet.size() != 1 )
		return;

	int nSlot = unitsSet[0]->GetUnit()->GetRPG()->GetInventoryInfo()->GetActiveSlot();
	nSlot += nInc;
	if ( nSlot >= NDb::N_SLOTS )
		nSlot = 0;
	if ( nSlot < 0 )
		nSlot = NDb::N_SLOTS - 1;

	Command( unitsSet[0]->GetUnit(), new NWorld::CCmdSetActiveItem( NDb::ESlot( nSlot ) ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMission::NewWeaponMode( NDb::EShootMode eMode )
{
	vector< CPtr<IUnitTracker> > unitsSet;
	GetSelectedUnits( &unitsSet );
	for ( vector< CPtr<IUnitTracker> >::iterator iTemp = unitsSet.begin(); iTemp != unitsSet.end(); iTemp++ )
		Command( (*iTemp)->GetUnit(), new NWorld::CCmdShootMode( eMode ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMission::NewGrenadeMode( NRPG::EGrenadeMode eMode )
{
	vector< CPtr<IUnitTracker> > unitsSet;
	GetSelectedUnits( &unitsSet );
	for ( vector< CPtr<IUnitTracker> >::iterator iTemp = unitsSet.begin(); iTemp != unitsSet.end(); iTemp++ )
		Command( (*iTemp)->GetUnit(), new NWorld::CCmdGrenadeMode( eMode ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMission::SelectWeaponMode( int nInc )
{
	vector< CPtr<IUnitTracker> > unitsSet;
	GetSelectedUnits( &unitsSet );

	if ( unitsSet.size() != 1 )
		return;

	CPtr<NRPG::IInventoryItem> pItem = unitsSet[0]->GetUnit()->GetRPG()->GetInventoryInfo()->GetActive();
	if ( IsValid( pItem ) )
	{
		if ( CDynamicCast<NRPG::IWeaponItemInfo> pWeapon( pItem ) )
		{
			int nMode = pWeapon->GetShootMode();
			for ( int nTemp = 0; nTemp < NDb::SM_MAXVALUE; nTemp++ )
			{
				nMode += nInc;
				if ( nMode < 0 )
					nMode = NDb::SM_MAXVALUE - 1;
				if ( nMode >= NDb::SM_MAXVALUE )
					nMode = 0;

				if ( pWeapon->GetDBWeapon()->shootModes[nMode] )
				{
					NewWeaponMode( NDb::EShootMode( nMode ) );
					break;
				}
			}
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMission::SetLightMode( int _nLightMode )
{
	CDBTable<NDb::CTAmbientLight> *pTable = NDatabase::GetTable<NDb::CTAmbientLight>();
  CDBIterator<NDb::CTAmbientLight> it( *pTable );
	int nCount = _nLightMode;
  while ( it.MoveNext() )
  {
		SRand rnd;
		CPtr<NDb::CAmbientLightReal> pLight = it.Get()->GetLight( &rnd );
		if ( !pLight->bInGameUse )
			continue;
		if ( nCount == 0 )
		{
			nLightMode = _nLightMode;
			pLightSource = 0;
			csSystem << "Light with ID = " << it.Get()->GetRecordID() << " selected" << endl;
			GetScene()->SetAmbient( pLight );
			return;
		}
		nCount--;
	}
	if ( _nLightMode == 0 )
	{
		// no lighting in table, using default one
		nLightMode = 0;
		GetScene()->SetAmbient( 0 );
		pLightSource = GetScene()->AddDirectionalLight( CVec3(0.5f,0.4f,0.45f), CVec3( 0.6f,1.4f,-1), CVec3(5,5,0), CVec2( 150, 150 ), 20 );
		GetScene()->SetAmbient( CVec3( 0.20f, 0.20f, 0.20f ), CVec3( 0.20f, 0.20f, 0.20f ) );
	}
	else
		SetLightMode( 0 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMission::TraceCursor()
{
	if ( nFramesSameCameraPosition < 4 )
		return;
	CTransformStack sTS;
	pCamera->GetTransform( &sTS, GetScene()->GetScreenRect() );
	MakeProjectiveRay( &rTraceRay.ptDir, &rTraceRay.ptOrigin, &sTS, GetScene()->GetScreenRect(), GetCursor()->GetPos() );
	bTraceOk = NWorld::TraceTile( GetWorld(), rTraceRay, &sTraceTile, GetScene()->GetCutFloor() );

	list< CPtr<NWorld::CUnit> > visibleList;
	GetActivePlayer()->GetPlayer()->GetVisible( &visibleList );

	pTraceObject = 0;
	vector<CObjectBase*> objectsSet;
	NWorld::TraceObjects( GetWorld(), rTraceRay, &objectsSet, GetScene()->GetCutFloor() );
	if ( !objectsSet.empty() )
	{
		for ( vector<CObjectBase*>::iterator iTemp = objectsSet.begin(); iTemp != objectsSet.end(); iTemp++ )
		{
			NWorld::CUnit *pTempUnit = dynamic_cast<NWorld::CUnit*>( *iTemp );
			if ( pTempUnit )
			{
				if ( find( visibleList.begin(), visibleList.end(), pTempUnit ) != visibleList.end() )
				{
					pTraceObject = *iTemp;
					break;
				}
			}

			CDynamicCast<NWorld::IObject> pTempObject( *iTemp );
			if ( pTempObject && pTempObject->IsTargetable() )
			{
				pTraceObject = *iTemp;
				break;
			}

			if ( CDynamicCast<NWorld::IItem> pTempItem( *iTemp ) )
			{
				pTraceObject = *iTemp;
				break;
			}
		}
	}

	if ( !IsValid( pTraceObject ) )
	{
		NAI::SUnitPosition sPosition;
		sPosition.pos = sTraceTile;
		sPosition.bRun = false;
		NWorld::CUnit *pTempUnit = GetWorld()->GetUnitInTile( sPosition );
		if ( find( visibleList.begin(), visibleList.end(), pTempUnit ) != visibleList.end() )
			pTraceObject = pTempUnit;
	}

	if ( pTraceObject != 0 )
	{
		NWorld::CUnit *pTempUnit = dynamic_cast<NWorld::CUnit*>( pTraceObject.GetPtr() );
		if ( pTempUnit )
		{
			bTraceOk = true;
			sTraceTile = pTempUnit->GetPosition().pos;
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMission::UnitCollectAP( NWorld::ECollectSnipeAP eAP )
{
	EUnitAction eAction;
	switch( eAP )
	{
	case NWorld::CSAP_1AP:
		eAction = UA_COLLECTAP_1AP;
		break;
	case NWorld::CSAP_10AP:
		eAction = UA_COLLECTAP_10AP;
		break;
	case NWorld::CSAP_MAX:
		eAction = UA_COLLECTAP_MAX;
		break;
	case NWorld::CSAP_ALL:
		eAction = UA_COLLECTAP_ALL;
		break;
	default:
		ASSERT( 0 );
		return;
	}

	SActionInfo sAction;
	GetActionInfo( eAction, &sAction );
	if ( sAction.eResult != NWorld::UCR_OK )
	{
		ShowError( this, sAction.eResult );
		return;
	}

	vector< CPtr<NGame::IUnitTracker> > unitsSet;
	GetSelectedUnits( &unitsSet );
	if ( unitsSet.size() == 1 )
		Command( unitsSet.front()->GetUnit(), new NWorld::CCmdCollectSnipeAP( eAP ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMission::ExecWorldCommands()
{
	while( !IsValid( pCmdExec ) )
	{
		CPtr<NWorld::CUICmd> pCmd = pWorld->GetUICommand();
		if ( !IsValid( pCmd ) )
			return;

		if ( CDynamicCast<NWorld::CUICmdPartFinished> pPartFinished( pCmd ) )
		{
			bWaitForPartFinished = false;
		}
		else if ( CDynamicCast<NWorld::CUICmdBeginSequence> pBeginSequence( pCmd ) )
		{
			pCamera->SetLimits( ICamera::SCameraLimits() );
			////
			CPtr<NUI::CMissionMovieUI> pMissionMovieUI = new NUI::CMissionMovieUI( NUI::SWindowInfo( pInterface, NUI::SPoint( 0, 0 ), NUI::SPoint( 1024, 768 ), "movieUI", NUI::STYLE_ENABLED ), this, GetDesktop() );
			NUI::LoadTemplate( pMissionMovieUI, NDb::GetUIContainer( 364 ) );
			pMissionMovieUI->ShowDesktop();
		}
		else if ( CDynamicCast<NWorld::CUICmdEndSequence> pEndSequence( pCmd ) )
		{
			pCamera->SetLimits( cameraLimits );
			////
			for ( list<CObj<NUI::CDesktopWindow> >::reverse_iterator iTemp = desktopWindowsList.rbegin(); iTemp != desktopWindowsList.rend(); iTemp++ )
			{
				if ( CDynamicCast<NUI::CMissionMovieUI> pMissionMovieUI( *iTemp ) )
					pMissionMovieUI->HideDesktop();
			}
		}
		else if ( CDynamicCast<NWorld::CUICmdPlayDialog> pDialog( pCmd ) )
		{
			if ( bWaitForPartFinished )
			{
				csSystem << CC_RED << "WARNING: Dialog in in WaitForPartFinished mode ignored!" << endl;
				continue;
			}
			////
			CPtr<NUI::CMissionDlgUI> pMissionDlgUI = new NUI::CMissionDlgUI( NUI::SWindowInfo( pInterface, NUI::SPoint( 0, 0 ), NUI::SPoint( 1024, 768 ), "dialogUI", NUI::STYLE_ENABLED ), this, GetDesktop(), pDialog->szDialogCode, pDialog->units, pDialog->phrases );
			NUI::LoadTemplate( pMissionDlgUI, NDb::GetUIContainer( 364 ) );
			pMissionDlgUI->ShowDesktop();
		}
		else if ( CDynamicCast<NWorld::CUICmdPlayAck> pAck( pCmd ) )
			GetDesktop()->PlayAck( pAck->phrases.front() );
		else if ( CDynamicCast<NWorld::CUICmdSetFloor> pFloor( pCmd ) )
			pScene->SetCutFloor( pFloor->nFloor );
		else if ( CDynamicCast<NWorld::CUICmdShowClue> pClue( pCmd ) )
		{
			if ( IsValid( pClue->pClue ) )
				NMainLoop::Command( new NGame::CICShowClue( pGlobalGame, pClue->pClue ) );
		}
		else
			pCmdExec = GetDesktop()->CreateExecutor( pCmd );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMission::SaveWorld( const string &szFile )
{
	NMainLoop::CSaveManager *pSaveManager = NMainLoop::GetSaveManager();

	pSaveManager->PrepareSlot( NMainLoop::S_SLOT_ACTIVE );

//#ifndef _DEBUG
	try
//#endif
	{
		CFileStream sFile;
		sFile.OpenWrite( pSaveManager->GetSlotFilePath( NMainLoop::S_SLOT_ACTIVE, szFile ).c_str() );

		CStructureSaver sSaver( sFile, CStructureSaver::WRITE );
		sSaver.Add( 2, &pWorld );
		SerializeShared( &sSaver );
	}
//#ifndef _DEBUG
	catch(...)
	{
		csSystem << "WARNING: Can't save zone" << szFile << endl;
	}
//#endif
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMission::LoadWorld( const string &szFile )
{
	NMainLoop::CSaveManager *pSaveManager = NMainLoop::GetSaveManager();

	pSaveManager->PrepareSlot( NMainLoop::S_SLOT_ACTIVE );

//#ifndef _DEBUG
	try
//#endif
	{
		CFileStream sFile;
		sFile.OpenRead( pSaveManager->GetSlotFilePath( NMainLoop::S_SLOT_ACTIVE, szFile ).c_str() );

		CSharedHolder hold;
		CStructureSaver sSaver( sFile, CStructureSaver::READ );
		sSaver.Add( 2, &pWorld );
		SerializeShared( &sSaver );
	}
//#ifndef _DEBUG
	catch(...)
	{
		csSystem << "WARNING: Can't load zone" << szFile << endl;
	}
//#endif
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMission::TestIntersection()
{
	CRay r;
	// CRAP - have to make special function in pGame for this
	CTransformStack sTS = GetCameraTransform();
	MakeProjectiveRay( &r.ptDir, &r.ptOrigin, &sTS, pScene->GetScreenRect(), pCursor->GetPos() );
	r.ptDir *= 100;
	float fT;
	CVec3 vNormal, vColor;
	if ( pScene->TraceScene( r, &fT, &vNormal, &vColor ) )
	{
		CVec3 vPoint = r.Get( fT );
		CPtr<CMemObject> pModel = new CMemObject;
		pModel->CreateSphere( vPoint, 0.06f );
		pIntersectHolder = pScene->CreateMesh( pModel, CVec4(vColor,1), 0 );
		vector<CVec3> points;
		points.push_back( vPoint );
		points.push_back( vPoint + vNormal * 0.2f );
		pIntersectLineHolder = pScene->CreatePolyline( points, CVec3(1,1,1) );
	}
	else
	{
		pIntersectHolder = 0;
		pIntersectLineHolder = 0;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMission::ShowWeatherEffect( int nID )
{
	ICamera::SCameraPos cameraPos;
	GetCamera()->GetPlacement( &cameraPos );
	SFBTransform pos;
	MakeMatrix( &pos, CVec3(1,1,1), cameraPos.ptAnchor, 0 );
	NDb::CTEffect *pTEffect = NDb::GetTEffect( nID );
	SRand rnd;
	pTestWeatherEffect = GetScene()->CreateParticles( pTEffect->GetEffect( &rnd ), 0, pRender->GetTime(), pos );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMission::ShowVisibleFrom()
{
	if ( !pVisibleTracker )
	{
		CRay r;
		CTransformStack sTS = GetCameraTransform();
		MakeProjectiveRay( &r.ptDir, &r.ptOrigin, &sTS, pScene->GetScreenRect(), pCursor->GetPos() );
		r.ptDir *= 100;
		float fT;
		CVec3 vNormal, vColor;
		if ( pScene->TraceScene( r, &fT, &vNormal, &vColor ) )
		{
			static int nMask = 1;
			pVisibleTracker = new CVisibleTracker( CVisibleTracker::VISIBLE_FROM, 7/*nMask*/, r.Get(fT), 5 );
			pVisibleTracker->Update( this );
			nMask <<= 1;
			if ( nMask == 8 )
				nMask = 1;
		}
	}
	else
		pVisibleTracker = 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMission::ClickOfDeath()
{
	pWorld->ClickOfDeath( GetTraceRay(), pScene->GetCutFloor() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMission::Explosion()
{
	pWorld->MakeExplosion( GetTraceRay(), pScene->GetCutFloor() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMission::ErasePartUnderCursor()
{
	CRay r;
	// CRAP - have to make special function in pGame for this
	CTransformStack sTS = GetCameraTransform();
	MakeProjectiveRay( &r.ptDir, &r.ptOrigin, &sTS, pScene->GetScreenRect(), pCursor->GetPos() );
	r.ptDir *= 100;
	float fT;
	CVec3 vNormal, vColor;
	CObjectBase *pTarget;
	if ( pScene->TraceScene( r, &fT, &vNormal, &vColor, NGScene::SPS_ALL, &pTarget ) )
		CMObj<CObjectBase> pKillerLoop( pTarget );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CVisibleTracker
////////////////////////////////////////////////////////////////////////////////////////////////////
void CVisibleTracker::Update( IMission* pMission )
{
	CVec3 vColors[4] = { CVec3( 1, 0, 0 ), CVec3( 0, 1, 0 ), CVec3( 1, 1, 1 ), CVec3( 0, 1, 0 ) };

	bUpdated = false;

	vector< CPtr<IUnitTracker> > unitsSet;
	pMission->GetSelectedUnits( &unitsSet );

	if ( unitsSet != selectedUnits )
	{
		selectedUnits = unitsSet;
		bUpdated = true;
	}

	if ( !bUpdated )
		return;

	nodesSet.clear();
	for ( int nTemp = 0; nTemp < selectedUnits.size(); nTemp++ )
	{
		vector<NRPG::SVisibilitySpot> spotsSet;
		if ( type == VISIBLE )
			pMission->GetWorld()->GetGame()->GetVisibilityArea( &spotsSet, unitsSet[nTemp]->GetUnit() );
		else
			pMission->GetWorld()->GetGame()->GetVisibleFromArea( &spotsSet, unitsSet[nTemp]->GetUnit(), vFrom, fRadius, nMask );

		for ( int nSpot = 0; nSpot < spotsSet.size(); nSpot++ )
		{
			CPtr<CMemObject> pModelBuilder = new CMemObject;
			pModelBuilder->CreateSphere( spotsSet[nSpot].ptPos, 0.06f, 0 );
			CVec3 cr = vColors[spotsSet[nSpot].nCanSee];
			nodesSet.push_back( pMission->GetScene()->CreateMesh( pModelBuilder, CVec4( cr, 1 ), 0 ) );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CViewBuildingScheme
////////////////////////////////////////////////////////////////////////////////////////////////////
CViewBuildingScheme::CViewBuildingScheme( CSyncSrc<NWorld::IVisObj> *pSrc, NGScene::IGameView *_pScene, vector<CObj<CObjectBase> > *_pRes ):
	CSyncDst<NWorld::IVisObj>(pSrc), pScene(_pScene), pRes(_pRes)
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CViewBuildingScheme::AddBuilding( const SMapBuilding &info )
{
	pRes->push_back( ViewBuildingSchema( pScene, info.pSWMap, info.pVariant->GetRecordID(), info.pGrid, info.pos ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUpdateBuildingStability
////////////////////////////////////////////////////////////////////////////////////////////////////
CUpdateBuildingStability::CUpdateBuildingStability( CSyncSrc<NWorld::IVisObj> *pSrc ):
	CSyncDst<NWorld::IVisObj>(pSrc)
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUpdateBuildingStability::AddBuilding( const SMapBuilding &info )
{
	NBuilding::UpdateBuildingStability( info.pVariant->GetRecordID(), info.pGrid, info.pSWMap );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// COMMANDS
////////////////////////////////////////////////////////////////////////////////////////////////////
static void VarCheatSeeAll( const string &szID, const NGlobal::CValue &sValue, void *pContext )
{
	CObjectBase *pBase = (CObjectBase*)pContext;
	IMission *pGame = dynamic_cast<IMission*>( pBase );
	ASSERT( pGame != 0 );

	if ( sValue.GetFloat() > 0 )
		pGame->Command( new NWorld::CCmdCheat( NRPG::CHEAT_SEEALL, true ) );
	else
		pGame->Command( new NWorld::CCmdCheat( NRPG::CHEAT_SEEALL, false ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void VarCheatShowAll( const string &szID, const NGlobal::CValue &sValue, void *pContext )
{
	bShowAllCheat = false;
	if ( sValue.GetFloat() != 0 )
		bShowAllCheat = true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void VarCheatGodMode( const string &szID, const NGlobal::CValue &sValue, void *pContext )
{
	CObjectBase *pBase = (CObjectBase*)pContext;
	IMission *pGame = dynamic_cast<IMission*>( pBase );
	ASSERT( pGame != 0 );

	if ( sValue.GetFloat() > 0 )
		pGame->Command( new NWorld::CCmdCheat( NRPG::CHEAT_GODMODE, true ) );
	else
		pGame->Command( new NWorld::CCmdCheat( NRPG::CHEAT_GODMODE, false ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void VarCheatAP( const string &szID, const NGlobal::CValue &sValue, void *pContext )
{
	CObjectBase *pBase = (CObjectBase*)pContext;
	IMission *pGame = dynamic_cast<IMission*>( pBase );
	ASSERT( pGame != 0 );
	//
	if ( sValue.GetFloat() > 0 )
		pGame->Command( new NWorld::CCmdCheat( NRPG::CHEAT_AP, true ) );
	else
		pGame->Command( new NWorld::CCmdCheat( NRPG::CHEAT_AP, false ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void VarCheatTeleport( const string &szID, const NGlobal::CValue &sValue, void *pContext )
{
	CObjectBase *pBase = (CObjectBase*)pContext;
	IMission *pGame = dynamic_cast<IMission*>( pBase );
	ASSERT( pGame != 0 );

	if ( sValue.GetFloat() > 0 )
		pGame->Command( new NWorld::CCmdCheat( NRPG::CHEAT_TELEPORT, true ) );
	else
		pGame->Command( new NWorld::CCmdCheat( NRPG::CHEAT_TELEPORT, false ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void CommandTypeCameraParams( const string &szID, const vector<wstring> &paramsSet, void *pContext )
{
	CObjectBase *pBase = (CObjectBase*)pContext;
	IMission *pGame = dynamic_cast<IMission*>( pBase );
	ASSERT( pGame != 0 );

	ICamera::SCameraPos pos;
	pGame->GetCamera()->GetPlacement( &pos );
	csSystem << "current camera params:" << endl;
	csSystem << "pitch = " << pos.fPitch << "  yaw = " << pos.fYaw << endl;
	csSystem << "rod = " << pos.fRod << endl;
	csSystem << "anchor = ( " << pos.ptAnchor.x << ", " << pos.ptAnchor.y << ", " << pos.ptAnchor.z << " )" << endl;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void CommandSetDefaultCameraLimits( const string &szID, const vector<wstring> &paramsSet, void *pContext )
{
	if ( paramsSet.size() != 5 )
	{
		csSystem << "Usage: mission_camera_limits FOV minRod maxRod minPitch maxPitch" << endl;
		return;
	}
	fDefaultCameraFOV = atof( NStr::ToAscii( paramsSet[0] ).data() );
	defaultCameraLimits.fMinRod = atof( NStr::ToAscii( paramsSet[1] ).data() );
	defaultCameraLimits.fMaxRod = atof( NStr::ToAscii( paramsSet[2] ).data() );
	defaultCameraLimits.fMinPitch = atof( NStr::ToAscii( paramsSet[3] ).data() );
	defaultCameraLimits.fMaxPitch = atof( NStr::ToAscii( paramsSet[4] ).data() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Console commands
////////////////////////////////////////////////////////////////////////////////////////////////////
static NRPG::CGlobalPlayer* CreateGlobalPlayer( const string &szID, const vector<wstring> &paramsSet, int *pNumber )
{
	int &n = *pNumber;
	if ( n >= paramsSet.size() )
		return 0;
	vector<int> pers;
	while ( n < paramsSet.size() )
	{
		int nPers = wcstol( paramsSet[n].data(), 0, 10 );
		if ( paramsSet[n] == L"vs" || paramsSet[n] == L"with" )
			break;
		++n;
		pers.push_back( nPers );
	}
	if ( pers.empty() )
		return 0;
	return NRPG::CreateGlobalPlayer( pers );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void AnalyzeMapStartParams( const string &szID, NRPG::CGlobalGame *pGlobalGame, const vector<wstring> &paramsSet, 
	vector<string> *pTemplParams )
{
	int nParam = 1;
	for(;;)
	{
		CPtr<NRPG::CGlobalPlayer> pGlobalPlayer = CreateGlobalPlayer( szID, paramsSet, &nParam );
		if ( IsValid( pGlobalPlayer ) )
			pGlobalGame->players.push_back( pGlobalPlayer.GetPtr() );
		if ( nParam == paramsSet.size() )
			break;
		if ( paramsSet[ nParam ] == L"vs" )
		{
			++nParam;
			continue;
		}
		if ( paramsSet[ nParam ] == L"with" )
		{
			for ( int k = nParam + 1; k < paramsSet.size(); ++k )
				pTemplParams->push_back( NStr::ToAscii( paramsSet[k] ) );
			break;
		}
	}
	if ( pGlobalGame->players.empty() )
		pGlobalGame->players.push_back( NRPG::CreateGlobalPlayer() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void CommandStartMission( const string &szID, const vector<wstring> &paramsSet, void *pContext )
{
	if ( paramsSet.size() == 0 )
	{
		csSystem << "usage: #zone [PersID [PersID]] [vs [PersID [PersID]]] [with [paramNames]]" << endl;
		return;
	}

	int nTemp = wcstol( paramsSet.front().data(), 0, 10 );

	vector<string> templParams;
	CPtr<NRPG::CGlobalGame> pGlobalGame = NRPG::CreateGlobalGame();
	AnalyzeMapStartParams( szID, pGlobalGame, paramsSet, &templParams );
	//
	csSystem << CC_BLUE << "Loading world ( variant " << nTemp << " ) ";
	if ( templParams.size() )
	{
		csSystem << "with ";
		for ( int k = 0; k < templParams.size(); ++k )
			csSystem << templParams[k] << ", ";
	}
	csSystem << "..." << endl;
	//
	NMainLoop::Command( new CICBeginMission( -1, nTemp, templParams, pGlobalGame ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void CommandStartMissionByTemplateID( const string &szID, const vector<wstring> &paramsSet, void *pContext )
{
	if ( paramsSet.size() == 0 )
	{
		csSystem << "usage: #zone [PersID [PersID]] [vs [PersID [PersID]]]" << endl;
		return;
	}

	int nTemp = wcstol( paramsSet.front().data(), 0, 10 );

	vector<string> templParams;
	CPtr<NRPG::CGlobalGame> pGlobalGame = NRPG::CreateGlobalGame();
	AnalyzeMapStartParams( szID, pGlobalGame, paramsSet, &templParams );
	//
	csSystem << CC_BLUE << "Loading world ( template " << nTemp << " ) ";
	if ( templParams.size() )
	{
		csSystem << "with ";
			for ( int k = 0; k < templParams.size(); ++k )
				csSystem << templParams[k] << ", ";
	}
	csSystem << "..." << endl;
	//
	NMainLoop::Command( new CICBeginMission( nTemp, -1, templParams, pGlobalGame ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void CommandSurrender( const string &szID, const vector<wstring> &paramsSet, void *pContext )
{
	CObjectBase *pBase = (CObjectBase*)pContext;
	IMission *pGame = dynamic_cast<IMission*>( pBase );
	ASSERT( pGame != 0 );

	NMainLoop::Command( new CICEndMission( pGame ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void CommandSetXPLevel( const string &szID, const vector<wstring> &paramsSet, void *pContext )
{
	if ( paramsSet.size() < 1 )
		return;
	//
	CObjectBase *pObject = (CObjectBase *)pContext;
	if ( CDynamicCast<CMission> pMission( pObject) )
	{
		CPtr<NWorld::IWorld> pWorld = pMission->GetWorld();
		CPtr<IPlayerTracker> pTracker = pMission->GetActivePlayer();
		if ( !IsValid( pTracker ) )
			return;
		//
		vector< CPtr<NWorld::CUnit> > units;
		CPtr<NWorld::IPlayer> pActivePlayer = pTracker->GetPlayer();
		if ( paramsSet.size() == 1 )
			pActivePlayer->GetUnits( &units );
		else if ( paramsSet.size() == 2 )
		{
			if ( paramsSet[0] == L"ally" )
				pActivePlayer->GetUnits( &units );
			else if ( paramsSet[0] == L"enemy" )
			{
				vector< CPtr<NWorld::CUnit> > tmpUnits;
				pWorld->GetAllUnits( &tmpUnits );
				for ( vector< CPtr<NWorld::CUnit> >::iterator i = tmpUnits.begin(); 
					i != tmpUnits.end(); ++i )
				{
					if ( (*i)->GetPlayer() != pActivePlayer )
						units.push_back( *i );
				}
			}
		}
		//
		int nLevel = wcstol( paramsSet[	paramsSet.size() - 1 ].c_str(), 0, 10 );
		for ( vector< CPtr<NWorld::CUnit> >::iterator i = units.begin(); i != units.end(); ++i )
			(*i)->GetRPG()->GetRPGUnit()->SetXPLevel( nLevel );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void CommandSummonUnit( const string &szID, const vector<wstring> &paramsSet, void *pContext )
{
	if ( paramsSet.size() < 1 )
		return;
	//
	CObjectBase *pObject = (CObjectBase *)pContext;
	if ( CDynamicCast<CMission> pMission( pObject) )
		pMission->GetActivePlayer()->AddUnit( NRPG::CreateMerc( NDb::GetPers( _wtol( paramsSet[0].c_str() ) ) ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void CommandUnsummonUnit( const string &szID, const vector<wstring> &paramsSet, void *pContext )
{
	if ( paramsSet.size() < 1 )
		return;
	//
	int nTemp = _wtol( paramsSet[0].c_str() );
	//
	CObjectBase *pObject = (CObjectBase *)pContext;
	if ( CDynamicCast<CMission> pMission( pObject ) )
	{
		vector< CPtr<NGame::IUnitTracker> > unitsSet;
		pMission->GetUnits( &unitsSet );

		if ( nTemp < unitsSet.size() )
			pMission->GetActivePlayer()->RemoveUnit( unitsSet[nTemp] );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void CommandGetItem( const string &szID, const vector<wstring> &paramsSet, void *pContext )
{
	if ( paramsSet.size() < 1 )
		return;
	//
	int nID = _wtol( paramsSet[0].c_str() );
	//
	CObjectBase *pBase = (CObjectBase*)pContext;
	IMission *pMission = dynamic_cast<IMission*>( pBase );
	ASSERT( pMission != 0 );

	vector< CPtr<NGame::IUnitTracker> > unitsSet;
	pMission->GetSelectedUnits( &unitsSet );

	for ( int nTemp = 0; nTemp < unitsSet.size(); nTemp++ )
		pMission->Command( unitsSet[nTemp]->GetUnit(), new NWorld::CCmdCreateInventoryItem( NDb::GetRPGItem( nID ) ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CICBeginMission
////////////////////////////////////////////////////////////////////////////////////////////////////
CICBeginMission::CICBeginMission( int _nTemplateID, int _nVariantID, const vector<string> &_params, NRPG::CGlobalGame *_pGlobalGame ):
	nTemplateID( _nTemplateID ), nVariantID( _nVariantID ), pGlobalGame( _pGlobalGame ), params(_params)
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CICBeginMission::CICBeginMission( NScenario::CScenarioZone *_pZone, 
		int _nTemplateID, const vector<string> &_params, NRPG::CGlobalGame *_pGlobalGame ):
	pZone( _pZone ), nTemplateID( _nTemplateID ), nVariantID( -1 ), pGlobalGame( _pGlobalGame ), params(_params)
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CICBeginMission::Exec()
{
	NMainLoop::ShowLogo();

	CMission *pRes = new CMission();
	if ( pRes->Initialize( nTemplateID, nVariantID, pZone, params, pGlobalGame ) )
		SetInterface( pRes );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CICEndMission
////////////////////////////////////////////////////////////////////////////////////////////////////
CICEndMission::CICEndMission( IMission *_pMission ):
	pMission( _pMission )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CICEndMission::Exec()
{
	pMission->Terminate();

	NRPG::CGlobalGame *pGame = pMission->GetRPGGame();
	if ( pGame->bChapterMapSet )
	{
		NMainLoop::Command( new CICContinueChapter( pGame ) );
		return;
	}
	else if ( pGame->bGlobalMapSet )
	{
		NMainLoop::Command( new CICContinueGlobal( pGame ) );
		return;
	}

	csSystem << CC_RED << L"ERROR: GlobalMap and ChapterMap not set!" << endl;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CICBeginMission
////////////////////////////////////////////////////////////////////////////////////////////////////
CICMapEditBeginMission::CICMapEditBeginMission( int nVariantID, NRPG::CGlobalGame *pGlobalGame ):
	CICBeginMission( -1, nVariantID, vector<string>(), pGlobalGame )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CICMapEditBeginMission::Exec()
{
	NMainLoop::ShowLogo();
	CMission *pRes = new CMission();
	pRes->Initialize( -1, nVariantID, 0, params, pGlobalGame );
	PushInterface( pRes );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
static void CommandStartScenarioZone( const string &szID, const vector<wstring> &paramsSet, void *pContext )
{
	if ( paramsSet.size() < 2 )
	{
		csSystem << "usage: zone [scenario] [zone] [clue1] [clue2] ... [pers1] [pers2] ... " << endl;
		return;
	}
	//
	vector<int> perses;
	vector<string> clues;
	int nSize = paramsSet.size();
	for ( int i = 2; i < nSize; ++i )
	{
		string szStr = NStr::ToAscii( paramsSet[ i ] );
		int n = atoi( szStr.c_str() );
		if ( n == 0 )
			clues.push_back( szStr );
		else
			perses.push_back( n );
	}
	//
	CPtr<NRPG::CGlobalGame> pGlobalGame = NRPG::CreateGlobalGame();	
	if ( perses.empty() )
		pGlobalGame->players.push_back( NRPG::CreateGlobalPlayer() );
	else
		pGlobalGame->players.push_back( NRPG::CreateGlobalPlayer( perses ) );
	//
	CPtr<NScenario::CScenarioTracker> pScenario = pGlobalGame->pScenarioTracker;
	string szScenarioName = NStr::ToAscii( paramsSet[ 0 ] );
	pScenario->CreateScenario( szScenarioName );
	CDBPtr<NDb::CSide> pSide = NScenario::GetSideForScenario( pScenario );
	if ( !IsValid( pSide ) )
		return;
	pGlobalGame->players.front()->pSide = pSide;
	if ( pScenario->IsScenarioAvailable() )
	{
		string szZoneName = NStr::ToAscii( paramsSet[ 1 ] );
		CPtr<NScenario::CScenarioZone> pZone = pScenario->GetZoneByName( szZoneName );
		if ( IsValid( pZone ) )
		{
			if ( nSize > 2 )
			{
				// удаляем clues-ы
				vector< CPtr<NScenario::CScenarioClue> > tmpClues = pZone->GetClues();
				for ( vector< CPtr<NScenario::CScenarioClue> >::iterator i  = tmpClues.begin(); i != tmpClues.end(); ++i )
					pZone->RemoveClue( *i );
				// добавляем clues-ы
				for ( vector<string>::iterator i = clues.begin(); i != clues.end(); ++i )
				{
					CPtr<NScenario::CScenarioClue> pClue = pScenario->GetClueByName( *i );
					if ( IsValid( pClue ) )
						pZone->PlaceClue( pClue );
					else
						csSystem << CC_RED << "Error : " << CC_GREY << " clue " << *i << " not found" << endl;
				}
			}
			//
			NMainLoop::GetSaveManager()->ClearSlot( NMainLoop::S_SLOT_ACTIVE );
			NMainLoop::Command( new CICBeginMission( pZone, -1, vector<string>(), pGlobalGame ) );
		}
		else
			csSystem << CC_RED << "Error : " << CC_GREY << " scenario zone " << szZoneName << " not found" << endl;
	}
	else
		csSystem << CC_RED << "Error : " << CC_GREY << " scenario " << szScenarioName << " not found" << endl;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
START_REGISTER(iMission)
	REGISTER_CMD( "map", CommandStartMission )
	REGISTER_CMD( "template", CommandStartMissionByTemplateID )
	REGISTER_CMD( "zone", CommandStartScenarioZone )
	REGISTER_CMD( "mission_camera_limits", CommandSetDefaultCameraLimits )
	REGISTER_VAR( "cheat_showall", VarCheatShowAll, 0, false )
FINISH_REGISTER
////////////////////////////////////////////////////////////////////////////////////////////////////
} // NAMESPACE
////////////////////////////////////////////////////////////////////////////////////////////////////
using namespace NGame;
REGISTER_SAVELOAD_CLASS( 0xB251121B, CMission )
REGISTER_SAVELOAD_CLASS( 0x02511218, CVisibleTracker )

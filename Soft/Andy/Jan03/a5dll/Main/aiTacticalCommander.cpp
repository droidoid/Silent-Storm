#include "StdAfx.h"
//
#include "..\misc\HPTimer.h"
//
#include "rpgCheatConstants.h"
#include "rpgItem.h"
#include "rpgUnitMission.h"
//
#include "..\DBFormat\DataRPG.h"
//
#include "wMain.h"
#include "wInterface.h"
#include "wUnitServer.h"
#include "wUnitCommands.h"
//
#include "aiJob.h"
#include "aiLog.h"
#include "aiUnit.h"
#include "aiState.h"
#include "aiPlayer.h"
#include "aiIterator.h"
#include "aiInventory.h"
#include "aiCommander.h"
#include "aiTaskCommander.h"
#include "aiCompoundAction.h"
#include "aiSignal.h"
#include "aiControl.h"
//
#include "aiTacticalCommander.h"
//
namespace NAI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAITacticalCommander
////////////////////////////////////////////////////////////////////////////////////////////////////
CAITacticalCommander::CAITacticalCommander( CAICommander *_pAICommander ):
	CAIJob( 0 ), bEndOfTurn( true ), pAICommander(_pAICommander)
{
	ASSERT( IsValid( pAICommander ) );
	//
	pAILog = CreateAILogContainer();
	pAIState = CreateAIState( GetWorld(), this );
	pAIUnitIterator = CreateAIUnitIterator( pAIState );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAITacticalCommander::OnTurnStarted()
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAITacticalCommander::OnTurnFinished() 
{ 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAITacticalCommander::OnStartRealTime()
{
	CheckForReleasableUnits();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAITacticalCommander::OnPassControl( NWorld::CPlayer *pPlayer )
{
	CheckForReleasableUnits();
	if ( IsAITurn() )
		Think();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAITacticalCommander::IsPerformingAction() const
{
	return pAIState->IsPerformingAction();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAITacticalCommander::IsEndOfTurn() const
{
	return bEndOfTurn || !IsAITurn();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAITacticalCommander::Segment()
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAITacticalCommander::IsIdleJob()
{
	return !IsAITurn();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAITacticalCommander::IsJobFinished()
{
	return pAIUnitIterator->IsEnd() || GetWorld()->IsRealTime() || !IsAITurn();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAITacticalCommander::OnJobFinished()
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAITacticalCommander::ChooseLogic( IAIUnit *pUnit )
{
	ASSERT( IsValid( pUnit ) );
	if ( IsValid( pUnit ) )
		pUnit->SetLogic( CreateAttackAILogic( pAIState, pAILog, this ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAITacticalCommander::ThinkForNextGoodUnit()
{
	ASSERT( pAILog->IsEmpty() );
	while ( !pAIUnitIterator->IsEnd() && !IsValid( pAIState->GetCurrentAIUnit() ) )
	{
		pAIUnitIterator->Next();
	}
	if ( !pAIUnitIterator->IsEnd() )
	{
		CPtr<IAIUnit> pUnit = pAIState->GetCurrentAIUnit();
		ASSERT( IsValid( pUnit ) );
		if ( IsValid( pUnit ) )
		{
			pUnit->Synchronize();
			ChooseLogic( pUnit );
			CPtr<IAIJobManager> pManager = pAIState->GetWorld()->GetAIJobManager();
			CPtr<CAILogic> pLogic = pUnit->GetLogic();
			ASSERT( IsValid( pManager ) );
			if ( IsValid( pManager ) && IsValid( pLogic ) )
			{
				if ( pLogic->Think() )
					pManager->WaitForJob( this, pLogic );
			}
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAITacticalCommander::Think()
{
	CPtr<IAIJobManager> pManager = pAIState->GetWorld()->GetAIJobManager();
	ASSERT( IsValid( pManager ) );
	if ( !IsValid( pManager ) )
		return;
	//
	pAIState->GetAllyAIPlayer()->CancelActions();
	pManager->Remove( this );
	pAILog->Clear();
	commands.clear();
	// не наш ход или не осталось врагов или союзников
	if ( !IsAITurn() || 
		pAIState->GetEnemyAIPlayer()->GetUnits()->empty() || 
		pAIState->GetAllyAIPlayer()->GetUnits()->empty() )
	{
		bEndOfTurn = true;
		return;
	}
	//
	bEndOfTurn = false;
	pAIState->OnTurnStarted();
	pManager->Add( this );
	//
	pAIUnitIterator->First();
	ThinkForNextGoodUnit();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAITacticalCommander::DoJob()
{
	pAILog->GetCommands( &commands );
	pAILog->Clear();
	pAIUnitIterator->Next();
	ThinkForNextGoodUnit();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NWorld::CCommand* CAITacticalCommander::GetCommand()
{
	NWorld::CCommand *pRes = 0;
	if ( !bEndOfTurn && !GetWorld()->IsRealTime() &&
			 !( IsValid( pLastCommandedUnit ) && pLastCommandedUnit->IsPerformingAction() ) )
	{
		// выдаем следующую комманду
		if ( !commands.empty() )				
		{
			pRes = commands.front().Extract();
			commands.pop_front();
		}
		//
		if ( CDynamicCast<NWorld::CCmdUnit> pCmdUnit( pRes ) )
		{
			if ( CDynamicCast<NWorld::CUnitServer> pUnitServer( pCmdUnit->pUnit ) )	
			{
				pLastCommandedUnit = pUnitServer;
			}
		}
		//
		if ( commands.empty() && IsJobFinished() && !IsValid( pRes ) )
			bEndOfTurn = true;
	}
	return pRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAITacticalCommander::OnUnitWasKilled( NWorld::CUnitServer *pUnit )
{
	RemoveUnit( pUnit );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAITacticalCommander::RemoveUnit( NWorld::CUnitServer *pUS )
{
	ASSERT( IsValid( pUS ) );
	CPtr<IAIUnit> pAIUnit = pAICommander->GetAIUnit( pUS );
	DismissUnit( pAIUnit );
	if ( pAICommander->IsAITurn() )
		Think();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAITacticalCommander::IsUnderControl( IAIUnit *_pAIUnit )
{
	return pAIState->IsContain( _pAIUnit );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAITacticalCommander::DismissUnit( IAIUnit *_pAIUnit )
{
	ASSERT( IsValid( _pAIUnit ) );
	pAIState->GetEnemyAIPlayer()->RemoveUnit( _pAIUnit );
	pAIState->GetAllyAIPlayer()->RemoveUnit( _pAIUnit );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAITacticalCommander::DismissAllUnits()
{
	pAIState->GetWorld()->GetAIJobManager()->Remove( this );
	pAIState->GetEnemyAIPlayer()->GetUnits()->clear();
	pAIState->GetAllyAIPlayer()->GetUnits()->clear();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAITacticalCommander::AddAllyUnit( IAIUnit *_pAIUnit )
{
	ASSERT( IsValid( _pAIUnit ) );
	if ( IsValid( _pAIUnit ) )
	{
		pAIState->GetAllyAIPlayer()->AddUnit( _pAIUnit );
		_pAIUnit->GetUnitServer()->animator.SetCustomIdleAnimation( 0 ); // снимаем custom idle animation
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAITacticalCommander::AddEnemyUnit( IAIUnit *_pAIUnit )
{
	ASSERT( IsValid( _pAIUnit ) );
	if ( IsValid( _pAIUnit ) )
		pAIState->GetEnemyAIPlayer()->AddUnit( _pAIUnit );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAITacticalCommander::CheckForReleasableUnits()
{
	vector< CPtr<IAIUnit> > unitsToRelease;
	vector< CPtr<IAIUnit> > *pUnits = pAIState->GetAllyAIPlayer()->GetUnits();
	for ( vector< CPtr<IAIUnit> >::iterator i = pUnits->begin(); i != pUnits->end(); ++i )
	{
		CPtr<NWorld::CUnitServer> pUS = (*i)->GetUnitServer();
		bool bCheat = 
			pUS->IsCheatEnabled( NRPG::CHEAT_SCRIPTSEQUENCE ) || pUS->IsCheatEnabled( NRPG::CHEAT_NOAI );
		if ( bCheat || !(*i)->HasVisibleEnemies() || !pUS->CanFight() )
			unitsToRelease.push_back( *i );
	}
	//
	for ( vector< CPtr<IAIUnit> >::iterator i = unitsToRelease.begin(); i != unitsToRelease.end(); ++i )
	{
		CPtr<IAIUnit> pAIUnit = *i;
		pAIUnit->OnControlFinished();
		CPtr<NWorld::CUnitServer> pUS = pAIUnit->GetUnitServer();
		bool bCheat = 
			pUS->IsCheatEnabled( NRPG::CHEAT_SCRIPTSEQUENCE ) || pUS->IsCheatEnabled( NRPG::CHEAT_NOAI );
		if ( bCheat || !pUS->CanFight() )
			continue;
		//
		CPtr<CTask> pTask = new CTask( pUS, false );
		// идем к последнему месту где видели врага
		IAIUnit *pEnemy = 0;
		SPosition Position;
		pAIUnit->GetLastSeenEnemy( &Position, &pEnemy );
		if ( IsValid( pEnemy ) && 
			!pEnemy->GetUnitServer()->IsDead() && !pEnemy->GetUnitServer()->IsUnconscious() )
		{
			Position.p.SetPose( NAI::WALK );
			pTask->AddCommand( new CTaskCommandGoto( Position ) );
		}
		pTask->AddLookAround();

		IAIControl *pAIControl = CreateAITaskControl( pAICommander,	pTask, AI_CONTROL_ERASABLE, AIM_AI );
		pAICommander->GetAIUnit( pUS )->AssignControl( pAIControl );
	}
	//
	unitsToRelease.clear();
	if ( pAIState->GetAllyAIPlayer()->GetUnits()->empty() )
		pAIState->GetEnemyAIPlayer()->GetUnits()->clear();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAITacticalCommander::OnSeeUnit( NWorld::CUnitServer *pWatcher, NWorld::CUnitServer *pTarget )
{
	if ( pWatcher->IsCheatEnabled( NRPG::CHEAT_NOAI ) || pWatcher->IsCheatEnabled( NRPG::CHEAT_SCRIPTSEQUENCE ) )
		return;
	//
	if ( !pWatcher->CanFight() || !pTarget->CanFight() )
		return;
	//
	if ( pWatcher->GetPlayer() == pTarget->GetPlayer() )
		return;
	//
	CDynamicCast<CAICommander> pTmpCommander( pWatcher->GetPlayer()->GetCommander() );
	if ( pTmpCommander != pAICommander )
		return;
	//
	CPtr<IAIUnit> pAIWatcher = pAICommander->GetAIUnit( pWatcher );
	CPtr<IAIUnit> pAITarget = pAICommander->GetAIUnit( pTarget );
	if ( !IsValid( pAITarget ) || !IsValid( pAIWatcher ) )
		return;
	//
	if ( !IsUnderControl( pAITarget ) )
		AddEnemyUnit( pAITarget );
	if ( !IsUnderControl( pAIWatcher ) )
		pAIWatcher->AssignControl( CreateAITacticalControl( pAICommander, pAIWatcher, AIM_AI ) );
	//
	if ( IsUnderControl( pAIWatcher ) )
		pAIWatcher->SetLastSeenEnemy( pAITarget );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
IAIUnit* CAITacticalCommander::GetDangerousAttackableEnemy( IAIUnit *pAIUnit ) const
{
	ASSERT( IsValid( pAIUnit ) );
	if ( !IsValid( pAIUnit ) )
		return 0;
	//
	const float fDeltaDistance = 1.5f;
	float fGoodDistance = float( 0xFFF );
	list< CPtr< IAIUnit > > goodEnemies;
	//
	vector< CPtr<IAIUnit> > *pEnemyUnits;
	if ( pAIState->GetAllyAIPlayer()->IsContain( pAIUnit ) )
		pEnemyUnits = pAIState->GetEnemyAIPlayer()->GetUnits();
	else
		pEnemyUnits = pAIState->GetAllyAIPlayer()->GetUnits();
	//
	for ( vector< CPtr<IAIUnit> >::iterator i = pEnemyUnits->begin(); i != pEnemyUnits->end(); ++i )
	{
		CPtr<NWorld::CUnitServer> pUS = (*i)->GetUnitServer();
		if ( pUS->CanFight() )
		{
			float fDistance = fabs( (*i)->GetPosition().GetCP() - pAIUnit->GetPosition().GetCP() );
			if ( fabs( fDistance - fGoodDistance ) < fDeltaDistance )
			{
				goodEnemies.push_back( *i );
			}
			else if ( fDistance < fGoodDistance )
			{
				goodEnemies.clear();
				goodEnemies.push_back( *i );
				fGoodDistance = fDistance;
			}
		}
	}
	//
	int nBestD = -1;
	CPtr<IAIUnit> pGoodEnemy;
	for ( list< CPtr< IAIUnit > >::iterator i = goodEnemies.begin(); i != goodEnemies.end(); ++i )
	{
		int nD = 0, nMaxToHit, nHitCover;
		NDb::EShootMode eTmpShootMode;
		CPtr<CAIFireArmsWeapon> pTmpWeapon = 
			(*i)->GetAIInventory()->GetBestFireArms( (*i)->GetUnitPosition(), pAIUnit, (*i)->GetAP(), &nHitCover, &nD, &eTmpShootMode, &nMaxToHit );
		//
		if ( nD > nBestD || !IsValid( pGoodEnemy ) )
		{
			nBestD = nD;
			pGoodEnemy = *i;
		}
	}
	//
	return pGoodEnemy;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAITacticalCommander::Synchronize()
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NWorld::CWorld *CAITacticalCommander::GetWorld() const
{ 
	return pAICommander->GetWorld(); 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAITacticalCommander::IsAITurn() const
{ 
	return pAICommander->IsAITurn(); 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
CAITacticalCommander* CreateAITacticalCommander( CAICommander *pAICommander )
{
	ASSERT( IsValid( pAICommander ) );
	return new CAITacticalCommander( pAICommander );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
using namespace NAI;
//
REGISTER_SAVELOAD_CLASS( 0x51422140, CAITacticalCommander );
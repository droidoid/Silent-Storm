#include "StdAfx.h"
#include "GView.h"
#include "G2DView.h"
#include "Transform.h"
#include "rpgUnitInfo.h"
#include "wInterface.h"
#include "wUICommands.h"
#include "Sound.h"
#include "Interface.h"
#include "iMission.h"
#include "iCommonUI.h"
#include "iMissionUI.h"
#include "iMissionMovieUI.h"
#include "iMissionExec.h"
#include "..\Misc\StrProc.h"
#include "..\Input\Bind.h"
#include "..\DBFormat\DataAck.h"
#include "..\DBFormat\DataRPG.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataInterface.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI
{
const int
	N_FADE_STAGE_TIME	= 1000;
////////////////////////////////////////////////////////////////////////////////////////////////////
// CMissionMovieUI
////////////////////////////////////////////////////////////////////////////////////////////////////
CMissionMovieUI::CMissionMovieUI():
	bindCancel( "cancel" )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CMissionMovieUI::CMissionMovieUI( const SWindowInfo &sInfo, NGame::IMission *_pMission, CDesktopWindow *_pTransition ):
	CDesktopWindow( sInfo ), pMission( _pMission ), pTransition( _pTransition ),
	eStage( START ), sStageTime( 0 ), bindCancel( "cancel" )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMissionMovieUI::ShowDesktop()
{
	eStage = START;

	ShowWindow( SWTYPE_SHOW );
	pMission->PushDesktop( this );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMissionMovieUI::HideDesktop()
{
	eStage = FINISH;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMissionMovieUI::UpdateDesktop( const STime &sTime )
{
	switch( eStage )
	{
	case START:
		{
			eStage = FADEIN;
			sStageTime = sTime;
			nPanelsStateSave = pMission->GetPanelState( NGame::PANEL_ALL );
			pMission->SetPanelState( NGame::PANEL_ALL, false );
			pMission->SetCheatVisibility( true );
		}
	case FADEIN:
		{
			if ( sStageTime + N_FADE_STAGE_TIME > sTime )
			{
				float fCoeff = float( sTime - sStageTime ) / N_FADE_STAGE_TIME;
				pTopBackground->SetColor( NGfx::SPixel8888( 0, 0, 0, 0xFF * fCoeff ) );
				pBottomBackground->SetColor( NGfx::SPixel8888( 0, 0, 0, 0xFF * fCoeff ) );
				break;
			}

			eStage = SHOWSCRIPT;
			sStageTime = sTime;
			pTopBackground->SetColor( NGfx::SPixel8888( 0, 0, 0, 0xFF ) );
			pBottomBackground->SetColor( NGfx::SPixel8888( 0, 0, 0, 0xFF ) );
			pTransition->SetStyle( STYLE_VISIBLE, false );
			break;
		}
	case FINISH:
		{
			eStage = FADEOUT;
			sStageTime = sTime;
			pTransition->SetStyle( STYLE_VISIBLE, true );
		}
	case FADEOUT:
		{
			if ( sStageTime + N_FADE_STAGE_TIME > sTime )
			{
				float fCoeff = 1.0f - float( sTime - sStageTime ) / N_FADE_STAGE_TIME;
				pTopBackground->SetColor( NGfx::SPixel8888( 0, 0, 0, 0xFF * fCoeff ) );
				pBottomBackground->SetColor( NGfx::SPixel8888( 0, 0, 0, 0xFF * fCoeff ) );
				break;
			}

			pMission->SetCheatVisibility( false );
			pMission->SetPanelState( nPanelsStateSave, true );
			pMission->PopDesktop( this );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NGame::CUICmdExec* CMissionMovieUI::CreateExecutor( NWorld::CUICmd *pCmd )
{
	if ( CDynamicCast<NWorld::CUICmdTurn> pTurn( pCmd ) )
		return false;
	if ( CDynamicCast<NWorld::CUICmdUnit> pTurn( pCmd ) )
		return false;

	return NGame::CreateExecutor( pCmd, pMission );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMissionMovieUI::ProcessEvent( const NInput::SEvent &sEvent )
{
	if ( bindCancel.ProcessEvent( sEvent ) )
	{
		pMission->SetWaitForPartFinished( true );
		return true;
	}

	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMissionMovieUI::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_MOUSEMOVE:
		{
			GetInterface()->SetCursorInfo( SCursorInfo() );
			break;
		}
	case EVENT_TEMPLATELOADCOMPLETE:
		{
			pTopBackground = GetUIWindow<CImage>( this, "top_background" );
			pBottomBackground = GetUIWindow<CImage>( this, "bottom_background" );
			break;
		}
	}

	return CDesktopWindow::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CAckEvent* CMissionMovieUI::PlayAckEvent( const STime &sTime, NWorld::CAckEvent *pEvent )
{
	if ( IsValid( pEvent->pAckInfo ) && IsValid( pEvent->pUnit ) )
	{
		const NDb::SAckVoice &voice = pEvent->pAckInfo->GetVoice( pEvent->pUnit->GetRPG()->GetRPGPers()->nVoice );
		PlaySound( voice.pSound );
	}

	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
using namespace NUI;
REGISTER_SAVELOAD_CLASS( 0xB1212160, CMissionMovieUI );

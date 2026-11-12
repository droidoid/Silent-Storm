#include "stdafx.h"
//
#include "A5Script.h"
#include "aiControl.h"
#include "aiCommander.h"
#include "aiPosition.h"
#include "aiRoute.h"
#include "rpgUnitMission.h"
#include "aiUnit.h"
#include "aiTaskCommander.h"
#include "wUnitServer.h"
#include "wMain.h"
#include "wUnitGroup.h"
#include "scriptCommon.h"
#include "scriptPtr.h"
#include "wUnitCommands.h"
//
#include "scriptRoute.h"
//
namespace NScript
{
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( CreateRoute, "" )
	vector< CPtr<NAI::CAIRouteWaypoint> > waypoints;
	int nWaypoints = luaGetParamCount( pState );
	for ( int i = 1; i <= nWaypoints; ++i )
	{
		string szName = pScript->GetObject( i ).GetString();
		CPtr<NAI::CAIRouteWaypoint> pWaypoint = pScript->pWorld->GetWaypoint( szName );
		if ( IsValid( pWaypoint ) )
			waypoints.push_back( pWaypoint );
	}
	luaPushCObj( pState, new NAI::CAIRoute( waypoints ) );
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( UnitSetRoute, "uub" )
	CDynamicCast<NWorld::CUnitServer> pUS( luaParams[ 0 ].p );
	CDynamicCast<NAI::CAIRoute> pRoute( luaParams[ 1 ].p );
	bool bCircled = luaParams[ 2 ].b;
	if ( IsValid( pRoute ) && IsValid( pUS ) )
		NAI::SetUnitRoute( pUS, pRoute, bCircled, NAI::AIM_SCRIPT );
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( GroupSetRoute, "uub" )
	CDynamicCast<NWorld::CUnitGroup> pGroup( luaParams[ 0 ].p );
	CDynamicCast<NAI::CAIRoute> pRoute( luaParams[ 1 ].p );
	bool bCircled = luaParams[ 2 ].b;
	if ( IsValid( pGroup ) && IsValid( pRoute ) )
		NAI::SetGroupRoute( pGroup, pRoute, bCircled, NAI::AIM_SCRIPT );
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
static void SetOneWaypointRoute( NWorld::CUnitServer *pUS, NAI::CAIRouteWaypoint *pWaypoint )
{
	ASSERT( IsValid( pUS ) );
	ASSERT( IsValid( pWaypoint ) );
	if ( !IsValid( pUS ) || !IsValid( pWaypoint ) )
		return;
	//
	vector< CPtr<NAI::CAIRouteWaypoint> > waypoints;
	waypoints.push_back( pWaypoint );
	CPtr<NAI::CAIRoute> pRoute = new NAI::CAIRoute( waypoints );
	NAI::SetUnitRoute( pUS, pRoute, false, NAI::AIM_SCRIPT );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( UnitSetToWaypoint, "us" )
	if ( CDynamicCast<NWorld::CUnitServer> pUS( luaParams[ 0 ].p ) )
	{
		CPtr<NAI::CAIRouteWaypoint> pWaypoint = pScript->pWorld->GetWaypoint( luaParams[ 1 ].s );
		if ( IsValid( pWaypoint ) )
		{
			NAI::SUnitPosition unitPos;
			unitPos.pos = pWaypoint->pos;
			unitPos.bRun = false;
			unitPos.SetPose( NAI::WALK );
			pUS->SetPosition( unitPos );
			pUS->animator.PlaceUnit( unitPos );
			SetOneWaypointRoute( pUS, pWaypoint );
		}
	}
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( UnitMoveToWaypoint, "us" )
	if ( CDynamicCast<NWorld::CUnitServer> pUS( luaParams[ 0 ].p ) )
	{
		CPtr<NAI::CAIRouteWaypoint> pWaypoint = pScript->pWorld->GetWaypoint( luaParams[ 1 ].s );
		if ( IsValid( pWaypoint ) )
		{
			if ( CDynamicCast<NAI::CAICommander> pAICommander( pUS->GetPlayer()->GetCommander() ) )
			{
				// AI Unit
				SetOneWaypointRoute( pUS, pWaypoint );			
			}
			else
			{
				// Player unit
				NAI::SPosition pos = pWaypoint->pos;
				pUS->Do( new NWorld::CCmdSetCommand( pUS, new NWorld::CCmdPath( pos ) ) );
				pUS->Do( new NWorld::CCmdSetCommand( pUS, new NWorld::CCmdContinue() ) );
			}
		}
	}
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( UnitRoaming, "usn" )
	if ( CDynamicCast<NWorld::CUnitServer> pUS( luaParams[ 0 ].p ) )
	{
		CPtr<NAI::CAIRouteWaypoint> pWaypoint = pScript->pWorld->GetWaypoint( luaParams[ 1 ].s );
		if ( IsValid( pWaypoint ) )
			NAI::SetUnitRoaming( pUS, pWaypoint->pos.p, luaParams[ 2 ].n, NAI::AIM_SCRIPT );
	}
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
}
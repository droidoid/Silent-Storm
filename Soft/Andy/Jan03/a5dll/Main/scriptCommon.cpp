#include "stdafx.h"
#include "A5Script.h"
#include "wMain.h"
#include "wUnitServer.h"
#include "wUnitGroup.h"
#include "wOSBase.h"
#include "..\DBFormat\DataCamera.h"
#include "..\MiscDll\LogStream.h"
#include "..\Misc\RandomGen.h"
#include "rpgGlobal.h"
#include "scriptPtr.h"
#include "aiPosition.h"
#include "aiRoute.h"
#include "..\DBFormat\DataRPG.h"
#include "..\DBFormat\DataDifficulty.h"
#include "scriptCallLUA.h"
#include "scriptCommon.h"
#include "scriptPosition.h"
#include "..\Script\lstate.h"
#include "wDebris.h"
//
namespace NScript
{
////////////////////////////////////////////////////////////////////////////////////////////////////
int luaGetParamCount( lua_State* pState )
{
	ASSERT( pState != 0 );
	if ( pState == 0 )
		return 0;
	//
	return GetScript()->GetTop();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool luaPrepareData( lua_State* pState, 
	string szFuncName, string szParams, CScript **ppScript, vector<SLuaParams> *pParams )
{
	*ppScript = 0;
	ASSERT( pState != 0 );
	if ( pState == 0 )
		return false;
	*ppScript = GetScript();
	ASSERT( (*ppScript)->m_state == pState );
	if ( (*ppScript)->m_state != pState )
		return false;
	if ( szParams != "" && !(*ppScript)->CheckArgs( szParams.c_str(), szFuncName.c_str(), pParams ) )
		return false;
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static bool luaOutDBUserData( const Script::Object &o )
{
	if ( luaIsDBPtr<NDb::CDBCamera>( o ) )
	{
		CDBPtr<NDb::CDBCamera> pDBCamera = luaGetDBPtr<NDb::CDBCamera>( o );
		csScript << " CDBPtr -> Camera " << pDBCamera->GetRecordID() << endl;		
		return true;
	}
	//
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void luaOutUserData( void *pData )
{
	if ( GetScript() == 0 )
		return;
	CPtr<NWorld::CWorld> pWorld = GetScript()->pWorld;
	if ( !IsValid( pWorld ) )
		return;
	//
	CObjectBase *pObject = ( CObjectBase * )pData;
	if ( CDynamicCast<NWorld::CUnitServer> pUS( pObject ) )
	{
		string szName;
		pWorld->GetUnitName( pUS, &szName );
		csScript << CC_WHITE << "Unit [" << CC_YELLOW << szName << CC_WHITE << "]" << endl;
	}
	else if ( CDynamicCast<NWorld::CUnitGroup> pGroup( pObject ) )
		csScript << "UnitGroup " << pGroup->GetID() << endl;
	else if ( CDynamicCast<NAI::CAIRoute> pRoute( pObject ) )
		csScript << "Route " << endl;
	else if ( CDynamicCast<CLUAObjectPosition> pPos( pObject ) )
		csScript << "Position ( " << pPos->ptPos.x << ", " << pPos->ptPos.y << ", " << pPos->ptPos.z << " )"<< endl;
	else if ( CDynamicCast<NWorld::CObjectServerBase> pOS( pObject ) )
	{
		string szName;
		pWorld->GetObjectName( pOS, &szName );
		csScript << CC_WHITE << "Object [" << CC_YELLOW << szName << CC_WHITE << "]" << endl;
	}
	else if ( CDynamicCast<NWorld::CDFrozenItem> pItem( pObject ) )
	{
		string szName;
		pWorld->GetItemName( pItem, &szName );
		csScript << CC_WHITE << "Item [" << CC_YELLOW << szName << CC_WHITE << "]" << endl;
	}
	else
	{
		csScript << CC_RED << "[Script] error: Unregistered CPtr or CObj target" << endl;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int luaOut(lua_State* state)
{
	Script script(state);
	int nTop = script.GetTop();
	for ( int i = 1; i <= nTop; ++i )
	{
		Script::Object o = script.GetObject(i);
		if ( o.Tag() >= tagLuaCPtr )
		{
			switch ( o.Tag() )
			{
				case tagLuaCPtr:
				case tagLuaCObj:
					{
						if ( !luaOutDBUserData( o ) )
						{
							if ( o.Tag() == tagLuaCPtr )
								csScript << " CPtr -> ";
							else if ( o.Tag() == tagLuaCObj )
								csScript << " CObj -> ";
							luaOutUserData( luaGetPtr( o ) );
						}
					}
					break;
				default:
					csScript << CC_RED << "[Script] error: Incorrect user tag ( " << o.Tag() << " )" << endl;
					break;
			}
		}
		else if ( o.IsUserData() )
		{
			csScript << CC_RED << "[Script] error: User data" << endl;
			ASSERT( 0 ); // We can't work with user data
			break;
		}
		else if ( o.IsNil() )
			csScript << "NIL";
		else if ( o.IsString() )
			csScript << o.GetString();
		else
			ASSERT(0);
	}
	csScript << endl;
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int luaRandom( lua_State* state )
{
	Script script(state);
	if ( !script.GetObject( 1 ).IsNumber() )
		script.PushNumber( 0 );
	else if ( script.GetTop() == 0 )
		script.PushNumber( random.Get() );
	else if ( script.GetTop() > 1 )
		script.PushNumber( random.Get( script.GetObject(1).GetInteger(), script.GetObject(2).GetInteger() ) );
	else
		script.PushNumber( random.Get( script.GetObject(1).GetInteger() ) );
	return 1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( IsRealTime, "" )
	if ( pScript->pWorld->IsRealTime() )
		pScript->PushNumber( 1 );
	else
		pScript->PushNil();
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
bool luaGetBool( const Script::Object &o )
{
	if ( o.IsNil() )
		return false;
	else
		return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void luaMakeCallParamsVector( char *szParams, va_list *pL, vector< CObj<CLUACallParam> > *pParams )
{
	pParams->clear();
	for ( char *pCh = szParams; *pCh != ( char )0; ++pCh )
	{
		switch ( *pCh )
		{
			case 'i':
				pParams->push_back( new CLUACallParam( va_arg( *pL, int ) ) );
				break;
			case 'f':
				pParams->push_back( new CLUACallParam( va_arg( *pL, float ) ) );
				break;
			case 's':
				pParams->push_back( new CLUACallParam( string( va_arg( *pL, char * ) ) ) );
				break;
			case 'p':
				pParams->push_back( new CLUACallParam( va_arg( *pL, CObjectBase * ) ) );
				break;
			default:
				ASSERT( 0 );
				pParams->push_back( new CLUACallParam() );
				break;
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static const int N_NO_TOP = -0xFFF;
//
static bool luaPushCallParameters( string szName, 
	const vector< CObj<CLUACallParam> > &params, lua_State *pState )
{
	ASSERT( pState );
	if ( !pState )
		return false;
	//
	int nLuaTop = lua_gettop( pState );
	StkId stackTop = pState->pCT->top;
  lua_getglobal( pState, szName.c_str() );
	if ( lua_isfunction( pState, -1 ) )
	{
		for ( vector< CObj<CLUACallParam> >::const_iterator i = params.begin(); i != params.end(); ++i )
		{
			switch ( (*i)->type )
			{
				case CLUACallParam::PT_INT:
					lua_pushnumber( pState, (*i)->nInt );
					break;
				case CLUACallParam::PT_FLOAT:
					lua_pushnumber( pState, (*i)->fFloat );
					break;
				case CLUACallParam::PT_STRING:
					lua_pushstring( pState, (*i)->szString.c_str() );
					break;
				case CLUACallParam::PT_POINTER:
					luaPushCPtr( pState, (*i)->pObject );
					break;
				default:
					lua_pushnil( pState );
					ASSERT( 0 ); // unknown parameter type
					continue;
			}
		}
		//
		ASSERT( lua_gettop( pState ) - nLuaTop == params.size() + 1 ); // not all params was pushed in the stack
		return true;
	}
	else
	{
		lua_pop( pState, 1 ); // pop function from stack
		ASSERT( pState->pCT->top == stackTop ); // stack corrupted
		return false;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void luaCallFunction( string szName, const vector< CObj<CLUACallParam> > &params )
{
	lua_State *pState = 0;
	CScript *pScript = GetScript();
	if ( pScript )
		pState = pScript->GetState();
	if ( !pState )
	{
		ASSERT( 0 );
		ScriptError( "script is unavailable at the moment" );
		return;
	}
	//
	StkId stackTop = pState->pCT->top;
	//
	CLuaThread *pOld = pState->pCT;
	ASSERT( pOld );
	lua_setThread( pState, lua_newThread( pState ) );
	if ( luaPushCallParameters( szName, params, pState ) )
		lua_startThread( pState, params.size() );
	lua_setThread( pState, pOld );
	ASSERT( pState->pCT->top == stackTop ); // stack corrupted or current thread was changed
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void luaCallFunction( string szName, char *szParams, ... )
{
	vector< CObj<CLUACallParam> > params;
	va_list l;
	va_start( l, szParams );
	luaMakeCallParamsVector( szParams, &l, &params );
	va_end( l );
	luaCallFunction( szName, params );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
//BEGIN_SCRIPT_COMMAND( LuaTest, "ns[Hello!]n[100]b[false]b[true]" )
BEGIN_SCRIPT_COMMAND( LuaTest, "" )
	//csSystem << luaParams[ 0 ].n << endl;
	//csSystem << luaParams[ 1 ].s << endl;
	//csSystem << luaParams[ 2 ].f << endl;
	//csSystem << luaParams[ 3 ].b << endl;
	//csSystem << luaParams[ 4 ].b << endl;


	luaCallFunction( "UnexistentFunction", "ii", 100, 200 );
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( Explosion, "ns" )
	int nGrenadeID = luaParams[ 0 ].n;
	string szWaypoint = luaParams[ 1 ].s;
	//
	CPtr<NAI::CAIRouteWaypoint> pWaypoint = pScript->pWorld->GetWaypoint( szWaypoint );
	if ( IsValid( pWaypoint ) )
		pScript->pWorld->AddGrenadeExplosion( pWaypoint->pos.GetCP(), NDb::GetRPGGrenade( nGrenadeID ), 0 );
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( GetTurn, "" )
	pScript->PushNumber( pScript->pWorld->GetTurnID() );
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( Difficulty, "n" )
	pScript->pWorld->GetGlobalGame()->ChangeDifficulty( luaParams[ 0 ].n );
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( IsEqual, "uu" )
	if ( luaParams[ 0 ].p == luaParams[ 1 ].p )
		pScript->PushNumber( 1 );
	else
		pScript->PushNil();
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
void luaPushBool( lua_State *pState, bool bValue )
{
	if ( bValue )
		lua_pushnumber( pState, 1 );
	else
		lua_pushnil( pState );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
using namespace NScript;
REGISTER_SAVELOAD_CLASS( 0x51822131, CLUACallParam )
// gaps_m test stand: runs the Jan03 loaders (Main objects from C:\a5b\recon\c\stand\out\main) for the remaining map resource types.
// usage: stand2.exe <Type> <id> [<id> ...]   (cwd must contain <Type>\<id>, i.e. C:\a5b\recon\gaps_m\samples)
#include "StdAfx.h"
#include "GResource.h"
#include "aiObject.h"
#include "aiObjectLoader.h"
#include "aiWaypoint.h"
#include "GBind.h"
#include "GlobalInfo.h"
#include "GAnimLight.h"
#include "GAnimFormat.h"
#include "..\FileIO\FilesPackage.h"
#include <stdio.h>
#include <math.h>

using namespace NGScene;
static bool Finite3( const CVec3 &v ) { return _finite( v.x ) && _finite( v.y ) && _finite( v.z ) && fabs(v.x) < 1e6 && fabs(v.y) < 1e6 && fabs(v.z) < 1e6; }
static void WaitLoads() { for ( int i = 0; i < 2000 && HasFileRequestsInFly(); ++i ) Sleep( 1 ); Sleep( 5 ); }

struct TWp : public NAI::CWaypointLoader
{
	void Run( int n ) { SetKey( n ); Recalc(); NAI::CWaypoint *p = pValue;
		if ( !p ) { printf( "  value: NULL (exception)\n" ); return; }
		int nGoto = 0, nBad = 0; for ( int k = 0; k < p->commands.size(); ++k ) { if ( p->commands[k].cmd == NAI::CMD_GOTO ) ++nGoto; if ( !Finite3( p->commands[k].ptPos ) ) ++nBad; }
		printf( "  CWaypoint: pos=(%g,%g,%g) floor=%d rot=%d commands=%d goto=%d badPos=%d firstCmd=%d\n", p->ptPos.x, p->ptPos.y, p->ptPos.z, p->nFloor, p->fRotation,
			(int)p->commands.size(), nGoto, nBad, p->commands.empty() ? -1 : (int)p->commands[0].cmd ); }
};
static void PrintRoutes( NAI::CUnitAIInfo *p )
{
	if ( !p ) { printf( "  value: NULL (exception)\n" ); return; }
	int n = 0; for ( int k = 0; k < p->routes.size(); ++k ) n += p->routes[k].waypoints.size();
	printf( "  CUnitAIInfo: routes=%d waypointRefs=%d first=%d\n", (int)p->routes.size(), n, ( !p->routes.empty() && !p->routes[0].waypoints.empty() ) ? p->routes[0].waypoints[0] : -1 );
}
struct TUnit : public NAI::CUnitAIInfoLoader { void Run( int n ) { SetKey( n ); Recalc(); PrintRoutes( pValue ); } };
struct TGroup : public NAI::CUnitGroupAIInfoLoader { void Run( int n ) { SetKey( n ); Recalc(); PrintRoutes( pValue ); } };
struct TGlob : public CGlobalInfoLoader
{
	void Run( int n ) { SetKey( n ); Recalc(); CGlobalInfo *p = pValue; int nPts = 0;
		for ( int k = 0; k < p->sectorsSet.size(); ++k ) nPts += p->sectorsSet[k].pointsSet.size();
		printf( "  CGlobalInfo: nMapID=%d nScenarioID=%d sectors=%d points=%d firstImage=%d\n", p->nMapID, p->nScenarioID, (int)p->sectorsSet.size(), nPts,
			p->sectorsSet.empty() ? -1 : p->sectorsSet[0].nImageID ); }
};
static void PrintBind( const char *psz, CFileBindInfo *p )
{
	int nBad = 0; for ( int k = 0; k < p->invBindPoses.size(); ++k ) { const float *f = &p->invBindPoses[k]._11; for ( int j = 0; j < 16; ++j ) if ( !_finite( f[j] ) || fabs( f[j] ) > 1e6 ) { ++nBad; break; } }
	printf( "  %s: invBindPoses=%d badMatrices=%d m0._44=%g\n", psz, (int)p->invBindPoses.size(), nBad, p->invBindPoses.empty() ? 0.f : p->invBindPoses[0]._44 );
}
struct TBind : public CFileBind { void Run( int n ) { SetKey( n ); Recalc(); PrintBind( "CFileBindInfo(Binds)", pValue ); } };
struct TAIBind : public CFileAIBind { void Run( int n ) { SetKey( n ); Recalc(); PrintBind( "CFileBindInfo(AIBinds)", pValue ); } };
struct TLoc : public NAnimation::CFileLocators
{
	void Run( int n ) { SetKey( n ); Recalc(); NAnimation::CFileSkeletonInfo *p = pValue; int nEmpty = 0;
		for ( int k = 0; k < p->bones.size(); ++k ) if ( p->bones[k].szName.empty() ) ++nEmpty;
		printf( "  CFileSkeletonInfo(Locators): bones=%d emptyName=%d first='%s'\n", (int)p->bones.size(), nEmpty, p->bones.empty() ? "" : p->bones[0].szName.c_str() ); }
};
struct TLight : public CLightLoader
{
	void Run( int n ) { SetKey( n ); Recalc(); WaitLoads(); Recalc(); CAnimLightInfo *p = pValue;
		if ( !p ) { printf( "  value: NULL\n" ); return; }
		printf( "  CAnimLightInfo: fps=%g tStart=%g tEnd=%g posKeys=%d colorKeys=%d radiusKeys=%d", p->fFrameRate, p->fTStart, p->fTEnd,
			(int)p->pos.keys.size(), (int)p->color.keys.size(), (int)p->radius.keys.size() );
		if ( !p->color.keys.empty() ) printf( " color0=(%g,%g,%g)@%d", p->color.keys[0].value.x, p->color.keys[0].value.y, p->color.keys[0].value.z, (int)p->color.keys[0].nT );
		if ( !p->radius.keys.empty() ) printf( " radius0=%g", p->radius.keys[0].value );
		printf( "\n" ); }
};
static void CountTrees( const vector<NAI::CBSPPieces> &v, int *pPieces, int *pValid, int *pNull )
{
	for ( int k = 0; k < v.size(); ++k )
		for ( NAI::CBSPPieces::const_iterator i = v[k].begin(); i != v[k].end(); ++i ) { ++*pPieces; if ( IsValid( i->second ) ) ++*pValid; else ++*pNull; }
}
struct TBSP : public NAI::CLoadTwoBSPTrees
{
	void Run( int n ) { SetKey( n ); Recalc(); NAI::CTwoBSPTrees *p = pValue; int a = 0, b = 0, c = 0, d = 0, e = 0, f = 0;
		CountTrees( p->treesOpen, &a, &b, &c ); CountTrees( p->treesClosed, &d, &e, &f );
		printf( "  CTwoBSPTrees: open=%d (entries %d valid %d null %d) closed=%d (entries %d valid %d null %d)\n", (int)p->treesOpen.size(), a, b, c, (int)p->treesClosed.size(), d, e, f ); }
};
static int FileSize( const char *psz ) { WIN32_FILE_ATTRIBUTE_DATA a; if ( !GetFileAttributesExA( psz, GetFileExInfoStandard, &a ) ) return -1; return a.nFileSizeLow; }
static void RunOne( const char *t, int n )
{
	char sz[256]; sprintf( sz, "%s\\%d", t, n ); printf( "%s (%d bytes)\n", sz, FileSize( sz ) );
	try
	{
		if ( !strcmp( t, "Waypoints" ) ) { CObj<TWp> a = new TWp; a->Run( n ); }
		else if ( !strcmp( t, "Units" ) ) { CObj<TUnit> a = new TUnit; a->Run( n ); }
		else if ( !strcmp( t, "Groups" ) ) { CObj<TGroup> a = new TGroup; a->Run( n ); }
		else if ( !strcmp( t, "Globals" ) ) { CObj<TGlob> a = new TGlob; a->Run( n ); }
		else if ( !strcmp( t, "Binds" ) ) { CObj<TBind> a = new TBind; a->Run( n ); }
		else if ( !strcmp( t, "AIBinds" ) ) { CObj<TAIBind> a = new TAIBind; a->Run( n ); }
		else if ( !strcmp( t, "Locators" ) ) { CObj<TLoc> a = new TLoc; a->Run( n ); }
		else if ( !strcmp( t, "Lights" ) ) { CObj<TLight> a = new TLight; a->Run( n ); }
		else if ( !strcmp( t, "AIBSPTrees" ) ) { CObj<TBSP> a = new TBSP; a->Run( n ); }
		else printf( "  unknown type\n" );
	}
	catch ( ... ) { printf( "  C++ EXCEPTION escaped the loader\n" ); }
}
static int RunGuarded( const char *t, int n )
{
	__try { RunOne( t, n ); }
	__except ( EXCEPTION_EXECUTE_HANDLER ) { printf( "  SEH EXCEPTION 0x%08X (crash inside loader)\n", GetExceptionCode() ); return 1; }
	return 0;
}
int main( int argc, char **argv )
{
	if ( argc < 3 ) { printf( "usage: stand2 <Type> <id>...\n" ); return 2; }
	setvbuf( stdout, 0, _IONBF, 0 );
	RunResourceLoadingThread();
	AddResourceDir( "nopacks" );
	int nFail = 0;
	for ( int i = 2; i < argc; ++i ) nFail += RunGuarded( argv[1], atoi( argv[i] ) );
	fflush( stdout );
	ExitProcess( nFail );
}

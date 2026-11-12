// Recon test stand: runs the Jan03 resource loaders (linked statically from Main's object files) on sample files.
// usage: stand.exe <Type> <id> [<id> ...]   (cwd must contain <Type>\<id>, i.e. C:\a5b\recon\c\samples)
//        stand.exe Pack <file.res> [<resName> <id>]
#include "StdAfx.h"
#include "GResource.h"
#include "GGeometry.h"
#include "GObjectInfo.h"
#include "GAnimFormat.h"
#include "METerrain.h"
#include "BuildingInfo.h"
#include "aiObject.h"
#include "aiObjectLoader.h"
#include "GParticleFormat.h"
#include "ChapterInfo.h"
#include "..\FileIO\FilesPackage.h"
#include <stdio.h>
#include <math.h>

using namespace NGScene;

static bool Finite3( const CVec3 &v ) { return _finite( v.x ) && _finite( v.y ) && _finite( v.z ) && fabs(v.x) < 1e6 && fabs(v.y) < 1e6 && fabs(v.z) < 1e6; }

// ---- wait for the loader thread (CFileRequest path) ----
static void WaitLoads()
{
	for ( int i = 0; i < 2000 && HasFileRequestsInFly(); ++i )
		Sleep( 1 );
	Sleep( 5 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Geometries
struct TGeom : public CObjectInfoLoader
{
	void Run( int nFileID )
	{
		SetKey( SPartKey( nFileID & 0xffff, nFileID >> 16 ) );
		Recalc(); WaitLoads(); Recalc();
		CObjectInfo *p = pValue;
		if ( !p ) { printf( "  value: NULL\n" ); return; }
		const vector<CVec3> &pos = p->GetPositions();
		int nBad = 0;
		for ( int k = 0; k < pos.size(); ++k ) if ( !Finite3( pos[k] ) ) ++nBad;
		printf( "  CObjectInfo: verts(uv)=%d positions=%d weights=%d tris=%d badPositions=%d\n",
			(int)p->GetVertices().size(), (int)pos.size(), (int)p->GetWeights().size(), p->GetTrisCount(), nBad );
	}
};
struct TGeomPieces : public CObjectInfoPiecesLoader
{
	void Run( int nFileID )
	{
		SetKey( SPartKey( nFileID & 0xffff, nFileID >> 16 ) );
		Recalc(); WaitLoads(); Recalc();
		CObjectInfoPieces *p = pValue;
		if ( !p ) { printf( "  pieces: NULL\n" ); return; }
		int nPts = 0, nIdx = 0;
		for ( CPieceMap::iterator i = p->faces.begin(); i != p->faces.end(); ++i ) { nPts += i->second.points.size(); nIdx += i->second.polys.indices.size(); }
		printf( "  CObjectInfoPieces (chunk 4): pieces=%d points=%d indices=%d\n", (int)p->faces.size(), nPts, nIdx );
	}
};
// Skeletons / Animations
struct TSkel : public NAnimation::CFileSkeleton
{
	void Run( int nID )
	{
		SetKey( nID ); Recalc();
		NAnimation::CFileSkeletonInfo *p = pValue;
		int nBadParent = 0, nEmptyName = 0;
		for ( int k = 0; k < p->bones.size(); ++k )
		{
			if ( p->bones[k].nParent < -1 || p->bones[k].nParent >= (int)p->bones.size() ) ++nBadParent;
			if ( p->bones[k].szName.empty() ) ++nEmptyName;
		}
		printf( "  CFileSkeletonInfo: bones=%d bScale=%d badParent=%d emptyName=%d first='%s'\n", (int)p->bones.size(), (int)p->bScale,
			nBadParent, nEmptyName, p->bones.empty() ? "" : p->bones[0].szName.c_str() );
	}
};
struct TAnim : public NAnimation::CFileAnimation
{
	void Run( int nID )
	{
		SetKey( nID ); Recalc();
		NAnimation::CFileAnimationInfo *p = pValue;
		const NAnimation::SAnimationHeader &h = p->hdr;
		int nFrames = (int)h.fLength;
		printf( "  CFileAnimationInfo: indices=%d fLength=%g fps=%g roots=%d bones=%d addBones=%d bScale=%d | keysRoots=%d keysBones=%d keysAdd=%d keysMSR=%d\n",
			(int)h.indices.size(), h.fLength, h.fFrameRate, h.nRoots, h.nBones, h.nAddBones, (int)h.bScale,
			(int)p->keysRoots.size(), (int)p->keysBones.size(), (int)p->keysAddBones.size(), (int)p->keysMSR.size() );
		// consistency used by the Jan03 player (GAnimation.cpp indexes keys[frame*nBones + bone])
		printf( "    roots*frames=%d bones*frames=%d (frames=%d)\n", h.nRoots * nFrames, h.nBones * nFrames, nFrames );
	}
};
// Terrain
struct TTerr : public CMETerrainLoader
{
	void Run( int nID )
	{
		SetKey( nID ); Recalc();
		CMETerrainInfo *p = pValue;
		if ( !p ) { printf( "  value: NULL (exception)\n" ); return; }
		const STerrainInfo &t = p->info;
		printf( "  CMETerrainInfo: info.nWidth=%d nHeight=%d typeMap=%dx%d heightMap=%dx%d holes=%d grass=%d color=%dx%d spots=%d soundmap=%dx%d | alphaMap=%dx%d fMinH=%g fMaxH=%g\n",
			t.nWidth, t.nHeight, t.typeMap.GetXSize(), t.typeMap.GetYSize(), t.heightMap.GetXSize(), t.heightMap.GetYSize(),
			(int)t.holes.size(), (int)t.grass.size(), t.color.GetXSize(), t.color.GetYSize(), (int)t.spots.size(),
			t.soundmap.GetXSize(), t.soundmap.GetYSize(), p->alphaMap.GetXSize(), p->alphaMap.GetYSize(), p->fMinH, p->fMaxH );
	}
};
// Buildings
struct TBuild : public NBuilding::CBuildInfoLoader
{
	void Run( int nID )
	{
		SetKey( nID ); Recalc();
		NBuilding::CBuildInfo *p = pValue;
		printf( "  CBuildInfo: walls=%d solids=%d roomMap=%d spots=%d maxX=%d maxY=%d floors=%d..%d ladders=%d cellar=%dx%d lgroups=%d\n",
			(int)p->wallFragments.size(), (int)p->solidFragments.size(), (int)p->roomMap.size(), (int)p->spots.size(),
			p->nMaxX, p->nMaxY, p->nMinFloor, p->nMaxFloor, (int)p->ladders.size(), p->cellar.GetXSize(), p->cellar.GetYSize(), (int)p->lgroups.size() );
		if ( !p->solidFragments.empty() )
		{
			const NBuilding::SBuildFragment &f = p->solidFragments[0];
			printf( "    solid[0]: part=%d pos=(%g,%g,%g) rot=%d frag=%d id=%d\n", f.nConstructionPartID, f.ptPos.x, f.ptPos.y, f.ptPos.z, f.nRotationID, f.nFragmentID, f.nID );
		}
		if ( !p->wallFragments.empty() )
		{
			const NBuilding::SBuildFragment &f = p->wallFragments[0];
			printf( "    wall[0]: part=%d pos=(%g,%g,%g) rot=%d frag=%d id=%d\n", f.nConstructionPartID, f.ptPos.x, f.ptPos.y, f.ptPos.z, f.nRotationID, f.nFragmentID, f.nID );
		}
	}
};
// AIGeometries
struct TAIGeom : public NAI::CLoadGeometryInfo
{
	void Run( int nID )
	{
		SetKey( nID ); Recalc();
		NAI::CGeometryInfo *p = pValue;
		int nPts = 0, nTris = 0, nTrees = 0, nNullTrees = 0, nBad = 0;
		for ( NAI::CGeometryInfo::CPieceMap::iterator i = p->pieces.begin(); i != p->pieces.end(); ++i )
		{
			nPts += i->second.points.size(); nTris += i->second.edges.mesh.size();
			for ( int k = 0; k < i->second.points.size(); ++k ) if ( !Finite3( i->second.points[k] ) ) ++nBad;
			for ( int k = 0; k < i->second.trees.size(); ++k ) { ++nTrees; if ( !IsValid( i->second.trees[k] ) ) ++nNullTrees; }
		}
		printf( "  CGeometryInfo: pieces=%d points=%d meshTris=%d bspTrees=%d (null %d) spheres=%d badPoints=%d\n",
			(int)p->pieces.size(), nPts, nTris, nTrees, nNullTrees, (int)p->spheres.size(), nBad );
	}
};
struct TAISkin : public NAI::CFileSkinPointsLoad
{
	void Run( int nID )
	{
		SetKey( nID ); Recalc();
		NAI::CFileSkinPoints *p = pValue;
		int nPts = 0;
		for ( NAI::CFileSkinPoints::CBodypartsHash::iterator i = p->parts.begin(); i != p->parts.end(); ++i ) nPts += i->second.points.size();
		printf( "  CFileSkinPoints (skin view of same file): parts=%d points=%d spheres=%d\n", (int)p->parts.size(), nPts, (int)p->spheres.size() );
	}
};
// Effects
struct TFx : public CParticlesLoader
{
	void Run( int nID, int nFileSize )
	{
		SetKey( nID );
		Recalc(); WaitLoads(); Recalc();
		CParticlesInfo *p = pValue;
		if ( !p ) { printf( "  value: NULL\n" ); return; }
		int nBadOfs = 0, nKeys = 0;
		const char *pBase = (const char*)p->pData->GetStream()->GetBuffer() + 4;
		const char *pEnd = pBase + nFileSize - 4;
		for ( int k = 0; k < p->nParticles && k < 1000000; ++k )
		{
			SParticle &s = p->particles[k];
			if ( (const char*)(s.pos.keys + s.pos.nKeys) > pEnd || (const char*)s.pos.keys < pBase ) ++nBadOfs;
			if ( (const char*)(s.color.keys + s.color.nKeys) > pEnd || (const char*)s.color.keys < pBase ) ++nBadOfs;
			if ( (const char*)(s.sprite.keys + s.sprite.nKeys) > pEnd || (const char*)s.sprite.keys < pBase ) ++nBadOfs;
			nKeys += s.pos.nKeys;
		}
		printf( "  CParticlesInfo: nBytes=%d (file-4=%d) fTEnd=%g fps=%g nParticles=%d posKeys=%d tracksOutOfFile=%d\n",
			p->nBytes, nFileSize - 4, p->fTEnd, p->fFrameRate, p->nParticles, nKeys, nBadOfs );
	}
};
// Chapters
struct TChap : public CChapterInfoLoader
{
	void Run( int nID )
	{
		SetKey( nID ); Recalc();
		CChapterInfo *p = pValue;
		int nPts = 0;
		for ( int k = 0; k < p->sectorsSet.size(); ++k ) nPts += p->sectorsSet[k].pointsSet.size();
		printf( "  CChapterInfo: nMapID=%d deploy=(%g,%g) sectors=%d sectorPoints=%d first='%s'\n", p->nMapID, p->vDeployPos.x, p->vDeployPos.y,
			(int)p->sectorsSet.size(), nPts, p->sectorsSet.empty() ? "" : p->sectorsSet[0].szID.c_str() );
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
static int FileSize( const char *psz )
{
	WIN32_FILE_ATTRIBUTE_DATA a;
	if ( !GetFileAttributesExA( psz, GetFileExInfoStandard, &a ) ) return -1;
	return a.nFileSizeLow;
}
static void RunOne( const char *pszType, int nID )
{
	char szName[256];
	sprintf( szName, "%s\\%d", pszType, nID );
	printf( "%s (%d bytes)\n", szName, FileSize( szName ) );
	try
	{
		if ( !strcmp( pszType, "Geometries" ) ) { CObj<TGeom> a = new TGeom; a->Run( nID ); CObj<TGeomPieces> b = new TGeomPieces; b->Run( nID ); }
		else if ( !strcmp( pszType, "Skeletons" ) ) { CObj<TSkel> a = new TSkel; a->Run( nID ); }
		else if ( !strcmp( pszType, "Animations" ) ) { CObj<TAnim> a = new TAnim; a->Run( nID ); }
		else if ( !strcmp( pszType, "Terrain" ) ) { CObj<TTerr> a = new TTerr; a->Run( nID ); }
		else if ( !strcmp( pszType, "Buildings" ) ) { CObj<TBuild> a = new TBuild; a->Run( nID ); }
		else if ( !strcmp( pszType, "AIGeometries" ) ) { CObj<TAIGeom> a = new TAIGeom; a->Run( nID ); CObj<TAISkin> b = new TAISkin; b->Run( nID ); }
		else if ( !strcmp( pszType, "Effects" ) ) { CObj<TFx> a = new TFx; a->Run( nID, FileSize( szName ) ); }
		else if ( !strcmp( pszType, "Chapters" ) ) { CObj<TChap> a = new TChap; a->Run( nID ); }
		else printf( "  unknown type\n" );
	}
	catch ( ... )
	{
		printf( "  C++ EXCEPTION escaped the loader\n" );
	}
}
static int RunGuarded( const char *pszType, int nID )
{
	__try { RunOne( pszType, nID ); }
	__except ( EXCEPTION_EXECUTE_HANDLER ) { printf( "  SEH EXCEPTION 0x%08X (crash inside loader)\n", GetExceptionCode() ); return 1; }
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void RunPack( const char *pszFile, const char *pszRes, int nID )
{
	IFilesPackage *p = OpenFilesPackage( pszFile );
	printf( "OpenFilesPackage(%s) -> %s\n", pszFile, p ? "OK" : "NULL (failed)" );
	if ( !p || !pszRes ) return;
	printf( "  DoesFileExist(%d) = %d\n", nID, (int)DoesFileExist( p, nID ) );
	// read the entry through the real resource path (package first, as GResource.cpp does)
	AddResourceDir( "packdir" );
	if ( !strcmp( pszRes, "Terrain" ) ) { CObj<TTerr> a = new TTerr; a->Run( nID ); }
	else if ( !strcmp( pszRes, "Buildings" ) ) { CObj<TBuild> a = new TBuild; a->Run( nID ); }
	else if ( !strcmp( pszRes, "Chapters" ) ) { CObj<TChap> a = new TChap; a->Run( nID ); }
}
static int RunPackGuarded( const char *f, const char *r, int n )
{
	__try { RunPack( f, r, n ); }
	__except ( EXCEPTION_EXECUTE_HANDLER ) { printf( "  SEH EXCEPTION 0x%08X\n", GetExceptionCode() ); return 1; }
	return 0;
}
int main( int argc, char **argv )
{
	if ( argc < 3 ) { printf( "usage: stand <Type> <id>... | stand Pack <file> [<res> <id>]\n" ); return 2; }
	setvbuf( stdout, 0, _IONBF, 0 );
	RunResourceLoadingThread();
	AddResourceDir( "nopacks" ); // <dir>\<Type>.res does not exist -> loose files are used
	if ( !strcmp( argv[1], "Pack" ) )
		return RunPackGuarded( argv[2], argc > 4 ? argv[3] : 0, argc > 4 ? atoi( argv[4] ) : 0 );
	int nFail = 0;
	for ( int i = 2; i < argc; ++i )
		nFail += RunGuarded( argv[1], atoi( argv[i] ) );
	fflush( stdout );
	ExitProcess( nFail );   // skip global destructors of the engine
}

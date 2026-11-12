#include "StdAfx.h"
#include "LSHead.h"
#include "GResource.h"
#include "GGeometry.h"
#include "..\Misc\BasicShare.h"
#include "..\DBFormat\DataFormat.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NLSHead
{
CBasicShare<int, CHeadMeshLoader> shareHeads(135);
static CBasicShare<int, CHeadSequenceLoader> shareSequences(137);
static CLSPtr<LifeStudioHeadAPI::IMMTree> pLSTree;
////////////////////////////////////////////////////////////////////////////////////////////////////
static void LoadLSTree()
{
	if ( pLSTree )
		return;
	try
	{
		pLSTree = LifeStudioHeadAPI::IMMTree::Create();
		pLSTree->Load( "tree.mma" );
	}
	catch(...)
	{
		OutputDebugString( "Exception: LoadLSTree()" );
		return;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CHeadMeshLoader
////////////////////////////////////////////////////////////////////////////////////////////////////
void CHeadMeshLoader::Recalc()
{
	try
	{
		NGScene::CResourceOpener file( "Heads", GetKey() );
		pValue = new CHeadMeshInfo;
		vector<CMemoryStream> streams;
		file->Add( 1, &streams );
		file->Add( 2, &pValue->nVertices );
		file->Add( 3, &pValue->copys );
		file->Add( 4, &pValue->UVs );
		file->Add( 5, &pValue->indices );
		file->Add( 6, &pValue->tris );
		pValue->pLSAnimators.resize( streams.size() );
		for ( int i = 0; i < streams.size(); ++i )
		{
			pValue->pLSAnimators[i] = LifeStudioHeadAPI::IAnimator::Create();
			pValue->pLSAnimators[i]->Load( (const char *)streams[i].GetBuffer(), streams[i].GetSize() );
			LoadLSTree();
			if ( pLSTree )
				pValue->pLSAnimators[i]->RegisterMacroMuscle( pLSTree->RootMacroMuscle() );
		}
	}
	catch(...)
	{
		OutputDebugString( "Exception: CHeadMeshLoader::Recalc()" );
		return;
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CHeadSequenceLoader
////////////////////////////////////////////////////////////////////////////////////////////////////
void CHeadSequenceLoader::Recalc()
{
	try
	{
		NGScene::CResourceOpener file( "Sequences", GetKey() );
		pValue = new CHeadSequenceInfo;
		CMemoryStream stream;
		file->Add( 1, &stream );
		pValue->pLSSequence = LifeStudioHeadAPI::ISequencer::Create();
		pValue->pLSSequence->Load( (const char *)stream.GetBuffer(), stream.GetSize() );
		LoadLSTree();
		if ( pLSTree )
			pValue->pLSSequence->RegisterMMTree( pLSTree );
	}
	catch(...)
	{
		OutputDebugString( "Exception: CHeadSequenceLoader::Recalc()" );
		return;
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CHeadBound
////////////////////////////////////////////////////////////////////////////////////////////////////
void CHeadBound::Recalc()
{
	const SFBTransform &trans = pParent->GetValue();
	CVec3 pt = trans.forward.GetTranslation();
	value.SphereInit( pt, 1 ); // CRAP
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CHeadAnimator
////////////////////////////////////////////////////////////////////////////////////////////////////
CHeadAnimator::CHeadAnimator( CFuncBase<STime> *_pTime, NDb::CHead *_pDbHead ): pTime(_pTime), pDbHead(_pDbHead)
{
	pHead = shareHeads.Get( pDbHead->GetRecordID() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CHeadAnimator::PlaySequence( NDb::CSequence *pDbSeq, STime _tStart, bool _bCycle )
{
	if ( !pDbSeq )
		return;
	pSequence = shareSequences.Get( pDbSeq->GetRecordID() );
	tStart = _tStart;
	bCycle = _bCycle;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CHeadAnimator::StopSequence()
{
	pSequence = 0;
	tStart = 0;
	bCycle = false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CHeadAnimator::Recalc()
{
	CHeadMeshInfo *pMesh = pHead->GetValue();
	value.mesh.resize( pMesh->UVs.size() );
	value.normals.resize( pMesh->UVs.size() );
	memset( &value.normals[0], 0, sizeof(CVec3) * value.normals.size() );

	STime t = pTime->GetValue();
	CHeadSequenceInfo *pSeq = 0;
	int nTime = 0;
	if ( IsValid(pSequence) )
	{
		pSequence.Refresh();
		pSeq = pSequence->GetValue();
		if ( pSeq )
			nTime = pSeq->pLSSequence->SequenceTime();
		if ( !nTime || ( !bCycle && t >= tStart && t - tStart > nTime ) )
		{
			pSeq = 0;
			pSequence = 0;
		}
	}

	int nVert = 0;
	for ( int i = 0; i < pMesh->pLSAnimators.size(); ++i )
	{
		LifeStudioHeadAPI::IAnimator *pLSAnimator = pMesh->pLSAnimators[i];
		pLSAnimator->ClearAllMacroMuscles();
		if ( pSeq && t >= tStart )
		{
			if ( bCycle )
				pSeq->pLSSequence->RenderMacroMuscles( pLSAnimator, (t - tStart) % nTime );
			else
				pSeq->pLSSequence->RenderMacroMuscles( pLSAnimator, t - tStart );
		}
		pLSAnimator->ComputePhysics();
		pLSAnimator->FillUnused( true );
		pLSAnimator->Process( &(value.mesh[nVert].x), 3 );
		nVert += pMesh->nVertices[i];
	}

	for ( int i = 0; i < pMesh->copys.size(); ++i )
		value.mesh[ pMesh->copys[i].y ] = value.mesh[ pMesh->copys[i].x ];

	int nFrom = 0;
	int nTo = 0;
	for ( int i = 0; i < pMesh->tris.size(); ++i )
	{
		nTo = pMesh->tris[i];
		CVec3 &v1 = value.mesh[ pMesh->indices[nFrom] ];
		CVec3 &v2 = value.mesh[ pMesh->indices[nFrom+1] ];
		CVec3 &v3 = value.mesh[ pMesh->indices[nFrom+2] ];
		CVec3 normal = (v2 - v1) ^ (v3 - v1);
		Normalize(&normal);
		for ( int j = nFrom; j < nTo; ++j )
			value.normals[ pMesh->indices[j] ] += normal;
		nFrom = nTo;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CHead
////////////////////////////////////////////////////////////////////////////////////////////////////
void CHead::Recalc()
{
	CHeadMeshInfo *pMesh = pHead->GetValue();
	const SHeadFrame &frame = pAnimator->GetValue();
	int nVertices = frame.mesh.size();
	ASSERT( nVertices == pMesh->UVs.size() );
	if ( nVertices > pMesh->UVs.size() )
		nVertices = pMesh->UVs.size();
	pValue = new NGScene::CObjectInfo;
	NGScene::CObjectInfo::SData res;
	res.verts.resize( nVertices );
	NGScene::SVertex *pRes = &res.verts[0];
	res.geometry.polys.resize( pMesh->tris.size() );
	for ( int k = 0; k < res.geometry.polys.size(); ++k )
		res.geometry.polys[k] = pMesh->tris[k];
	res.geometry.indices = pMesh->indices;

	const SFBTransform &trans = pParent->GetValue();

	for ( int i = 0; i < nVertices; ++i, ++pRes )
	{
		CVec3 normal;
		trans.forward.RotateVector( &normal, frame.normals[i] );
		Normalize( &normal );
		CVec3 b1 = CVec3( 1, 0, 0 ), b2;
		if ( fabs2(normal.x) > 1e-12f )
		{
			b1 = CVec3( normal.y, -normal.x, 0 );
			Normalize( &b1 );
		}
		b2 = b1 ^ normal;
		// fill vertex
		trans.forward.RotateHVector( &(pRes->pos), frame.mesh[i] );
		pRes->normal = normal;
		pRes->tex = pMesh->UVs[i];
		pRes->texU = b1;
		pRes->texV = b2;
	}
	pValue->AssignFast( res );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
using namespace NLSHead;
REGISTER_SAVELOAD_CLASS( 0x10942150, CHeadBound )
REGISTER_SAVELOAD_CLASS( 0x10942151, CHeadAnimator )
REGISTER_SAVELOAD_CLASS( 0x10942152, CHead )
REGISTER_SAVELOAD_CLASS( 0x11042141, CHeadMeshLoader )
REGISTER_SAVELOAD_CLASS( 0x11042142, CHeadSequenceLoader )

#ifndef __aiPathTable_H_
#define __aiPathTable_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

namespace NAI 
{
////////////////////////////////////////////////////////////////////////////////////////////////////
class CPathPlaceTable
{
public: // VS2022 port: the typedefs are used from outside the class (the old compiler did not check access)
	typedef SMoveInfo<SPathPlace, WORD> SMove;
	typedef hash_map<SPathPlace, SMove, SPathPlaceHash> CMovesHash;
public:
	ZDATA
	CMovesHash data;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&data); return 0; }
	SMove& operator[]( const SPathPlace &_pt ) { return data[ _pt ]; }
	WORD GetCost( const SPathPlace &pt ) 
	{ 
		hash_map<SPathPlace, SMove, SPathPlaceHash>::const_iterator i = data.find( pt );
		if ( i == data.end() ) 
			return 65535;
		return (i->second).cost;
	}
	void MakeInfinity(void) { data.clear(); }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
}

#endif
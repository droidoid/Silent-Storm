// VS2022 port: null implementation of the LifeStudio:HEAD stand-in (see LifeStudioHeadAPI.h).
// Objects can be created, loaded and destroyed; nothing is parsed and nothing is animated:
//  - IAnimator::Process() leaves the vertex buffer untouched (the caller clears it, see
//    CHeadAnimator::Recalc in Main/LSHead.cpp),
//  - ISequencer::SequenceTime() returns 0, which the caller treats as "sequence finished".
#include "LifeStudioHeadAPI.h"

namespace LifeStudioHeadAPI
{
namespace
{
class CNullMMTree : public IMMTree
{
public:
	virtual void Destroy() { delete this; }
	virtual void Load( const char* ) {}
	virtual IMacroMuscle* RootMacroMuscle() { return 0; }
};

class CNullAnimator : public IAnimator
{
public:
	virtual void Destroy() { delete this; }
	virtual void Load( const char*, int ) {}
	virtual void RegisterMacroMuscle( IMacroMuscle* ) {}
	virtual void ClearAllMacroMuscles() {}
	virtual void ComputePhysics() {}
	virtual void FillUnused( bool ) {}
	virtual void Process( float*, int ) {}
};

class CNullSequencer : public ISequencer
{
public:
	virtual void Destroy() { delete this; }
	virtual void Load( const char*, int ) {}
	virtual void RegisterMMTree( IMMTree* ) {}
	virtual int SequenceTime() { return 0; }
	virtual void RenderMacroMuscles( IAnimator*, int ) {}
};
}

IMMTree* IMMTree::Create() { return new CNullMMTree; }
IAnimator* IAnimator::Create() { return new CNullAnimator; }
ISequencer* ISequencer::Create() { return new CNullSequencer; }
}

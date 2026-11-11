// VS2022 port: stand-in for the LifeStudio:HEAD SDK (closed third-party SDK, not available).
// Written from scratch against the calls made by Main/LSHead.cpp; it is NOT the vendor header.
// Only the names and members the A5 sources use are declared, with signatures implied by the
// call sites. Implementation: LifeStudioNull.cpp (null objects - no facial animation).
// Selected by the CMake option A5_NULL_LIFESTUDIO (default ON).
#ifndef __A5_LIFESTUDIO_NULL_STUB_H_
#define __A5_LIFESTUDIO_NULL_STUB_H_

namespace LifeStudioHeadAPI
{
// opaque handle, only passed from IMMTree::RootMacroMuscle() to IAnimator::RegisterMacroMuscle()
class IMacroMuscle;

class IMMTree
{
public:
	static IMMTree* Create();
	virtual void Destroy() = 0;
	virtual void Load( const char *pszFileName ) = 0;
	virtual IMacroMuscle* RootMacroMuscle() = 0;
protected:
	virtual ~IMMTree() {}
};

class IAnimator
{
public:
	static IAnimator* Create();
	virtual void Destroy() = 0;
	virtual void Load( const char *pData, int nSize ) = 0;
	virtual void RegisterMacroMuscle( IMacroMuscle *pRoot ) = 0;
	virtual void ClearAllMacroMuscles() = 0;
	virtual void ComputePhysics() = 0;
	virtual void FillUnused( bool bFill ) = 0;
	// writes the animated vertex positions; the null implementation writes nothing
	virtual void Process( float *pVertices, int nFloatsPerVertex ) = 0;
protected:
	virtual ~IAnimator() {}
};

class ISequencer
{
public:
	static ISequencer* Create();
	virtual void Destroy() = 0;
	virtual void Load( const char *pData, int nSize ) = 0;
	virtual void RegisterMMTree( IMMTree *pTree ) = 0;
	// length of the sequence; the null implementation returns 0 ("no sequence")
	virtual int SequenceTime() = 0;
	virtual void RenderMacroMuscles( IAnimator *pAnimator, int nTime ) = 0;
protected:
	virtual ~ISequencer() {}
};
}

#endif

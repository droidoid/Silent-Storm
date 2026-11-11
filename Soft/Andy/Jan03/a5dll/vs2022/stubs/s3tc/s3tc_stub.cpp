// A5 VS2022 port - stub for the missing S3TC encoder library (see s3tc.h in this directory).
// It does not compress: it reports that DXT encoding is unavailable. No fake output is produced.
#include <windows.h>
#include <stdio.h>

static void Report()
{
	static const char szMsg[] = "A5: DXT (S3TC) compression is not available in this build - s3tc.lib is replaced by a stub\n";
	OutputDebugStringA( szMsg );
	fputs( szMsg, stderr );
}

extern "C" {
unsigned int S3TCgetEncodeSize( struct _DDSURFACEDESC *, unsigned int ) { Report(); return 0; }
void S3TCsetAlphaReference( int ) {}
void S3TCencode( struct _DDSURFACEDESC *, void *, struct _DDSURFACEDESC *, void *, unsigned int, float * ) { Report(); }
}

// A5 VS2022 port - silent "null device" implementation of the FMOD calls declared in fmod.h (this directory).
// Compiled into FModSound when A5_NULL_SOUND is ON. Initialisation, loading and playing succeed and do
// nothing; nothing is ever "playing", so the sound objects of the game are retired on the next update.
// No sound data is parsed, no file is opened, stream synch callbacks are never invoked.
#include "fmod.h"

struct FSOUND_SAMPLE { unsigned int mode; };
struct FSOUND_STREAM { unsigned int mode; };

static int nNextChannel = 0;
// channel handles stay unique for a long time: the game keys a map by them
static int NewChannel() { nNextChannel = ( nNextChannel + 1 ) & 0x3FFFFFFF; return nNextChannel; }

extern "C" {

float        FSOUND_GetVersion( void ) { return FMOD_VERSION; }
signed char  FSOUND_SetOutput( int ) { return 1; }
signed char  FSOUND_SetDriver( int ) { return 1; }
signed char  FSOUND_SetHWND( void * ) { return 1; }
int          FSOUND_GetNumDrivers( void ) { return 1; }
const char  *FSOUND_GetDriverName( int ) { return "No sound (A5 null device)"; }
signed char  FSOUND_GetDriverCaps( int, unsigned int *caps ) { if ( caps ) *caps = 0; return 1; }
signed char  FSOUND_Init( int, int, unsigned int ) { return 1; }
void         FSOUND_Close( void ) {}
void         FSOUND_Update( void ) {}
int          FSOUND_GetMixer( void ) { return FSOUND_MIXER_QUALITY_FPU; }
int          FSOUND_GetError( void ) { return FMOD_ERR_NONE; }
void         FSOUND_SetSFXMasterVolume( int ) {}
void         FSOUND_SetSpeakerMode( unsigned int ) {}

FSOUND_SAMPLE *FSOUND_Sample_Load( int, const char *, unsigned int mode, int )
{
	FSOUND_SAMPLE *p = new FSOUND_SAMPLE;
	p->mode = mode;
	return p;
}
void         FSOUND_Sample_Free( FSOUND_SAMPLE *sptr ) { delete sptr; }
signed char  FSOUND_Sample_SetMode( FSOUND_SAMPLE *sptr, unsigned int mode ) { if ( sptr ) sptr->mode = mode; return 1; }
signed char  FSOUND_Sample_SetDefaults( FSOUND_SAMPLE *, int, int, int, int ) { return 1; }
signed char  FSOUND_Sample_SetMinMaxDistance( FSOUND_SAMPLE *, float, float ) { return 1; }
unsigned int FSOUND_Sample_GetLength( FSOUND_SAMPLE * ) { return 0; }

int          FSOUND_PlaySound( int, FSOUND_SAMPLE * ) { return NewChannel(); }
int          FSOUND_PlaySoundEx( int, FSOUND_SAMPLE *, void *, signed char ) { return NewChannel(); }
signed char  FSOUND_StopSound( int ) { return 1; }
signed char  FSOUND_SetVolume( int, int ) { return 1; }
signed char  FSOUND_SetVolumeAbsolute( int, int ) { return 1; }
int          FSOUND_GetVolume( int ) { return 0; }
signed char  FSOUND_SetPan( int, int ) { return 1; }
signed char  FSOUND_SetPaused( int, signed char ) { return 1; }
signed char  FSOUND_IsPlaying( int ) { return 0; }
unsigned int FSOUND_GetLoopMode( int ) { return FSOUND_LOOP_OFF; }
unsigned int FSOUND_GetCurrentPosition( int ) { return 0; }
int          FSOUND_GetPriority( int ) { return 0; }

signed char  FSOUND_3D_SetAttributes( int, float *, float * ) { return 1; }
void         FSOUND_3D_Listener_SetAttributes( float *, float *, float, float, float, float, float, float ) {}

FSOUND_STREAM *FSOUND_Stream_OpenFile( const char *, unsigned int mode, int )
{
	FSOUND_STREAM *p = new FSOUND_STREAM;
	p->mode = mode;
	return p;
}
signed char  FSOUND_Stream_Close( FSOUND_STREAM *stream ) { delete stream; return 1; }
int          FSOUND_Stream_Play( int, FSOUND_STREAM * ) { return NewChannel(); }
signed char  FSOUND_Stream_Stop( FSOUND_STREAM * ) { return 1; }
signed char  FSOUND_Stream_SetTime( FSOUND_STREAM *, int ) { return 1; }
int          FSOUND_Stream_GetTime( FSOUND_STREAM * ) { return 0; }
signed char  FSOUND_Stream_SetSynchCallback( FSOUND_STREAM *, FSOUND_STREAMCALLBACK, int ) { return 1; }

}

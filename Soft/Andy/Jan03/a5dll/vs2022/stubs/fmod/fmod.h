/* A5 VS2022 port - stand-in for the FMOD 3 SDK header (the SDK is not part of the repository).
 *
 * Written from scratch against the calls made by FModSound/FMSound.cpp; it declares only what that file
 * uses, with the signatures the call sites imply. It is NOT the vendor header: constant values are private
 * to this stub and it is not binary compatible with fmod.dll. The matching implementation is
 * fmod_null.cpp (a silent "null device"). To use the real SDK configure with -DA5_NULL_SOUND=OFF
 * -DA5_FMOD_INCLUDE_DIR=... -DA5_FMOD_LIBRARY=... so that this directory is not on the include path.
 */
#ifndef A5_STUB_FMOD_H
#define A5_STUB_FMOD_H

#define A5_FMOD_NULL_DEVICE 1

#ifdef __cplusplus
extern "C" {
#endif

#define FMOD_VERSION 3.0f

struct FSOUND_SAMPLE;
struct FSOUND_STREAM;
#ifndef __cplusplus
typedef struct FSOUND_SAMPLE FSOUND_SAMPLE;
typedef struct FSOUND_STREAM FSOUND_STREAM;
#endif

/* sample / stream mode flags (bit mask) */
#define FSOUND_LOOP_OFF     0x0001
#define FSOUND_LOOP_NORMAL  0x0002
#define FSOUND_2D           0x0004
#define FSOUND_HW3D         0x0008
#define FSOUND_LOADMEMORY   0x0010

/* special sample-slot / channel / pan values */
#define FSOUND_FREE         (-1)
#define FSOUND_UNMANAGED    (-2)
#define FSOUND_STEREOPAN    (-1)

enum { FSOUND_OUTPUT_NOSOUND, FSOUND_OUTPUT_WINMM, FSOUND_OUTPUT_DSOUND, FSOUND_OUTPUT_A3D };
enum { FSOUND_MIXER_BLENDMODE, FSOUND_MIXER_MMXP5, FSOUND_MIXER_MMXP6,
       FSOUND_MIXER_QUALITY_FPU, FSOUND_MIXER_QUALITY_MMXP5, FSOUND_MIXER_QUALITY_MMXP6 };
enum { FSOUND_SPEAKERMODE_MONO, FSOUND_SPEAKERMODE_STEREO, FSOUND_SPEAKERMODE_HEADPHONES,
       FSOUND_SPEAKERMODE_QUAD, FSOUND_SPEAKERMODE_SURROUND, FSOUND_SPEAKERMODE_DOLBYDIGITAL };

/* driver capability bits */
#define FSOUND_CAPS_HARDWARE 0x0001
#define FSOUND_CAPS_EAX2     0x0002

enum { FMOD_ERR_NONE = 0 };

typedef signed char (__cdecl *FSOUND_STREAMCALLBACK)( struct FSOUND_STREAM *stream, void *buff, int len, int param );

/* system */
float        FSOUND_GetVersion( void );
signed char  FSOUND_SetOutput( int outputtype );
signed char  FSOUND_SetDriver( int driver );
signed char  FSOUND_SetHWND( void *hwnd );
int          FSOUND_GetNumDrivers( void );
const char  *FSOUND_GetDriverName( int id );
signed char  FSOUND_GetDriverCaps( int id, unsigned int *caps );
signed char  FSOUND_Init( int mixrate, int maxchannels, unsigned int flags );
void         FSOUND_Close( void );
void         FSOUND_Update( void );
int          FSOUND_GetMixer( void );
int          FSOUND_GetError( void );
void         FSOUND_SetSFXMasterVolume( int volume );
void         FSOUND_SetSpeakerMode( unsigned int speakermode );

/* samples */
struct FSOUND_SAMPLE *FSOUND_Sample_Load( int index, const char *name_or_data, unsigned int mode, int memlength );
void         FSOUND_Sample_Free( struct FSOUND_SAMPLE *sptr );
signed char  FSOUND_Sample_SetMode( struct FSOUND_SAMPLE *sptr, unsigned int mode );
signed char  FSOUND_Sample_SetDefaults( struct FSOUND_SAMPLE *sptr, int deffreq, int defvol, int defpan, int defpri );
signed char  FSOUND_Sample_SetMinMaxDistance( struct FSOUND_SAMPLE *sptr, float fmin, float fmax );
unsigned int FSOUND_Sample_GetLength( struct FSOUND_SAMPLE *sptr );

/* channels */
int          FSOUND_PlaySound( int channel, struct FSOUND_SAMPLE *sptr );
int          FSOUND_PlaySoundEx( int channel, struct FSOUND_SAMPLE *sptr, void *dsp, signed char startpaused );
signed char  FSOUND_StopSound( int channel );
signed char  FSOUND_SetVolume( int channel, int vol );
signed char  FSOUND_SetVolumeAbsolute( int channel, int vol );
int          FSOUND_GetVolume( int channel );
signed char  FSOUND_SetPan( int channel, int pan );
signed char  FSOUND_SetPaused( int channel, signed char paused );
signed char  FSOUND_IsPlaying( int channel );
unsigned int FSOUND_GetLoopMode( int channel );
unsigned int FSOUND_GetCurrentPosition( int channel );
int          FSOUND_GetPriority( int channel );

/* 3D */
signed char  FSOUND_3D_SetAttributes( int channel, float *pos, float *vel );
void         FSOUND_3D_Listener_SetAttributes( float *pos, float *vel, float fx, float fy, float fz, float tx, float ty, float tz );

/* streams */
struct FSOUND_STREAM *FSOUND_Stream_OpenFile( const char *filename, unsigned int mode, int memlength );
signed char  FSOUND_Stream_Close( struct FSOUND_STREAM *stream );
int          FSOUND_Stream_Play( int channel, struct FSOUND_STREAM *stream );
signed char  FSOUND_Stream_Stop( struct FSOUND_STREAM *stream );
signed char  FSOUND_Stream_SetTime( struct FSOUND_STREAM *stream, int ms );
int          FSOUND_Stream_GetTime( struct FSOUND_STREAM *stream );
signed char  FSOUND_Stream_SetSynchCallback( struct FSOUND_STREAM *stream, FSOUND_STREAMCALLBACK callback, int userdata );

#ifdef __cplusplus
}
#endif
#endif

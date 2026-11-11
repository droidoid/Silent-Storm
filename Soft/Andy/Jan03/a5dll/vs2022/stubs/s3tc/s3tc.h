/* A5 VS2022 port - stand-in for the S3 texture-compression SDK header (not part of the repository).
 *
 * Written from scratch against the calls made by Image/ImagePack.cpp. NOT the vendor header; flag values
 * are private to this stub. The implementation (s3tc_stub.cpp) cannot compress anything: every call
 * reports failure - see A5_S3TC_STUB in ImagePack.cpp for how the failure is surfaced.
 * The includer provides DDSURFACEDESC (<ddraw.h>) and the surrounding extern "C" block.
 */
#ifndef A5_STUB_S3TC_H
#define A5_STUB_S3TC_H

#define A5_S3TC_STUB 1

/* encoding type flags (bit mask) */
#define S3TC_ENCODE_RGB_FULL            0x0001
#define S3TC_ENCODE_RGB_COLOR_KEY       0x0002
#define S3TC_ENCODE_RGB_ALPHA_COMPARE   0x0004
#define S3TC_ENCODE_ALPHA_EXPLICIT      0x0010
#define S3TC_ENCODE_ALPHA_INTERPOLATED  0x0020

struct _DDSURFACEDESC;

/* Stub: always returns 0 (no encoder available) and logs to stderr / the debugger. */
unsigned int S3TCgetEncodeSize( struct _DDSURFACEDESC *lpDesc, unsigned int dwEncodeType );
/* Stub: no effect. */
void S3TCsetAlphaReference( int nRef );
/* Stub: writes nothing to lpDest / lpDestBuf and logs; never produces compressed data. */
void S3TCencode( struct _DDSURFACEDESC *lpSrc, void *lpPalette, struct _DDSURFACEDESC *lpDest,
                 void *lpDestBuf, unsigned int dwEncodeType, float *lpfWeights );

#endif

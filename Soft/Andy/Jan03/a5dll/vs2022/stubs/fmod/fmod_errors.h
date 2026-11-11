/* A5 VS2022 port - stand-in for the FMOD 3 SDK error-string header. See fmod.h in this directory. */
#ifndef A5_STUB_FMOD_ERRORS_H
#define A5_STUB_FMOD_ERRORS_H

static const char *FMOD_ErrorString( int errcode )
{
	return errcode == 0 ? "No errors (null sound device)" : "Unknown error (null sound device)";
}

#endif

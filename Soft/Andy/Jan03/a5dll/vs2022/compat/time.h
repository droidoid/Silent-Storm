// VS2022 port: Main/Time.h (engine header) shadows the CRT <time.h> on case-insensitive lookups because
// the Main targets put their own directory on the include path. Angle-bracket lookups land here first
// (compat is searched BEFORE everything else) and are forwarded to the real UCRT header.
// Quoted includes of the engine's "Time.h" are resolved relative to the including file and never get here.
#include <../ucrt/time.h>

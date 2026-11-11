# Build log - VS2022 port of the Jan03 A5 sources

## Environment found

- Visual Studio Build Tools 2022 (17.14), MSVC 14.44, Windows SDK 10.0.22621, CMake + Ninja.
- No MFC/ATL component installed -> MapEdit (MFC + Stingray Objective Toolkit) and Sound (MFC test app)
  cannot be built on this machine.
- Windows SDK provides `d3d9.h`, `dinput.h`, `dsound.h`; it does not provide D3DX.

## Decisions

- Source snapshot: `Soft/Andy/Jan03/a5dll` - the only complete source tree in the repository
  (Oct02 is `Main` only, Monster is a VC6 prototype, later folders hold 3-4 files or binaries only).
- Build system: CMake (Ninja generator) in `vs2022/`, file lists parsed from the original `.vcproj`.
  Original project types are kept (Misc/MemoryMngr static, the rest DLL/EXE), configuration `ReleaseDll`.
- Architecture: x86 only (inline x86 assembly, 32-bit binary formats).
- DLL exports: the originals shipped tool-generated `.def` files with VC7/STLport manglings;
  replaced by CMake `WINDOWS_EXPORT_ALL_SYMBOLS`.
- STL: STLport 4.5.2 replaced by the MSVC STL through thin shims in `vs2022/compat/`.
- Default calling convention: original Release used `/Gr` (`__fastcall`); the port uses the default
  `__cdecl` (no ABI consumer outside this build).
- Sources are compiled as cp1251 (`/source-charset:windows-1251 /execution-charset:windows-1251`).
- Direct3D: the code already targets Direct3D 9 (not 8.1 as the README says); built against the
  Windows SDK headers. D3DX is not needed by the game itself.
- Memory manager: the original pool allocator reserves 512 MB at a fixed address; optional in the port
  (`-DA5_USE_MEMORY_MANAGER=ON`), default is the CRT heap.

## Behaviour changes

(append below: file/area - what changed - why)

- Misc/BasicFactory.h `CClassFactory::GetTypeID<TT>` - looks the class up by the decorated name of
  `typeid(TT*)` (x86 mangling, `.PAV<name>` vs `.?AV<name>`) instead of `typeid(TT)`; same result, different
  mechanism - `typeid` of a forward-declared class (CDBPtr<T> serialization in headers) no longer compiles.
  A type that is not registered still yields -1. Needs re-checking if the port ever goes x64 (`.PEAV`).
- Main/LSHead.cpp, LSController.cpp (LifeStudio:HEAD facial animation) - built against a from-scratch null
  stand-in, `vs2022/stubs/lifestudio/` (CMake option `A5_NULL_LIFESTUDIO`, default ON). Objects are created
  and destroyed normally but nothing is parsed or animated: `ISequencer::SequenceTime()` returns 0 (the caller
  treats the sequence as finished), `IAnimator::Process()` writes no vertices. The rest pose lives inside the
  vendor data blob, so a static head is not possible: `CHeadAnimator::Recalc` zero-fills the vertex buffer
  under `A5_NULL_LIFESTUDIO`, i.e. the face mesh collapses to a point (not visible); hair/cap models are
  still created. The callers (GView.cpp `CreateLSHead`, RWGame.cpp) have no failure path, so "creation fails"
  was not an option without changing them. Not run-tested.
- FModSound (FMOD 3 SDK missing) - built against a from-scratch null device, `vs2022/stubs/fmod/`
  (`fmod.h`, `fmod_errors.h`, `fmod_null.cpp`; CMake option `A5_NULL_SOUND`, default ON; OFF needs
  `A5_FMOD_INCLUDE_DIR` + `A5_FMOD_LIBRARY`). Init/load/play succeed and do nothing, one driver "No sound"
  with no caps, `FSOUND_IsPlaying` is always false (sounds and music streams are retired on the next
  `NFMSound::Update`), sample length/position 0, synch callbacks never fire. The game is silent. Not run-tested.
- Image/ImagePack.cpp, TexConv (s3tc SDK missing) - `vs2022/stubs/s3tc/` cannot encode (CMake option
  `A5_STUB_S3TC`, default ON; OFF needs `A5_S3TC_INCLUDE_DIR` + `A5_S3TC_LIBRARY`). With the stub
  `NImage::Pack()` returns 0 for DXT1..DXT5 and logs to stderr/debugger; TexConv then prints
  "Picture saving failed..." and exits with code 1 without writing a file. Uncompressed formats are unaffected.
- MemoryMngrDll - Trace.cpp/GetFuncName.cpp/NT4ProcessInfo.cpp/TLHELPProcessInfo.cpp are not built (they
  are ExcludedFromBuild in the original ReleaseDll configuration as well). MemoryMngr/DumbPow2Alloc.h uses
  the compiler's `<new>` instead of its own fake one.
- DataImport - the stale duplicate DataImport/BasicDB.cpp is not compiled; the tool uses ADOImport.dll.
- zlib/libpng - minigzip.c / pngtest.c (test programs, ExcludedFromBuild in the original) are not built.

## Journal

- Scaffold created; `Misc` builds. First full pass: 439 translation units, ~1700 unique errors,
  dominated by STLport-isms and pre-standard C++ (missing `typename`, default-int, deleted constructors).

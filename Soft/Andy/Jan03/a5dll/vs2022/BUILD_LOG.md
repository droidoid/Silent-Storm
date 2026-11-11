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

## Journal

- Scaffold created; `Misc` builds. First full pass: 439 translation units, ~1700 unique errors,
  dominated by STLport-isms and pre-standard C++ (missing `typename`, default-int, deleted constructors).

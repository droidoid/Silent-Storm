# Catalogue of Silent Storm derivatives (2026-11-11)

Sources: bare repo C:\a5b\forks\net.git; shallow clones here: remaster/ (develop 8dfdf0c, blob<=3MB),
mike_macos/ (macos-port 941bb24, sparse android/), s2blenderio/. lsh/ = LifeStudio header comparison.

## Verified facts
### silent-storm-remaster (mrartanis, develop 8dfdf0c)
- Merge base with recon: 667d2e1 (2026-09-17, early recon). 1429 own commits, no later merges.
  Recon fixes are re-implemented, not merged: diagnostics/upstream-import-status.tsv audits 300 recon
  commits: 258 imported, 32 already-present, 5 excluded.
- android, feature/macos-arm64, launcher-check/* branches are fully contained in develop.
- Diff vs 667d2e1 (excl. assets/diagnostics/data): 888 files, +62.5k/-11.5k lines; Main 586 files +28k/-8.8k.
- Renderer: bgfx only (cmake/Bgfx.cmake: "renders exclusively through the pinned bgfx backend");
  D3D11 on Windows, Vulkan Linux/Android, Metal macOS. Main/BgfxBackend.cpp 66KB, GfxLegacyTokens.h,
  diagnostics/CompileBgfxShaders.py converts the original GfxShaders.cpp tables (470 variants per REFACTOR.md).
- Platform: SDL3 3.4.16 exact (Game/Platform.cpp, Input/SDLInput.cpp; DirectInput removed).
- Media: Media/NativeFmodCompat.cpp (FMOD3 game subset on miniaudio+FFmpeg), BinkCompat.cpp (Bink API on FFmpeg),
  FFmpegDecoder, NativeMusicPlayer: ~1k lines total. x86 build still links original fmod.dll.
- Faces: third_party/lifestudio/src/Native*.cpp + lifestudio_x64_stub.cpp (~130KB) native x64 reimplementation;
  header says "full vertex parity remains incomplete"; FACE-PARITY.md reports bounded parity vs x86 DLL.
- Vector fonts: FreeType 2.14.3 (FetchContent) + Liberation fonts, Main/VectorFont*.cpp (~9KB).
- HD: engine side = res-hd layer + Textures-N.res packages in Main/GResource.cpp; art = AI-generated (imagegen
  prompts in assets/terrain-hd, 13k metadata files ~760MB), packs in LFS (data/hd-runtime, hd-sources).
- Launcher: Python/PySide6 + bundled innoextract 1.9 (GOG import) + 7-Zip 26.06.
- Network: Main/Network*.cpp + iNetworkMenu.cpp (~150KB), raw TCP port 7780; LLM agent = tools/agent_play.py +
  Game/AgentPlay.cpp + .agents/skills.
- Licence: Nival non-commercial offer (LICENSE.md).
### timskap android-port / Mikepicker macos-port
- Built on **Jan03** (Soft/Andy/Jan03/a5dll) via android/tools/prepare_sources.py (283KB, 288 rewrite rules:
  Main 215, Misc 31, FileIO 21...), recon merged as untouched subtree reconstruction/ (61b033f) for reference only.
- android/platform/db_retail.cpp (62KB): loads retail game.db (CDBTableDataStorage 0xA1843130) through Jan03
  record Import() -- 130/130 tables per README. Rule accepts .res signature 0x96948A22.
- compat/d3d9gles (d3d9gles.cpp 120KB + d3d9.h): D3D9 subset on GLES3; tools/d3dasm2glsl.py + extract_shaders.py
  (155 shaders). macOS: SDL2 + GL headers shim (compat/macos).
- 64-bit fixes documented: CBufferedStream unsigned offset, int-truncated ptr diff, ISAAC seeding walking C:\,
  CStructureSaver pointer-as-ID, Float2Int must round (lrintf).
- macos-port extras (Nov 10): tools/gltf_export.py, mmp2png.py, merge_parts.py, blender_anim.py (need engine
  binary to dump manifests, except mmp2png).
### sssr33 feature/vs2026_build (adae768)
- In-place edits to ~196 Jan03 files + 30 vcxproj; as of 2026-10-03 Main still fails (683 errors), Game not linked.
- Tests/Small (MatrixInitialization, PointerComparison), BUGS.md BUG-001 (SHMatrix::HomogeneousInverse always false).
- Soft/SDK/LifeStudioHead-2.5: 4 headers + x86 import libs from archive.org (Pathologic provenance unverified).
- LifeStudio check (this session, static read of bin/LifeStudioHeadAPI.dll, FileVersion 2.5.0.0):
  6 factory exports ?Create@...@@SG... ; IAnimator vtable methods use ECX as this (thiscall).
  SDK header declares virtuals LIFESTUDIOHEADAPICALL=__stdcall and IAnimator slot Flags() where recon has Clone().
  Recon commit 32a54a4 "Downgraded LS shim to be 2.5.0.0 compatible" removed the __stdcall macro.
### TanghaohanSC silent-storm-port (57b49ef): Jan03-based, x86-windows vcpkg (sdl3, fmt, flatbuffers, ffmpeg, rmlui),
  d3d9 facade over bgfx with 9 handwritten .sc shaders, RmlUi screens (data/ui/*.rml), flatbuffers save v1
  (not wired), CDBTableDataStorage stub, bik2webm.ps1, mod overrides. Paired with tang/main edits to Jan03 (~165 files).
### WormSlayer silent-storm-rtx: remix-comp-base ASI (dinput8 proxy) for retail Game.exe; extracts VP matrix from
  shader constants; limitations: per-frame dynamic terrain textures break Remix hashing, no lights.
### Others: Gunnarguy/Silent-Storm-Mac (recon-based, Metal, conveniences origin), met-nikita/S2BlenderIO (pure-Python
  formats incl. 56-byte vertex variant, byte-validated on ~13.9k files), maltsevda/FakeD3D9 (2019 d3d9 proxy,
  shadows/vsync/1080p hacks), cheaterBY fork (no code changes beyond upload).

## Inferences (not tested)
- SDK 2.5 import libs are link-compatible with bin DLL (factory names match); SDK headers are only usable with
  -DLIFESTUDIOHEADAPICALL= (empty) and must not call slot Flags/Clone; recon's headers are the safer choice.
- Remaster features other than media shims, fonts, launcher and HD pack are entangled with its x64/SDL3/bgfx core.

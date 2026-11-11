# Targets, in dependency order. Mirrors A5.sln (configuration ReleaseDll).
# Original project types are kept: Misc/MemoryMngr are static libs, the rest DLLs/EXEs.

# --- infrastructure -------------------------------------------------------
a5_target(Misc STATIC)
# The stack-trace / leak-report files are ExcludedFromBuild in the original ReleaseDll configuration.
a5_target(MemoryMngrDll SHARED DEFS MEMORYMNGRDLL_EXPORTS
	EXCLUDE GetFuncName.cpp NT4ProcessInfo.cpp TLHELPProcessInfo.cpp Trace.cpp)
a5_target(MemoryMngr STATIC LIBS MemoryMngrDll)

# Every original project links MemoryMngr (global operator new/delete -> pool allocator).
# In the port this is optional, see A5_USE_MEMORY_MANAGER.
set(A5_MEM)
if(A5_USE_MEMORY_MANAGER)
	set(A5_MEM MemoryMngr)
endif()

a5_target(FileIO SHARED LIBS Misc ${A5_MEM})
a5_target(MiscDll SHARED DEFS MISCDLL_EXPORTS LIBS Misc FileIO ${A5_MEM})
a5_target(zlib SHARED EXCLUDE minigzip.c LIBS ${A5_MEM}) # minigzip.c: ExcludedFromBuild in the original
# libpng is plain C and never sees Misc/Tools.h: the snapshot's "externA5" (extern -> externA5 global replace
# in png.h/pngconf.h) has to be given on the command line. Inside the DLL it is a plain "extern".
a5_target(libpng SHARED DEFS externA5=extern EXCLUDE pngtest.c LIBS zlib ${A5_MEM}) # pngtest.c: ExcludedFromBuild in the original
# s3tc (DXT encoder used by the texture tools) is a commercial SDK that is not in the repository.
option(A5_STUB_S3TC "Use the non-encoding s3tc stub (vs2022/stubs/s3tc) instead of a real s3tc SDK" ON)
set(A5_S3TC_INCLUDE_DIR "" CACHE PATH "Directory of the real s3tc.h (A5_STUB_S3TC=OFF)")
set(A5_S3TC_LIBRARY "" CACHE FILEPATH "Path of the real s3tc.lib (A5_STUB_S3TC=OFF)")
if(A5_STUB_S3TC)
	a5_target(Image SHARED EXTRA "${A5_STUBS}/s3tc/s3tc_stub.cpp" LIBS zlib libpng Misc FileIO ${A5_MEM})
	target_include_directories(Image PRIVATE "${A5_STUBS}/s3tc")
else()
	a5_target(Image SHARED LIBS zlib libpng Misc FileIO "${A5_S3TC_LIBRARY}" ${A5_MEM})
	target_include_directories(Image PRIVATE "${A5_S3TC_INCLUDE_DIR}")
endif()
a5_target(Input SHARED LIBS Misc MiscDll FileIO dxguid dinput8 ${A5_MEM})
a5_target(Script SHARED LIBS Misc MiscDll FileIO Input ${A5_MEM})

# --- content database -----------------------------------------------------
# ADO type library for #import <msado15.dll> (the original hard-coded C:\Program Files\Common Files\System\ADO).
set(_pf86 "CommonProgramFiles(x86)")
find_path(A5_ADO_DIR msado15.dll
	PATHS "$ENV{${_pf86}}/System/ado" "$ENV{CommonProgramFiles}/System/ado" "$ENV{CommonProgramW6432}/System/ado"
	NO_DEFAULT_PATH DOC "Directory that contains msado15.dll")
if(NOT A5_ADO_DIR)
	message(FATAL_ERROR "msado15.dll (ADO) not found; set A5_ADO_DIR to the directory that contains it")
endif()
a5_target(ADOImport SHARED DEFS ADOIMPORT_EXPORTS LIBS Misc FileIO ${A5_MEM})
target_include_directories(ADOImport PRIVATE "${A5_ADO_DIR}")
a5_target(ADOFake SHARED DEFS ADOFAKE_EXPORTS LIBS Misc FileIO ${A5_MEM})
a5_target(DBFormat SHARED LIBS ADOImport Misc FileIO ${A5_MEM})

# --- sound ----------------------------------------------------------------
# FMOD 3 is a commercial SDK that is not in the repository. Default: silent null device (vs2022/stubs/fmod).
option(A5_NULL_SOUND "Build FModSound against the silent null-device stub instead of the FMOD 3 SDK" ON)
set(A5_FMOD_INCLUDE_DIR "" CACHE PATH "Directory of the real fmod.h / fmod_errors.h (A5_NULL_SOUND=OFF)")
set(A5_FMOD_LIBRARY "" CACHE FILEPATH "Path of the real fmodvc.lib (A5_NULL_SOUND=OFF)")
if(A5_NULL_SOUND)
	a5_target(FModSound SHARED EXTRA "${A5_STUBS}/fmod/fmod_null.cpp" LIBS Misc FileIO ${A5_MEM})
	target_include_directories(FModSound PRIVATE "${A5_STUBS}/fmod")
else()
	if(NOT A5_FMOD_INCLUDE_DIR OR NOT A5_FMOD_LIBRARY)
		message(FATAL_ERROR "A5_NULL_SOUND=OFF needs A5_FMOD_INCLUDE_DIR and A5_FMOD_LIBRARY")
	endif()
	a5_target(FModSound SHARED LIBS Misc FileIO "${A5_FMOD_LIBRARY}" ${A5_MEM})
	target_include_directories(FModSound PRIVATE "${A5_FMOD_INCLUDE_DIR}")
endif()

# --- engine + game --------------------------------------------------------
# "Main" is the object library (all Main sources); "MainDll" links them into Main.dll together with the
# generated export list (see below - the list is derived from the objects, hence the split).
a5_target(Main OBJECT DEFS _USRDLL)
set(A5_MAIN_LIBS Misc MiscDll FileIO Input Script ADOImport DBFormat FModSound dxguid d3d9 ${A5_MEM})
# Same split for the executable: "Game" = objects, "GameExe" = Game.exe.
a5_target(Game OBJECT)
add_executable(GameExe WIN32 $<TARGET_OBJECTS:Game>)
set_target_properties(GameExe PROPERTIES OUTPUT_NAME Game)
target_link_libraries(GameExe PRIVATE MainDll Misc MiscDll FileIO Input Script ADOImport DBFormat FModSound dxguid dinput8 d3d9 ${A5_MEM})
# Game.rc includes afxres.h (MFC) just for the standard resource ids; compat/rc/afxres.h forwards to winres.h.
target_include_directories(Game PRIVATE "${A5_COMPAT}/rc")

# Development aid: Main split into object-only groups so that parts can be compiled
# independently (cmake --build <dir> --target Main_part_G ...). Not part of the default build.
option(A5_DEV_PARTS "Add Main_part_* object targets (porting aid)" ON)
if(A5_DEV_PARTS)
	a5_sources(_main_src Main)
	set(_rx_G "/(G[A-Z0-9a-z]|Gfx|2D|SW[A-Z]|Camera|RW|LS|Sound|ScreenShot|Transform|PolyUtils|RectPacker|FontFormat|MemObject|VolumeContainer|Time|DG\\.|Cache|DiscretePos|SuperCollider|StdAfx)[^/]*$")
	set(_rx_A "/(ai|AI|BSP|ph[A-Z]|Building|rod|Rod|MakeBuilding|MapBuild|TerrainInfo|BSchema)[^/]*$")
	set(_rx_W "/(w[A-Z]|RPG|rpg|InventoryUnit|script|Script|sc[A-Z]|A5Script)[^/]*$")
	set(_rest ${_main_src})
	foreach(part G A W)
		set(_p ${_rest})
		list(FILTER _p INCLUDE REGEX "${_rx_${part}}")
		list(FILTER _p EXCLUDE REGEX "/GlobalInfo")
		list(FILTER _rest EXCLUDE REGEX "${_rx_${part}}")
		if(part STREQUAL "G")
			list(FILTER _main_src INCLUDE REGEX "/GlobalInfo")
			list(APPEND _rest ${_main_src})
			list(REMOVE_DUPLICATES _rest)
		endif()
		list(LENGTH _p _n)
		message(STATUS "Main_part_${part}: ${_n} files")
		add_library(Main_part_${part} OBJECT EXCLUDE_FROM_ALL ${_p})
		target_include_directories(Main_part_${part} PRIVATE "${A5_ROOT}/Main")
		target_compile_definitions(Main_part_${part} PRIVATE _USRDLL)
	endforeach()
	list(LENGTH _rest _n)
	message(STATUS "Main_part_I: ${_n} files")
	add_library(Main_part_I OBJECT EXCLUDE_FROM_ALL ${_rest})
	target_include_directories(Main_part_I PRIVATE "${A5_ROOT}/Main")
	target_compile_definitions(Main_part_I PRIVATE _USRDLL)
endif()

# LifeStudio:HEAD facial animation SDK is not available: null stand-in (heads are not animated).
option(A5_NULL_LIFESTUDIO "Build Main against the null LifeStudio stub in stubs/lifestudio" ON)
if(A5_NULL_LIFESTUDIO)
	target_sources(Main PRIVATE "${A5_STUBS}/lifestudio/LifeStudioNull.cpp")
	foreach(_t Main Main_part_G)
		if(TARGET ${_t})
			target_include_directories(${_t} PRIVATE "${A5_STUBS}/lifestudio")
			target_compile_definitions(${_t} PRIVATE A5_NULL_LIFESTUDIO)
		endif()
	endforeach()
endif()

# Main has more than 65535 external symbols, so "export everything" cannot link (LNK1189).
# The export list is generated instead: what Main defines and its consumers (Game) reference.
find_package(Python3 COMPONENTS Interpreter REQUIRED)
add_library(MainDll SHARED $<TARGET_OBJECTS:Main> "${CMAKE_BINARY_DIR}/Main.def")
set_target_properties(MainDll PROPERTIES OUTPUT_NAME Main WINDOWS_EXPORT_ALL_SYMBOLS OFF)
target_link_libraries(MainDll PRIVATE ${A5_MAIN_LIBS})
file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/Main_objs.rsp" CONTENT "$<JOIN:$<TARGET_OBJECTS:Main>,\n>\n")
file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/Game_objs.rsp" CONTENT "$<JOIN:$<TARGET_OBJECTS:Game>,\n>\n")
add_custom_command(
	OUTPUT "${CMAKE_BINARY_DIR}/Main.def"
	COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/gen_def.py" --out "${CMAKE_BINARY_DIR}/Main.def" --name Main
		--provider "@${CMAKE_BINARY_DIR}/Main_objs.rsp" --consumer "@${CMAKE_BINARY_DIR}/Game_objs.rsp"
	DEPENDS $<TARGET_OBJECTS:Main> $<TARGET_OBJECTS:Game> "${CMAKE_CURRENT_SOURCE_DIR}/tools/gen_def.py"
	COMMAND_EXPAND_LISTS VERBATIM)

# --- tools ----------------------------------------------------------------
# DataImport/BasicDB.cpp is a stale copy of ADOImport/BasicDB.cpp (identical except that it includes the
# no longer existing ..\FileIO\BasicDB.h); the code lives in ADOImport.dll, which DataImport links.
a5_target(DataImport EXE EXCLUDE BasicDB.cpp LIBS DBFormat ADOImport Misc FileIO ${A5_MEM})
a5_target(PkgBuilder EXE LIBS Misc FileIO ${A5_MEM})
a5_target(TexMipStrip EXE LIBS Image Misc FileIO ${A5_MEM})
a5_target(TexConv EXE LIBS Image zlib libpng Misc FileIO ${A5_MEM})
a5_target(FontGen EXE LIBS Image zlib libpng Misc FileIO ${A5_MEM})

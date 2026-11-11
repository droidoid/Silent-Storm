# Targets, in dependency order. Mirrors A5.sln (configuration ReleaseDll).
# Original project types are kept: Misc/MemoryMngr are static libs, the rest DLLs/EXEs.

# --- infrastructure -------------------------------------------------------
a5_target(Misc STATIC)
a5_target(MemoryMngrDll SHARED DEFS MEMORYMNGRDLL_EXPORTS)
a5_target(MemoryMngr STATIC LIBS MemoryMngrDll)

# Every original project links MemoryMngr (global operator new/delete -> pool allocator).
# In the port this is optional, see A5_USE_MEMORY_MANAGER.
set(A5_MEM)
if(A5_USE_MEMORY_MANAGER)
	set(A5_MEM MemoryMngr)
endif()

a5_target(FileIO SHARED LIBS Misc ${A5_MEM})
a5_target(MiscDll SHARED DEFS MISCDLL_EXPORTS LIBS Misc FileIO ${A5_MEM})
a5_target(zlib SHARED LIBS ${A5_MEM})
a5_target(libpng SHARED LIBS zlib ${A5_MEM})
a5_target(Image SHARED LIBS zlib libpng Misc FileIO ${A5_MEM})
a5_target(Input SHARED LIBS Misc MiscDll FileIO dxguid dinput8 ${A5_MEM})
a5_target(Script SHARED LIBS Misc MiscDll FileIO Input ${A5_MEM})

# --- content database -----------------------------------------------------
a5_target(ADOImport SHARED DEFS ADOIMPORT_EXPORTS LIBS Misc FileIO ${A5_MEM})
a5_target(ADOFake SHARED DEFS ADOFAKE_EXPORTS LIBS Misc FileIO ${A5_MEM})
a5_target(DBFormat SHARED LIBS ADOImport Misc FileIO ${A5_MEM})

# --- sound ----------------------------------------------------------------
a5_target(FModSound SHARED LIBS Misc FileIO ${A5_MEM})

# --- engine + game --------------------------------------------------------
a5_target(Main SHARED LIBS Misc MiscDll FileIO Input Script ADOImport DBFormat FModSound dxguid d3d9 ${A5_MEM})
a5_target(Game WINEXE LIBS Main Misc MiscDll FileIO Input Script ADOImport DBFormat FModSound dxguid dinput8 d3d9 ${A5_MEM})

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

# --- tools ----------------------------------------------------------------
a5_target(DataImport EXE LIBS DBFormat ADOImport Misc FileIO ${A5_MEM})
a5_target(PkgBuilder EXE LIBS Misc FileIO ${A5_MEM})
a5_target(TexMipStrip EXE LIBS Image Misc FileIO ${A5_MEM})
a5_target(TexConv EXE LIBS Image zlib libpng Misc FileIO ${A5_MEM})
a5_target(FontGen EXE LIBS Image zlib libpng Misc FileIO ${A5_MEM})

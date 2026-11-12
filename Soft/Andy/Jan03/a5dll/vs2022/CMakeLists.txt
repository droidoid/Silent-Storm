# Silent Storm (A5 engine, Jan03 snapshot) - Visual Studio 2022 build.
# Source of truth for file lists are the original .vcproj files; they are parsed here.
cmake_minimum_required(VERSION 3.20)
project(A5 C CXX)

if(NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
	message(FATAL_ERROR "A5 builds only as 32-bit x86 with MSVC (inline x86 asm, 32-bit data formats). Run from an x86 VS developer prompt or use build.cmd.")
endif()

get_filename_component(A5_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
set(A5_COMPAT "${CMAKE_CURRENT_SOURCE_DIR}/compat")
set(A5_STUBS "${CMAKE_CURRENT_SOURCE_DIR}/stubs")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
# DLL exports: every DLL gets a .def file from vs2022/tools/gen_def.py (see a5_export_all below and MainDll in
# targets.cmake). CMake's own WINDOWS_EXPORT_ALL_SYMBOLS is not used: its list depends on the CMake version
# (3.29 wrote a broken line "=" -> LNK2001 when linking Input.dll).
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS OFF)
find_package(Python3 COMPONENTS Interpreter REQUIRED)
set(A5_GEN_DEF "${CMAKE_CURRENT_SOURCE_DIR}/tools/gen_def.py")

# a5_export_all(<target>) - export everything the target's own objects define (same rules as
# WINDOWS_EXPORT_ALL_SYMBOLS); the .def is written right before the link, like CMake does it.
function(a5_export_all tgt)
	set(def "${CMAKE_BINARY_DIR}/def/${tgt}.def")
	set(rsp "${CMAKE_BINARY_DIR}/def/${tgt}_objs.rsp")
	file(GENERATE OUTPUT "${rsp}" CONTENT "$<JOIN:$<TARGET_OBJECTS:${tgt}>,\n>\n")
	add_custom_command(TARGET ${tgt} PRE_LINK
		COMMAND "${Python3_EXECUTABLE}" "${A5_GEN_DEF}" --all --name ${tgt} --out "${def}" --provider "@${rsp}"
		VERBATIM)
	target_link_options(${tgt} PRIVATE "/DEF:${def}")
endfunction()

option(A5_USE_MEMORY_MANAGER "Link the original pool allocator (MemoryMngr) instead of the CRT heap" OFF)

# Sources are cp1251; keep string literals byte-identical to the original build.
add_compile_options(/source-charset:windows-1251 /execution-charset:windows-1251)
add_compile_options(/W1 /MP /bigobj /Zm300 /EHsc /GR /Zc:strictStrings-)
add_compile_definitions(WIN32 _WINDOWS _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_WARNINGS _SCL_SECURE_NO_WARNINGS
	_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS _SILENCE_STDEXT_HASH_DEPRECATION_WARNINGS _HAS_AUTO_PTR_ETC=1 _HAS_STD_BYTE=0
	A5_VS2022_PORT)
include_directories(BEFORE "${A5_COMPAT}")

# a5_sources(<out> <dir> [vcproj name]) - compiled files listed in the original project.
function(a5_sources out dir)
	set(proj "${dir}")
	if(ARGC GREATER 2)
		set(proj "${ARGV2}")
	endif()
	file(STRINGS "${A5_ROOT}/${dir}/${proj}.vcproj" lines REGEX "RelativePath=\"[^\"]+\.(cpp|c|cxx|rc|CPP|C)\"")
	set(result)
	foreach(line IN LISTS lines)
		string(REGEX REPLACE ".*RelativePath=\"([^\"]+)\".*" "\\1" rel "${line}")
		string(REPLACE "\\" "/" rel "${rel}")
		string(REGEX REPLACE "^\./" "" rel "${rel}")
		list(APPEND result "${A5_ROOT}/${dir}/${rel}")
	endforeach()
	set(${out} "${result}" PARENT_SCOPE)
endfunction()

# a5_target(<name> <STATIC|SHARED|EXE|WINEXE> [DIR dir] [EXCLUDE files...] [EXTRA files...] [LIBS libs...] [DEFS defs...])
function(a5_target name kind)
	cmake_parse_arguments(A "" "DIR" "EXCLUDE;EXTRA;LIBS;DEFS" ${ARGN})
	if(NOT A_DIR)
		set(A_DIR "${name}")
	endif()
	a5_sources(src "${A_DIR}" "${name}")
	foreach(x IN LISTS A_EXCLUDE)
		list(FILTER src EXCLUDE REGEX "/${x}$")
	endforeach()
	foreach(x IN LISTS A_EXTRA)
		list(APPEND src "${x}")
	endforeach()
	if(kind STREQUAL "EXE")
		add_executable(${name} ${src})
	elseif(kind STREQUAL "WINEXE")
		add_executable(${name} WIN32 ${src})
	else()
		add_library(${name} ${kind} ${src})
	endif()
	target_include_directories(${name} PRIVATE "${A5_ROOT}/${A_DIR}")
	if(kind STREQUAL "SHARED")
		target_compile_definitions(${name} PRIVATE _USRDLL)
		a5_export_all(${name})
	endif()
	if(A_DEFS)
		target_compile_definitions(${name} PRIVATE ${A_DEFS})
	endif()
	if(A_LIBS)
		target_link_libraries(${name} PRIVATE ${A_LIBS})
	endif()
endfunction()

include(targets.cmake)

cmake_minimum_required(VERSION 3.20)
project(ssproto_core LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# The engine-agnostic core. No dependencies, no RTTI, nothing throws on purpose: the same sources
# are meant to be compiled inside the Unreal module later.
add_library(ssproto_core STATIC
	src/Lattice.cpp
	src/Building.cpp
	src/TestBuilding.cpp
)
target_include_directories(ssproto_core PUBLIC include)

add_executable(ssproto_tests tests/Tests.cpp)
target_link_libraries(ssproto_tests PRIVATE ssproto_core)

add_executable(ssproto_bench bench/Bench.cpp)
target_link_libraries(ssproto_bench PRIVATE ssproto_core)

foreach(target ssproto_core ssproto_tests ssproto_bench)
	if(MSVC)
		target_compile_options(${target} PRIVATE /W4 /WX /permissive- /utf-8)
	else()
		target_compile_options(${target} PRIVATE -Wall -Wextra -Wshadow -Werror)
	endif()
endforeach()
if(MSVC)
	target_compile_options(ssproto_core PRIVATE /GR-)
else()
	target_compile_options(ssproto_core PRIVATE -fno-rtti)
endif()

enable_testing()
add_test(NAME ssproto_tests COMMAND ssproto_tests)

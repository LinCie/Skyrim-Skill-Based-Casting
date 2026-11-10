cmake_minimum_required(VERSION 3.25)

project(SkillBasedCasting VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(WIN32)
	set(_plugin_default ON)
else()
	set(_plugin_default OFF)
endif()
option(SBC_BUILD_PLUGIN "Cross-compile the Windows x64 SKSE plugin from Ubuntu" ${_plugin_default})
option(SBC_BUILD_TESTS "Build standalone progression and configuration tests" ON)

include(CTest)

if(SBC_BUILD_TESTS)
	add_executable(SkillBasedCastingTests
		tests/CastingProgressionTests.cpp
		src/CastingProgression.cpp
		src/Config.cpp
	)
	target_include_directories(SkillBasedCastingTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
	target_compile_features(SkillBasedCastingTests PRIVATE cxx_std_23)
	if(MSVC)
		target_compile_options(SkillBasedCastingTests PRIVATE /W4 /permissive-)
	else()
		target_compile_options(SkillBasedCastingTests PRIVATE -Wall -Wextra -Wpedantic)
	endif()
	add_test(NAME SkillBasedCasting.ProgressionAndConfig COMMAND SkillBasedCastingTests)
endif()

if(SBC_BUILD_PLUGIN)
	if(NOT CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
		message(FATAL_ERROR "The SKSE plugin must be cross-compiled on Ubuntu; configure with the ubuntu-debug or ubuntu-release preset.")
	endif()
	file(STRINGS "/etc/os-release" _sbc_os_release_id REGEX "^ID=ubuntu$")
	if(NOT _sbc_os_release_id)
		message(FATAL_ERROR "The SKSE plugin build is supported on Ubuntu only; configure on an Ubuntu host.")
	endif()
	if(NOT WIN32 OR NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR NOT CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
		message(FATAL_ERROR "SkillBasedCasting.dll requires clang-cl targeting the Windows x64 MSVC ABI via the Ubuntu cross-compile toolchain.")
	endif()
	if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
		message(FATAL_ERROR "SkillBasedCasting supports only the Windows x64 target.")
	endif()

	include(FetchContent)

	# CommonLibSSE-NG v16.0.1, pinned to the release commit and verified archive hash.
	FetchContent_Declare(CommonLibSSE
		URL https://codeload.github.com/alandtse/CommonLibSSE-NG/zip/dddc6ec670381ab6af883b32dcb67a2589878acd
		URL_HASH SHA256=799c05ac2e8a17f3be8f70fa823991d31cec2f797f5fdf7a230d050bd70dee1e
		DOWNLOAD_EXTRACT_TIMESTAMP TRUE
	)

	# Only the 1.7.x AE layout is needed. Keep upstream tests and optional hook decoders disabled.
	set(ENABLE_SKYRIM_SE OFF CACHE BOOL "" FORCE)
	set(ENABLE_SKYRIM_AE ON CACHE BOOL "" FORCE)
	set(ENABLE_SKYRIM_VR OFF CACHE BOOL "" FORCE)
	set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
	set(SKSE_SUPPORT_PATCH_SAFETY OFF CACHE BOOL "" FORCE)
	set(SKSE_SUPPORT_XBYAK OFF CACHE BOOL "" FORCE)
	set(COMMONLIB_ENABLE_IPO OFF CACHE BOOL "" FORCE)
	set(COMMONLIB_PREBUILT OFF CACHE BOOL "" FORCE)
	FetchContent_MakeAvailable(CommonLibSSE)
	include("${CommonLibSSE_SOURCE_DIR}/cmake/CommonLibSSE.cmake")

	add_commonlibsse_plugin(SkillBasedCasting
		NAME "SkillBasedCasting"
		AUTHOR "SkillBasedCasting contributors"
		EMAIL ""
		VERSION "${PROJECT_VERSION}"
		MINIMUM_SKSE_VERSION "2.3.1"
		USE_ADDRESS_LIBRARY
		STRUCT_DEPENDENT
		SOURCES
			src/main.cpp
			src/Config.cpp
			src/CastingProgression.cpp
			src/CastingHook.cpp
	)
	target_include_directories(SkillBasedCasting PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
	target_compile_features(SkillBasedCasting PRIVATE cxx_std_23)
	target_precompile_headers(SkillBasedCasting PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/PCH.h")
	target_compile_definitions(SkillBasedCasting PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
	if(CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
		target_compile_options(SkillBasedCasting PRIVATE /W4 /permissive- /Zc:preprocessor)
	endif()
	set_target_properties(SkillBasedCasting PROPERTIES
		OUTPUT_NAME "SkillBasedCasting"
		RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>"
		LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>"
	)
endif()
